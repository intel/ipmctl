#!/usr/bin/env bash

DIR="./src/os/patches"

# Apply patches
echo "Applying patches"
if [[ -d "$DIR" ]]; then
    for file in "$DIR"/*.patch
    do
        git apply --ignore-space-change --ignore-whitespace --whitespace=nowarn "${file}"
    done
fi
