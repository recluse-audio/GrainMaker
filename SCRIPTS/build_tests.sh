#!/bin/bash

pushd BUILD
cmake  ..
cmake --build . --target Tests
popd # back to top level

