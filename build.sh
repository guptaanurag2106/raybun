#!/usr/bin/env bash
set -e

#CFLAGS="-Wall -Wextra -ggdb -std=c11 -DDEBUG -O2 -fno-omit-frame-pointer -fno-inline"
CFLAGS="-Wall -Wextra -Wno-unused-variable -O3 -std=c11 -march=native -funroll-loops -flto -ffunction-sections -fdata-sections -ffast-math"

mkdir -p build

gcc $CFLAGS \
  -I./include -I./thirdparty/ \
  -I./thirdparty/libmicrohttpd-1.0.1/build/install/include \
  -I./thirdparty/cJSON-1.7.19 \
  -o build/raybun-unity ./src/unity.c \
  -lcurl -lpthread -lm \
  -L./thirdparty/libmicrohttpd-1.0.1/build/install/lib -lmicrohttpd \
  -L./thirdparty/cJSON-1.7.19/build -lcjson \
  -flto -Wl,--gc-sections \
  -Wl,-rpath,./thirdparty/libmicrohttpd-1.0.1/build/install/lib \
  -Wl,-rpath,./thirdparty/cJSON-1.7.19/build
