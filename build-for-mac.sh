#!/bin/sh

make build CC=clang BIN=drsb_arm_mac TARGET="-arch arm64"
make build CC=clang BIN=drsb_x86_mac TARGET="-arch x86_64"
