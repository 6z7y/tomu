CC           := cc
CFLAGS       := -Wall -g -O3 -Iinclude
LIBS         := -lm -lpthread -lavformat -lavcodec -lswresample -lavutil
INSTALL_PATH := /usr/bin
OBJECT_BUILD_DIR    := build

# names
TOMU_NAME    := tomu
TOMUCLI_NAME := tomucli
TOMUTUI_NAME := tomutui
TOMUGUI_NAME := tomugui

# dirs
TOMU_DIR    := src/tomu
TOMUCLI_DIR := src/tomucli
TOMUTUI_DIR := src/tomutui
TOMUGUI_DIR := src/tomugui
SHARED_DIR  := src/shared

# build dirs
TOMU_BUILD_DIR    := $(OBJECT_BUILD_DIR)/tomu
TOMUCLI_BUILD_DIR := $(OBJECT_BUILD_DIR)/tomucli
TOMUTUI_BUILD_DIR := $(OBJECT_BUILD_DIR)/tomutui
TOMUGUI_BUILD_DIR := $(OBJECT_BUILD_DIR)/tomugui
SHARED_BUILD_DIR  := $(OBJECT_BUILD_DIR)/shared

# sources
TOMU_SRC    := $(wildcard $(TOMU_DIR)/*.c)
TOMUCLI_SRC := $(wildcard $(TOMUCLI_DIR)/*.c)
TOMUTUI_SRC := $(wildcard $(TOMUTUI_DIR)/*.c)
TOMUGUI_SRC := $(wildcard $(TOMUGUI_DIR)/*.c)
SHARED_SRC  := $(wildcard $(SHARED_DIR)/*.c)

# objects
TOMU_OBJECTS    := $(patsubst $(TOMU_DIR)/%.c, $(TOMU_BUILD_DIR)/%.o, $(TOMU_SRC))
TOMUCLI_OBJECTS := $(patsubst $(TOMUCLI_DIR)/%.c, $(TOMUCLI_BUILD_DIR)/%.o, $(TOMUCLI_SRC))
TOMUTUI_OBJECTS := $(patsubst $(TOMUTUI_DIR)/%.c, $(TOMUTUI_BUILD_DIR)/%.o, $(TOMUTUI_SRC))
TOMUGUI_OBJECTS := $(patsubst $(TOMUGUI_DIR)/%.c, $(TOMUGUI_BUILD_DIR)/%.o, $(TOMUGUI_SRC))
SHARED_OBJECTS  := $(patsubst $(SHARED_DIR)/%.c, $(SHARED_BUILD_DIR)/%.o, $(SHARED_SRC))

# build all
all: $(TOMU_NAME) $(TOMUCLI_NAME) $(TOMUTUI_NAME) $(TOMUGUI_NAME)

# linking (IMPORTANT: include shared objects)
$(TOMU_NAME): $(TOMU_OBJECTS) $(SHARED_OBJECTS)
	$(CC) $^ -o $@ $(CFLAGS) $(LIBS)

$(TOMUCLI_NAME): $(TOMUCLI_OBJECTS) $(SHARED_OBJECTS)
	$(CC) $^ -o $@ $(CFLAGS) $(LIBS)

$(TOMUTUI_NAME): $(TOMUTUI_OBJECTS) $(SHARED_OBJECTS)
	$(CC) $^ -o $@ $(CFLAGS) $(LIBS)

$(TOMUGUI_NAME): $(TOMUGUI_OBJECTS) $(SHARED_OBJECTS)
	$(CC) $^ -o $@ $(CFLAGS) $(LIBS)


# compile rules (per folder)
$(TOMU_BUILD_DIR)/%.o: $(TOMU_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -c $< -o $@ $(CFLAGS)

$(TOMUCLI_BUILD_DIR)/%.o: $(TOMUCLI_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -c $< -o $@ $(CFLAGS)

$(TOMUTUI_BUILD_DIR)/%.o: $(TOMUTUI_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -c $< -o $@ $(CFLAGS)

$(TOMUGUI_BUILD_DIR)/%.o: $(TOMUGUI_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -c $< -o $@ $(CFLAGS)

$(SHARED_BUILD_DIR)/%.o: $(SHARED_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -c $< -o $@ $(CFLAGS)

# install
install: all
	sudo install -m755 $(TOMU_NAME) $(TOMUCLI_NAME) $(TOMUTUI_NAME) $(TOMUGUI_NAME) $(INSTALL_PATH)

# clean
clean:
	rm -rf $(OBJECT_BUILD_DIR) $(TOMU_NAME) $(TOMUCLI_NAME) $(TOMUGUI_NAME) $(TOMUTUI_NAME)

# uninstall
uninstall:
	sudo rm -f $(INSTALL_PATH)/$(TOMU_NAME)
	sudo rm -f $(INSTALL_PATH)/$(TOMUCLI_NAME)
	sudo rm -f $(INSTALL_PATH)/$(TOMUTUI_NAME)
	sudo rm -f $(INSTALL_PATH)/$(TOMUGUI_NAME)

.PHONY: all install clean uninstall
