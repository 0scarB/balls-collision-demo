#!/bin/sh
clang --target=wasm32 \
    -nostdlib -ffreestanding \
    -O3 \
    -ggdb \
    balls.c -o balls.wasm

