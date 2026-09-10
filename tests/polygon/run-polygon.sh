#!/bin/sh
# Run the polygon with the environment of the ctest tiers (see README.md):
# ./run-polygon.sh <command> ...
cd "$(dirname "$0")" || exit 1
exec python3 -m polygon "$@"
