CC = gcc

INCLUDES = -Iinclude -I./thirdparty/cJSON-1.7.19/ -I./thirdparty/
CFLAGS_DEBUG   = -Wall -Wextra -ggdb -std=c11 -DDEBUG -O0
CFLAGS_RELEASE = -Wall -Wextra -O3 -std=c11 -march=native -funroll-loops -flto -ffunction-sections -fdata-sections -ffast-math

LIBS = -L./thirdparty/cJSON-1.7.19/build -lcjson -lm
LD_FLAGS = -flto -Wl,--gc-sections -Wl,-rpath,./thirdparty/cJSON-1.7.19/build/

SRC_DIR   = src
BUILD_DIR = build

SRC = $(shell find $(SRC_DIR) -type f -name "*.c")

OBJ = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRC))

RAYBUN = $(BUILD_DIR)/raybun

all: release

$(RAYBUN): $(OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS) $(LD_FLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

debug:   CFLAGS = $(CFLAGS_DEBUG)
debug:   $(RAYBUN)

release: CFLAGS = $(CFLAGS_RELEASE)
release: $(RAYBUN)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean debug release

