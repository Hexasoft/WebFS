#!/bin/sh

BIN=webfs
SOURCE="webfs.c tree.c tools.c cache.c webget.c"

compil() {
  CMD="gcc -D_FILE_OFFSET_BITS=64 -O2 -Wall -o $BIN $SOURCE -lfuse -lcurl"
  
  echo "Exec: $CMD"
  $CMD
}

if [ "$1" = "" ]
then
  compil
  exit $?
fi


if [ "$1" = "run" ]
then
  compil
  if [ ! $? = 0 ]
  then
    echo "Error!" >&2
    exit 1
  fi
  fusermount -u ./Z
  # ./webfs -s -r -o direct_io "http://localhost" ./Z/
  ./webfs -s -r -o direct_io --metadata="/description.data" --url="http://localhost" ./Z/
fi

if [ "$1" = "stop" ]
then
  fusermount -u ./Z
fi

