# core option installer
CC := cc
CFLAGS := -Wall -g -O3 -Iinclude
LIBS := -lm -lpthread -lavformat -lavcodec -lswresample -lavutil

#Paths
INSTALL_PATH := /usr/bin
BUILD_DIR := build

# (Server)
SERVER_NAME := tomu
SERVER_DIR := src/server
SERVER_BUILD_DIR := $(BUILD_DIR)/server
SERVER_SOURCES := $(wildcard $(SERVER_DIR)/*.c)
SERVER_OBJECTS := $(patsubst $(SERVER_DIR)/%.c, $(SERVER_BUILD_DIR)/%.o, $(SERVER_SOURCES))

# # cli (CLIENT)
CLIENT_CLI_NAME := tomucli
CLIENT_CLI_DIR := src/clients/cli src/clients
CLIENT_CLI_BUILD_DIR := $(BUILD_DIR)/clients/cli
# CLIENT_CLI_SOURCES := $(wildcard $(CLIENT_CLI_DIR)/*.c)
# CLIENT_CLI_OBJECTS := $(patsubst $(CLIENT_CLI_DIR)/%.c, $(CLIENT_CLI_BUILD_DIR)/%.o, $(CLIENT_CLI_SOURCES))
CLIENT_CLI_DIRS := src/clients/cli src/clients
CLIENT_CLI_SOURCES := $(foreach dir, $(CLIENT_CLI_DIRS), $(wildcard $(dir)/*.c))
CLIENT_CLI_OBJECTS := $(foreach dir, $(CLIENT_CLI_DIRS), \
    $(patsubst $(dir)/%.c, $(CLIENT_CLI_BUILD_DIR)/%.o, $(wildcard $(dir)/*.c)))
#
# # tui (CLIENT)
# CLIENT_TUI_NAME := tomutui
# CLIENT_TUI_DIR := src/clients/tui
# CLIENT_TUI_BUILD_DIR := $(BUILD_DIR)/clients/tui
# CLIENT_TUI_SOURCES := $(wildcard $(CLIENT_TUI_DIR)/*.c)
# CLIENT_TUI_OBJECTS := $(patsubst $(CLIENT_TUI_DIR)/%.c, $(CLIENT_TUI_BUILD_DIR)/%.o, $(CLIENT_TUI_SOURCES))

# TODO: later
# CLIENT_GUI_SRC_PATH := src/clients/gui

all: $(SERVER_NAME) $(CLIENT_CLI_NAME) $(CLIENT_TUI_NAME)

$(SERVER_NAME): $(SERVER_OBJECTS)
	$(CC) $(SERVER_OBJECTS) -o $@ $(CFLAGS) $(LIBS)

$(CLIENT_CLI_NAME): $(CLIENT_CLI_OBJECTS) $(SERVER_OBJECTS)
	$(CC) $(CLIENT_CLI_OBJECTS) -o $@ $(CFLAGS) $(LIBS)
#
# $(CLIENT_TUI_NAME): $(CLIENT_TUI_OBJECTS)
# 	$(CC) $(CLIENT_TUI_OBJECTS) -o $@ $(CFLAGS) $(LIBS)


$(SERVER_BUILD_DIR)/%.o: $(SERVER_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -c $< -o $@ $(CFLAGS)

$(CLIENT_CLI_BUILD_DIR)/%.o: src/clients/cli/%.c
	@mkdir -p $(dir $@)
	$(CC) -c $< -o $@ $(CFLAGS)

$(CLIENT_CLI_BUILD_DIR)/%.o: src/clients/%.c
	@mkdir -p $(dir $@)
	$(CC) -c $< -o $@ $(CFLAGS)
#
# $(CLIENT_TUI_BUILD_DIR)/%.o: $(CLIENT_TUI_DIR)/%.c
# 	@mkdir -p $(dir $@)
# 	$(CC) -c $< -o $@ $(CFLAGS)

install: all
	sudo install -m755 $(SERVER_NAME) $(CLIENT_CLI_NAME) $(CLIENT_TUI_NAME) $(INSTALL_PATH)

clean:
	rm -rf $(SERVER_NAME) $(CLIENT_CLI_NAME) $(CLIENT_TUI_NAME) $(BUILD_DIR)

uninstall:
	sudo rm -f $(INSTALL_PATH)/$(SERVER_NAME)
	sudo rm -f $(INSTALL_PATH)/$(CLIENT_CLI_NAME)
	# sudo rm -f $(INSTALL_PATH)/$(CLIENT_TUI_NAME)

.PHONY: all install uninstall clean
