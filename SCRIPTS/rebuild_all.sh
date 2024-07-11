#!/bin/bash

rm -rf BUILD
mkdir BUILD
pushd BUILD
cmake ..
cmake --build . --target BLACK_PYRAMID
cmake --build . --target Tests
popd # back to top level
