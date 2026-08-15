#!/bin/sh
# Run the editor test suite (see docs/TESTING.md); extra args go to ctest,
# e.g. ./run-tests.sh -R '^l0-'
cd "$(dirname "$0")/build" || exit 1
exec ctest --output-on-failure "$@"
