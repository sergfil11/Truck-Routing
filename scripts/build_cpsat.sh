#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
cmake --preset cpsat "$@"
cmake --build --preset cpsat --parallel
