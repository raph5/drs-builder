#!/bin/sh

make build BIN=drsb_arm_mac TARGET="-arch arm64"
make build BIN=drsb_x86_mac TARGET="-arch x86_64"
