CC           := cc
# -O0 (for developer) / -O3 (for final binary)
CFLAGS       := -Wall -Wextra -g -O0 -Iinclude $(shell pkg-config --cflags dbus-1)
LIBS         := -lm -lcurl -lpthread -lavformat -lavcodec -lswresample -lavutil $(shell pkg-config --libs dbus-1)
OBJECT_BUILD_DIR    := build
INSTALL_PATH := /usr/local/bin

TOMU_NAME    := tomu
TOMU_DIR    := src/tomu
TOMU_BUILD_DIR    := $(OBJECT_BUILD_DIR)/tomu
TOMU_SRC    := $(wildcard $(TOMU_DIR)/*.c)
TOMU_OBJECTS    := $(patsubst $(TOMU_DIR)/%.c, $(TOMU_BUILD_DIR)/%.o, $(TOMU_SRC))

# build
all: $(TOMU_NAME)

$(TOMU_NAME): $(TOMU_OBJECTS)
	$(CC) $^ -o $@ $(CFLAGS) $(LIBS)

# compile rules (per folder)
$(TOMU_BUILD_DIR)/%.o: $(TOMU_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -c $< -o $@ $(CFLAGS)

# install
install: all
	mkdir -p $(HOME)/.config/tomu
	@echo "\033[0;33mrequesting root permission, for install tomu\033[0m"
	sudo install -m755 $(TOMU_NAME) $(INSTALL_PATH)
	@echo "\033[0;32mDone\033[0m"

clean:
	rm -rf $(OBJECT_BUILD_DIR) $(TOMU_NAME)

update:
	@echo "\033[0;32mFetching latest tomu source...\033[0m"
	git pull

uninstall:
	sudo rm -f /usr/local/bin/tomu
	@echo "\033[0;32mUninstalled successfully\033[0m"

.PHONY: all install clean uninstall
