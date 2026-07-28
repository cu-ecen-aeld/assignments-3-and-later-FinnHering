#!/bin/bash

if [ -z $1 ] || [ -z $2 ]; then
    echo "USAGE: writer.sh [writefile] [writestr]"
    exit 1
fi

writefile="$1"
writestr="$2"

mkdir -p $(dirname "$writefile")

echo "$writestr" > "$writefile"