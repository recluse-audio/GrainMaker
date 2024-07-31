#!/bin/bash

rm -rf BUILD
mkdir BUILD
pushd BUILD
cmake ..
cmake --build . --target ArtieTune
#cmake --build . --target Tests
popd # back to top level
