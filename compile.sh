#!/bin/sh

set -e

if [ "$#" -ne 1 ]; then
    echo "usage: $0 <filename>"
    exit 1
fi

FNAME=$(basename $1 .lisp)

make bootstrap
./bootstrap < $FNAME.lisp > $FNAME.c
make runtime.o
echo cc $FNAME.c runtime.o -o $FNAME
cc $FNAME.c runtime.o -o $FNAME
