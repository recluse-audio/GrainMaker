#!/bin/bash



pushd BUILD
#cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake ..
cmake --build . --target ArtieTune_VST3
popd # back to top level