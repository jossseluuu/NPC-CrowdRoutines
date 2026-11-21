#!/bin/sh
# Helper to build/test/run inside WSL from the repository root.
#   sh tools/wsl.sh build | test | serve [args] | bench [args]
set -e
cd "$(dirname "$0")/.."
case "$1" in
  build) cmake --preset wsl-release >/dev/null && cmake --build --preset wsl-release ;;
  test)  cmake --preset wsl-release >/dev/null && cmake --build --preset wsl-release && ctest --preset wsl-release ;;
  serve) shift; ./build-wsl/npc_crowd_routines serve "$@" ;;
  bench) shift; ./build-wsl/npc_crowd_routines benchmark "$@" ;;
  *) echo "usage: sh tools/wsl.sh build|test|serve|bench"; exit 2 ;;
esac
