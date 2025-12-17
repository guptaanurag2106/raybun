CC = gcc

SRC_DIR     = src
BUILD_DIR   = build
DATA_DIR    = data
THIRD_PARTY = thirdparty

RAYBUN = $(BUILD_DIR)/raybun

SRC = $(shell find $(SRC_DIR) -type f -name "*.c")
OBJ = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC))

INCLUDES = -Iinclude\
		   -I./thirdparty/cJSON-1.7.19/\
		   -I./thirdparty/

LIBS = \
	-L$(THIRD_PARTY)/cJSON-1.7.19/build -lcjson \
	-lm

LD_FLAGS = \
	-flto \
	-Wl,--gc-sections \
	-Wl,-rpath,$(THIRD_PARTY)/cJSON-1.7.19/build \

CFLAGS_DEBUG   = -Wall -Wextra -ggdb -std=c11 -DDEBUG -O0
CFLAGS_RELEASE = -Wall -Wextra -O3 -std=c11 -march=native -funroll-loops \
                 -flto -ffunction-sections -fdata-sections -ffast-math

CJSON_DIR   = $(THIRD_PARTY)/cJSON-1.7.19
CJSON_BUILD = $(CJSON_DIR)/build
CJSON_STAMP = $(CJSON_BUILD)/.built

all: release

$(RAYBUN): $(OBJ) | $(CJSON_STAMP)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS) $(LD_FLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

debug: CFLAGS = $(CFLAGS_DEBUG)
debug: $(RAYBUN)

release: CFLAGS = $(CFLAGS_RELEASE)
release: $(RAYBUN)

max_scene_gen:
	$(CC) -o $(DATA_DIR)/max_scene_gen $(DATA_DIR)/max_scene_gen.c -lm
	$(DATA_DIR)/max_scene_gen > $(DATA_DIR)/max_scene.json

$(CJSON_STAMP):
	@echo "Building cJSON (one-time)..."
	@mkdir -p $(CJSON_BUILD)
	@cd $(CJSON_BUILD) && cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 .. && make
	@touch $@

clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(CJSON_BUILD)

.PHONY: all clean debug release max_scene_gen
