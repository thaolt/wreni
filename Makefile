# Allow user to specify WREN_DIR, default to original path if not provided
WREN_DIR ?= $(PWD)/wren

# Check if WREN_DIR exists
ifeq ($(wildcard $(WREN_DIR)),)
$(error WREN directory "$(WREN_DIR)" does not exist. Please specify a valid WREN_DIR using: make WREN_DIR=/path/to/wren)
endif

BUILD_DIR := build

WREN_INC := $(WREN_DIR)/src/include
WREN_OPT_DIR := $(WREN_DIR)/src/optional
WREN_SRC_DIR := $(WREN_DIR)/src/vm
WREN_SRC_FILES := $(WREN_SRC_DIR)/*.c $(WREN_OPT_DIR)/*.c
WREN_OBJ_FILES := $(patsubst $(WREN_SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(wildcard $(WREN_SRC_DIR)/*.c)) \
                  $(patsubst $(WREN_OPT_DIR)/%.c,$(BUILD_DIR)/%.o,$(wildcard $(WREN_OPT_DIR)/*.c))

CFLAGS = -std=c99 -g -Wall
DEFINES = -DWREN_OPT_META

$(BUILD_DIR)/wreni: main.c $(BUILD_DIR)/libwren.a | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(DEFINES) main.c -o $(BUILD_DIR)/wreni -I$(WREN_INC) -I$(WREN_SRC_DIR) -I$(WREN_OPT_DIR) -L$(BUILD_DIR) -lwren -lm -lffi

$(BUILD_DIR)/%.o: $(WREN_SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(DEFINES) -I$(WREN_INC) -I$(WREN_SRC_DIR) -I$(WREN_OPT_DIR) -c $< -o $@

$(BUILD_DIR)/%.o: $(WREN_OPT_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(DEFINES) -I$(WREN_INC) -I$(WREN_SRC_DIR) -I$(WREN_OPT_DIR) -c $< -o $@

$(BUILD_DIR)/libwren.a: $(WREN_OBJ_FILES) | $(BUILD_DIR)
	ar rcs $(BUILD_DIR)/libwren.a $^ 

clean:
	rm -rf $(BUILD_DIR)

run: $(BUILD_DIR)/wreni libraylib.so main.wren
	./$(BUILD_DIR)/wreni main

libraylib.so: raylib-5.5_linux_amd64.tar.gz
	tar xzvf raylib-5.5_linux_amd64.tar.gz
	cp raylib-5.5_linux_amd64/lib/libraylib.so.5.5.0 ./libraylib.so

raylib-5.5_linux_amd64.tar.gz:
	wget -O raylib-5.5_linux_amd64.tar.gz https://github.com/raysan5/raylib/releases/download/5.5/raylib-5.5_linux_amd64.tar.gz

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)