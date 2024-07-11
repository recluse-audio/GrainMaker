#!/bin/bash

pushd build
cmake  ..
cmake --build . --target Tests
popd # back to top level

