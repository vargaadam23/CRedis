#!/bin/sh
# Build and run the unit tests.
# Usage: ./run_tests.sh          run all tests
#        ./run_tests.sh -f       show only failures (and summary)
cd "$(dirname "$0")"

cmake -B build -DCMAKE_BUILD_TYPE=Debug > /dev/null || exit 1
cmake --build build --target clrf_tests utils_tests || exit 1

status=0
for t in clrf_tests utils_tests; do
    echo "=== $t ==="
    if [ "$1" = "-f" ]; then
        ./build/$t 2>&1 | grep -E -A1 'FAIL|passed' | grep -v '^--$'
        ./build/$t > /dev/null 2>&1 || status=1
    else
        ./build/$t || status=1
    fi
done
exit $status
