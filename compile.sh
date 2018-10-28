#!/bin/sh
set -e
compile() {
	rm -f "$1"
	gcc "$1.c" -Wall -ffunction-sections -fdata-sections -Wl,--gc-sections -s -Os -o "$1"
	strip --strip-all --remove-section=.comment --remove-section=.note "$1"
	wc -c "$1"
}

compile minit
compile parser

