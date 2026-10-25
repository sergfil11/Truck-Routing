#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
cmake --preset gurobi "$@"
cmake --build --preset gurobi --parallel
