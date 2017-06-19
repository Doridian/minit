#!/bin/sh -e
gcc minit.c -Wall -O3 -o minit.new
strip minit.new
rm -f minit
mv minit.new minit
