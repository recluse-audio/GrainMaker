#!/bin/bash

rm -rf BUILD
mkdir BUILD
pushd BUILD
cmake ..
cmake --build . --target GrainMaker
#cmake --build . --target Tests
popd # back to top level
