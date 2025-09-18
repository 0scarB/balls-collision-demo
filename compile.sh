#!/bin/sh
clang --target=wasm32 \
    -nostdlib -ffreestanding \
    -O3 \
    -g \
    balls.c -o balls.wasm

