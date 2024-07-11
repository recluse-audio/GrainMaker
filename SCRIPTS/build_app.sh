#!/bin/bash



pushd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . --target BLACK_PYRAMID_Standalone
popd # back to top level