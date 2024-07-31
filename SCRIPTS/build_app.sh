#!/bin/bash



pushd BUILD
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . --target ArtieTune_Standalone
popd # back to top level