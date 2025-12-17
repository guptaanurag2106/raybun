CC = gcc

SRC_DIR     = src
BUILD_DIR   = build
DATA_DIR    = data
THIRD_PARTY = thirdparty

RAYBUN = $(BUILD_DIR)/raybun

SRC = $(shell find $(SRC_DIR) -type f -name "*.c")
OBJ = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC))

CJSON_DIR   = $(THIRD_PARTY)/cJSON-1.7.19
CJSON_BUILD = $(CJSON_DIR)/build
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
LD_RPATH += -Wl,-rpath,$(CJSON_BUILD)

CFLAGS_DEBUG   = -Wall -Wextra -ggdb -std=c11 -DDEBUG -O0

CFLAGS_RELEASE = -Wall -Wextra -O3 -std=c11 -march=native \
                 -funroll-loops -flto -ffunction-sections \
                 -fdata-sections -ffast-math

LD_FLAGS = -flto -Wl,--gc-sections $(LD_RPATH)

all: release

$(RAYBUN): $(OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS) $(LD_FLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(CJSON_STAMP) $(MHD_STAMP)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

debug: CFLAGS = $(CFLAGS_DEBUG)
debug: $(RAYBUN)

release: CFLAGS = $(CFLAGS_RELEASE)
release: $(RAYBUN)

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

max_scene_gen:
	$(CC) -o $(DATA_DIR)/max_scene_gen $(DATA_DIR)/max_scene_gen.c -lm
	$(DATA_DIR)/max_scene_gen > $(DATA_DIR)/max_scene.json

clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(CJSON_BUILD)
	rm -rf $(MHD_BUILD)

.PHONY: all clean debug release max_scene_gen

