#!/usr/bin/env python3
"""
tomu_discord.py — Discord Rich Presence client for Tomu music player
Reads JSON from /tmp/tomu.sock and pushes presence to Discord IPC directly.
No external dependencies required.

Usage:
    python3 tomu_discord.py
    python3 tomu_discord.py --socket /tmp/tomu.sock
    python3 tomu_discord.py --no-cover          # skip cover upload
"""

import json
import os
import re
import socket
import struct
import subprocess
import sys
import time
import argparse
from pathlib import Path
from typing import Optional

# ─────────────────────────────────────────────────────────────
# Config
# ─────────────────────────────────────────────────────────────
DISCORD_APP_ID  = "1503183287235903521"
SOCKET_PATH     = "/tmp/tomu.sock"
COVER_DIR       = "/tmp/tomu_cover_img"
UPLOAD_URL      = "https://femboy.beauty/api/upload"

COVER_FALLBACK  = "music"          # Discord asset key used when no cover art
POSITION_SYNC_INTERVAL = 5.0      # seconds between timestamp re-syncs
RECONNECT_DELAY = 3.0             # seconds before retry after disconnect

# Discord IPC opcodes
OP_HANDSHAKE = 0
OP_FRAME     = 1


# ─────────────────────────────────────────────────────────────
# Discord IPC
# ─────────────────────────────────────────────────────────────
class DiscordIPC:
    def __init__(self, app_id: str):
        self.app_id    = app_id
        self.sock: Optional[socket.socket] = None
        self.connected = False
        self._nonce    = 0

    # ── connection ────────────────────────────────────────────
    def connect(self) -> bool:
        runtime = os.environ.get("XDG_RUNTIME_DIR") or \
                  os.environ.get("TMPDIR")           or \
                  "/tmp"
        # Some distros (Flatpak Discord) nest the socket deeper
        search_dirs = [
            runtime,
            os.path.join(runtime, "app", "com.discordapp.Discord"),
            os.path.join(runtime, "snap.discord"),
        ]

        for d in search_dirs:
            for i in range(10):
                path = os.path.join(d, f"discord-ipc-{i}")
                if not os.path.exists(path):
                    continue
                try:
                    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                    s.settimeout(3.0)
                    s.connect(path)
                    self.sock = s
                    print(f"[discord] Connected → {path}")
                    return self._handshake()
                except OSError:
                    pass

        print("[discord] No IPC socket found — is Discord running?")
        return False

    def _handshake(self) -> bool:
        payload = json.dumps({"v": 1, "client_id": self.app_id})
        self._send_raw(OP_HANDSHAKE, payload)
        # Discord sends a READY frame back — give it up to 5 seconds
        resp = self._recv_raw(timeout=5.0)
        if resp:
            self.connected = True
            print("[discord] Handshake OK")
            return True
        # Discord sometimes sends the READY frame slightly late;
        # treat a timeout as a soft success and let set_activity confirm
        print("[discord] Handshake — no READY frame yet, assuming OK")
        self.connected = True
        return True

    # ── framing ───────────────────────────────────────────────
    def _send_raw(self, opcode: int, data: str):
        if not self.sock:
            return
        b = data.encode()
        header = struct.pack("<II", opcode, len(b))
        try:
            self.sock.sendall(header + b)
        except OSError as e:
            print(f"[discord] send error: {e}")
            self._mark_disconnected()

    def _recv_raw(self, timeout: float = 0.5) -> Optional[dict]:
        if not self.sock:
            return None
        try:
            self.sock.settimeout(timeout)
            hdr = self._recvn(8)
            if not hdr:
                return None
            _op, length = struct.unpack("<II", hdr)
            if length == 0:
                return None
            body = self._recvn(length)
            if not body:
                return None
            return json.loads(body.decode())
        except (socket.timeout, OSError, json.JSONDecodeError):
            return None

    def _recvn(self, n: int) -> Optional[bytes]:
        """Read exactly n bytes."""
        buf = b""
        while len(buf) < n:
            try:
                chunk = self.sock.recv(n - len(buf))
            except OSError:
                return None
            if not chunk:
                return None
            buf += chunk
        return buf

    # ── public API ────────────────────────────────────────────
    def set_activity(self, activity: dict) -> bool:
        if not self.connected:
            return False
        self._nonce += 1
        payload = {
            "cmd": "SET_ACTIVITY",
            "args": {"pid": os.getpid(), "activity": activity},
            "nonce": str(self._nonce),
        }
        self._send_raw(OP_FRAME, json.dumps(payload))
        resp = self._recv_raw(timeout=2.0)
        if resp:
            if resp.get("evt") == "ERROR" or "error" in resp:
                print(f"[discord] SET_ACTIVITY error: {resp}")
                return False
        return True

    def clear_activity(self):
        if not self.connected:
            return
        self._nonce += 1
        payload = {
            "cmd": "SET_ACTIVITY",
            "args": {"pid": os.getpid(), "activity": None},
            "nonce": str(self._nonce),
        }
        self._send_raw(OP_FRAME, json.dumps(payload))
        self._recv_raw(timeout=2.0)

    def close(self):
        if self.sock:
            try:
                self.sock.close()
            except OSError:
                pass
        self.sock = None
        self.connected = False

    def _mark_disconnected(self):
        self.connected = False


# ─────────────────────────────────────────────────────────────
# Cover art uploader
# ─────────────────────────────────────────────────────────────
class CoverUploader:
    def __init__(self, enabled: bool = True):
        self.enabled = enabled
        # Map local path → hosted URL so we only upload each file once
        self._cache: dict[str, str] = {}

    def get_local_path(self, filename: Optional[str] = None) -> Optional[str]:
        """
        If `filename` is given (from queue), look for a matching jpg in COVER_DIR.
        Otherwise fall back to the most-recently-modified image in COVER_DIR.
        """
        cover_dir = Path(COVER_DIR)
        if not cover_dir.is_dir():
            return None

        if filename:
            stem = Path(filename).stem
            candidate = cover_dir / f"{stem}.jpg"
            if candidate.exists():
                return str(candidate)

        # Fallback: newest image
        images = sorted(
            list(cover_dir.glob("*.jpg")) + list(cover_dir.glob("*.png")),
            key=lambda p: p.stat().st_mtime,
            reverse=True,
        )
        return str(images[0]) if images else None

    def upload(self, path: str) -> Optional[str]:
        if not self.enabled or not path or not os.path.exists(path):
            return None

        if path in self._cache:
            return self._cache[path]

        try:
            result = subprocess.run(
                ["curl", "-s", "-F", f"file=@{path}", UPLOAD_URL],
                capture_output=True, text=True, timeout=15,
            )
            out = result.stdout.strip()
            # Response may be JSON: {"link":"https://..."} or a bare URL
            url: Optional[str] = None
            if out.startswith("{"):
                try:
                    data = json.loads(out)
                    url = data.get("link") or data.get("url") or data.get("file")
                except json.JSONDecodeError:
                    pass
            elif out.startswith("http"):
                url = out

            if url and url.startswith("http"):
                self._cache[path] = url
                print(f"[cover] Uploaded → {url}")
                return url
            else:
                print(f"[cover] Upload returned unexpected response: {out[:80]}")
        except subprocess.TimeoutExpired:
            print("[cover] Upload timed out")
        except FileNotFoundError:
            print("[cover] curl not found — install curl to enable cover art")
        except Exception as e:
            print(f"[cover] Error: {e}")

        return None


# ─────────────────────────────────────────────────────────────
# Tomu socket reader
# ─────────────────────────────────────────────────────────────
class TomuSocket:
    """
    Connects to Tomu's UNIX socket.
    Tomu sends newline-terminated or null-terminated JSON blobs.
    We handle both.
    """
    def __init__(self, path: str):
        self.path = path
        self.sock: Optional[socket.socket] = None
        self._buf = b""

    def connect(self) -> bool:
        try:
            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s.connect(self.path)
            # Tomu's accept_new_client() immediately reads sizeof(ClientType)
            # (an int/enum = 4 bytes) to identify client type.
            # Send 0 (STATUS_CLIENT or first enum value) so it doesn't block.
            s.sendall(struct.pack("<I", 0))
            s.setblocking(False)
            self.sock = s
            print(f"[socket] Connected → {self.path}")
            return True
        except OSError as e:
            print(f"[socket] Cannot connect to {self.path}: {e}")
            return False

    def read_messages(self) -> list[dict]:
        """Return all complete JSON messages received since last call.

        Tomu terminates every broadcast with a null byte (\x00).
        The JSON itself contains literal newlines (pretty-printed in the
        format string), so we must NOT split on \n.
        """
        if not self.sock:
            return []

        # Drain everything available right now (non-blocking)
        try:
            while True:
                chunk = self.sock.recv(4096)
                if not chunk:
                    raise ConnectionResetError("socket closed by server")
                self._buf += chunk
        except BlockingIOError:
            pass  # no more data right now — normal

        # Split completed messages on the null terminator
        messages = []
        while b"\x00" in self._buf:
            idx = self._buf.index(b"\x00")
            raw = self._buf[:idx]
            self._buf = self._buf[idx + 1:]
            raw = raw.strip()
            if not raw:
                continue
            try:
                messages.append(json.loads(raw.decode("utf-8")))
            except (json.JSONDecodeError, UnicodeDecodeError) as e:
                print(f"[socket] Parse error: {e} | raw: {raw[:120]}")

        return messages

    def close(self):
        if self.sock:
            try:
                self.sock.close()
            except OSError:
                pass
        self.sock = None
        self._buf = b""


# ─────────────────────────────────────────────────────────────
# State
# ─────────────────────────────────────────────────────────────
class PlayerState:
    def __init__(self):
        # Status fields
        self.duration:         int   = 0
        self.position:         int   = 0
        self.paused:           bool  = False
        self.playback_running: bool  = False
        self.volume:           float = 100.0
        self.speed:            float = 1.0
        self.shuffle:          bool  = False
        self.loop:             bool  = False
        # Metadata
        self.title:        str = ""
        self.artist:       str = ""
        self.album:        str = ""
        self.album_artist: str = ""
        self.genre:        str = ""
        self.date:         str = ""
        self.track:        str = ""
        # Queue (if Tomu sends it)
        self.queue_list:  list[str] = []
        self.queue_index: int       = -1

    def update(self, msg: dict):
        if "status" in msg:
            s = msg["status"]
            self.duration         = int(s.get("duration",         self.duration))
            self.position         = int(s.get("position",         self.position))
            self.paused           = bool(s.get("paused",          self.paused))
            self.playback_running = bool(s.get("playback_running",self.playback_running))
            self.volume           = float(s.get("volume",         self.volume))
            self.speed            = float(s.get("speed",          self.speed))
            self.shuffle          = bool(s.get("shuffle",         self.shuffle))
            self.loop             = bool(s.get("loop",            self.loop))

        if "metadata" in msg:
            m = msg["metadata"]
            self.title        = m.get("title",        self.title)
            self.artist       = m.get("artist",       self.artist)
            self.album        = m.get("album",        self.album)
            self.album_artist = m.get("album_artist", self.album_artist)
            self.genre        = m.get("genre",        self.genre)
            self.date         = m.get("date",         self.date)
            self.track        = m.get("track",        self.track)

        if "queue" in msg:
            q = msg["queue"]
            self.queue_list  = q.get("list",  self.queue_list)
            self.queue_index = int(q.get("index", self.queue_index))

    @property
    def current_file(self) -> Optional[str]:
        if 0 <= self.queue_index < len(self.queue_list):
            return self.queue_list[self.queue_index]
        return None

    @property
    def song_key(self) -> str:
        return f"{self.title}|{self.artist}"


# ─────────────────────────────────────────────────────────────
# Main client
# ─────────────────────────────────────────────────────────────
class TomuDiscordClient:
    def __init__(self, socket_path: str, no_cover: bool = False):
        self.socket_path  = socket_path
        self.state        = PlayerState()
        self.discord      = DiscordIPC(DISCORD_APP_ID)
        self.uploader     = CoverUploader(enabled=not no_cover)
        self.tomu         = TomuSocket(socket_path)

        self._last_song_key   = ""
        self._last_sync_time  = 0.0
        self._cover_url       = COVER_FALLBACK

    # ── connect / reconnect ───────────────────────────────────
    def connect_all(self) -> bool:
        ok_sock    = self.tomu.connect()
        ok_discord = self.discord.connect()
        if not ok_discord:
            print("[discord] Continuing without Discord RPC")
        return ok_sock   # must have the player socket

    # ── presence logic ────────────────────────────────────────
    def _refresh_cover(self):
        path = self.uploader.get_local_path(self.state.current_file)
        if not path:
            self._cover_url = COVER_FALLBACK
            return
        url = self.uploader.upload(path)
        self._cover_url = url if url else COVER_FALLBACK

    def _push_presence(self, force: bool = False):
        if not self.discord.connected:
            return

        # Not playing → clear
        if not self.state.playback_running:
            if self._last_song_key:
                self.discord.clear_activity()
                self._last_song_key = ""
                print("[discord] Cleared (playback stopped)")
            return

        now = time.time()
        song_changed   = self.state.song_key != self._last_song_key
        time_to_resync = (now - self._last_sync_time) >= POSITION_SYNC_INTERVAL

        if not force and not song_changed and not time_to_resync:
            return

        # New song → fetch fresh cover
        if song_changed:
            self._refresh_cover()
            self._last_song_key = self.state.song_key

        self._last_sync_time = now

        # ── build activity dict ──────────────────────────────
        title  = self.state.title  or "Unknown"
        artist = self.state.artist or "Unknown"
        album  = self.state.album

        details = title
        state   = f"{artist} • {album}" if album else artist

        activity: dict = {
            "type": 2,          # "Listening to"
            "details": details,
            "state":   state,
        }

        # Timestamps (only when playing, not paused)
        if self.state.duration > 0 and not self.state.paused:
            ts_now   = int(now)
            ts_start = ts_now - self.state.position
            ts_end   = ts_start + self.state.duration
            activity["timestamps"] = {"start": ts_start, "end": ts_end}

        # Assets
        large_text = f"♪ {album} ♪" if album else "♪ Now Playing ♪"
        if self.state.paused:
            large_text = "⏸ Paused"

        activity["assets"] = {
            "large_image": self._cover_url,
            "large_text":  large_text,
        }

        if self.discord.set_activity(activity):
            status = "⏸" if self.state.paused else "▶"
            print(f"[discord] {status} {title} — {artist}")

    # ── main loop ─────────────────────────────────────────────
    def run(self):
        print("\n" + "=" * 52)
        print("  🎵  Tomu Discord Presence Client")
        print("=" * 52)
        print(f"  Socket   : {self.socket_path}")
        print(f"  App ID   : {DISCORD_APP_ID}")
        print(f"  Cover dir: {COVER_DIR}")
        print(f"  Covers   : {'disabled' if not self.uploader.enabled else UPLOAD_URL}")
        print("  Ctrl-C to quit")
        print("=" * 52 + "\n")

        os.makedirs(COVER_DIR, exist_ok=True)

        while True:
            if not self.connect_all():
                print(f"[main] Retrying in {RECONNECT_DELAY}s …")
                time.sleep(RECONNECT_DELAY)
                continue

            try:
                self._loop()
            except ConnectionResetError:
                print("[socket] Disconnected — reconnecting …")
                self.tomu.close()
                time.sleep(RECONNECT_DELAY)
            except KeyboardInterrupt:
                break

        print("\n[main] Shutting down …")
        self._cleanup()

    def _loop(self):
        _discord_retry_at = 0.0

        while True:
            msgs = self.tomu.read_messages()
            changed = False
            for msg in msgs:
                self.state.update(msg)
                changed = True
                print(f"[tomu] {msg}")   # debug

            if changed:
                self._push_presence(force=True)
            else:
                self._push_presence()

            # Reconnect Discord only if dropped, with cooldown
            if not self.discord.connected:
                now = time.time()
                if now >= _discord_retry_at:
                    print("[discord] Attempting reconnect ...")
                    self.discord.close()
                    self.discord = DiscordIPC(DISCORD_APP_ID)
                    if self.discord.connect():
                        self._push_presence(force=True)
                    else:
                        _discord_retry_at = now + RECONNECT_DELAY

            time.sleep(0.1)

    def _cleanup(self):
        self.discord.clear_activity()
        self.discord.close()
        self.tomu.close()
        print("[main] Done.")


# ─────────────────────────────────────────────────────────────
# Entry point
# ─────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description="Tomu Discord Rich Presence")
    parser.add_argument(
        "--socket", default=SOCKET_PATH,
        help=f"Path to Tomu UNIX socket (default: {SOCKET_PATH})"
    )
    parser.add_argument(
        "--no-cover", action="store_true",
        help="Disable cover art upload"
    )
    args = parser.parse_args()

    client = TomuDiscordClient(socket_path=args.socket, no_cover=args.no_cover)
    client.run()


if __name__ == "__main__":
    main()
