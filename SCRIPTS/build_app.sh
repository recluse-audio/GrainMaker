#!/bin/bash



pushd BUILD
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . --target GrainMaker_Standalone
popd # back to top level