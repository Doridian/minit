#!/bin/sh
set -e
compile() {
	gcc "$1.c" -Wall -Os -o "$1.new"
	strip "$1.new"
	rm -f "$1"
	mv "$1.new" "$1"
}

compile minit
compile parser

