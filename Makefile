CC = gcc

SRC_DIR     = src
BUILD_DIR   = build
DATA_DIR    = data
THIRD_PARTY = thirdparty

RAYBUN = $(BUILD_DIR)/raybun

SRC = $(shell find $(SRC_DIR) -type f -name "*.c" ! -name "unity.c")
OBJ = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC))

CJSON_DIR   = $(THIRD_PARTY)/cJSON-1.7.19
CJSON_BUILD = $(CJSON_DIR)/build
CJSON_PREFIX  = $(abspath $(CJSON_BUILD))
CJSON_STAMP = $(CJSON_BUILD)/.built

MHD_DIR     = $(THIRD_PARTY)/libmicrohttpd-1.0.1
MHD_BUILD   = $(MHD_DIR)/build
MHD_PREFIX  = $(abspath $(MHD_BUILD)/install)
MHD_STAMP   = $(MHD_BUILD)/.built

INCLUDES = -Iinclude -Ithirdparty/
LIBS     = -lcurl -lpthread -lm


INCLUDES += -I$(MHD_PREFIX)/include
LIBS     += -L$(MHD_PREFIX)/lib -lmicrohttpd
LD_RPATH += -Wl,-rpath,$(MHD_PREFIX)/lib

INCLUDES += -I$(CJSON_DIR)
LIBS     += -L$(CJSON_BUILD) -lcjson
LD_RPATH += -Wl,-rpath,$(CJSON_PREFIX)

CFLAGS_DEBUG   = -Wall -Wextra -ggdb -std=gnu11 -DDEBUG -O2 -fno-omit-frame-pointer -fno-inline 

CFLAGS_RELEASE = -Wall -Wextra -Wno-unused-variable -O3 -std=gnu11 -march=native \
                 -funroll-loops -ffunction-sections \
                 -fdata-sections -ffast-math -DDEBUG

LD_FLAGS = -flto -Wl,--gc-sections $(LD_RPATH)

all: release

$(RAYBUN): $(OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS) $(LD_FLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(CJSON_STAMP) $(MHD_STAMP)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

debug: CFLAGS = $(CFLAGS_DEBUG)
debug: embed_bench $(RAYBUN)

release: CFLAGS = $(CFLAGS_RELEASE)
release: embed_bench $(RAYBUN)

$(CJSON_STAMP):
	@echo "Building cJSON..."
	@mkdir -p $(CJSON_BUILD)
	@cd $(CJSON_BUILD) && cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DENABLE_CJSON_TEST=Off .. && make
	@touch $@

$(MHD_STAMP):
	@echo "Building libmicrohttpd..."
	@mkdir -p $(MHD_BUILD)
	@cd $(MHD_BUILD) && \
		../configure --prefix=$(MHD_PREFIX) --disable-doc --disable-examples && \
		make
	@cd $(MHD_BUILD) && make install
	@touch $@

max_scene_gen: $(DATA_DIR)/max_scene_gen.c $(DATA_DIR)/max_scene.json
	$(CC) -o $(BUILD_DIR)/max_scene_gen $(DATA_DIR)/max_scene_gen.c -lm
	$(BUILD_DIR)/max_scene_gen > $(DATA_DIR)/max_scene.json

$(BUILD_DIR)/embed_json: $(DATA_DIR)/embed_json.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(INCLUDES) -o $@ $<

include/benchmark.h: $(BUILD_DIR)/embed_json ./data/benchmark.json
	$(BUILD_DIR)/embed_json ./data/benchmark.json > $@

embed_bench: include/benchmark.h

clean:
	rm -rf $(BUILD_DIR)
	# rm -rf $(CJSON_BUILD)
	# rm -rf $(MHD_BUILD)

.PHONY: all clean debug release max_scene_gen embed_bench

