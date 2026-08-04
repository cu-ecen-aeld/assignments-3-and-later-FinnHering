#!/bin/sh

if [ ! -d "$1" ] || [ -z "$2" ]; then
    echo "USAGE: finder.sh [filesdir] [searchstring]"
    exit 1
fi

filesdir="$1"
searchstr="$2"

file_count=$(find "$filesdir" -mindepth 1 | wc -l) # Use mindepth=1 since we dont want to count $filesdir 
lines_count=$(grep -r "$searchstr" "$filesdir" | wc -l)

echo "The number of files are $file_count and the number of matching lines are $lines_count"