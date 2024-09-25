#!/bin/bash

pushd BUILD
cmake  -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . --target Tests
popd # back to top level

