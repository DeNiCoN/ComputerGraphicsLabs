#!/usr/bin/env bash

cd "$(dirname "$0")" || exit
cmake -B Release -S .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Release "$@"
cp Debug/compile_commands.json ..
