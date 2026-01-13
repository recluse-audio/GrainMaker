# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

GrainMaker is a JUCE-based audio plugin implementing pitch detection (YIN algorithm) and pitch shifting using granular synthesis techniques (TD-PSOLA). The project builds as VST3, AU, AUv3, and Standalone formats.

## Build Commands

### Building the Plugin
```bash
# Build VST3 plugin (Release mode)
./SCRIPTS/build_vst3.sh

# Build Standalone application
./SCRIPTS/build_app.sh

# Full rebuild (clean and build all targets)
./SCRIPTS/rebuild_all.sh

# Manual CMake build from BUILD directory
cd BUILD
cmake ..
cmake --build . --target GrainMaker_VST3
```

### Testing
```bash
# Build and run all tests
./SCRIPTS/build_tests.sh
./BUILD/Tests

# Run GrainShifter tests specifically (primary development focus)
./BUILD/Tests "[GrainShifter]"

# Run specific test suite
./BUILD/Tests "[PitchDetector]"

# Run with verbose output
./BUILD/Tests --success
```

### Version Management
The project uses automated version management. Version is stored in `VERSION.txt` and automatically propagated to `SOURCE/Util/Version.h` during build via `SCRIPTS/update_version.py`.

## Architecture

### Core Signal Processing Pipeline

1. **PitchDetector** (`SOURCE/PITCH/PitchDetector.h`): Implements YIN algorithm for fundamental frequency detection
   - Processes audio blocks to detect pitch in Hz
   - Returns period in samples for grain-based processing
   - Uses difference function and cumulative mean normalized difference

2. **GrainShifter** (`SOURCE/GRAIN/GrainShifter.h`): **PRIMARY DEVELOPMENT FOCUS** - Performs pitch shifting using granular synthesis
   - Handles grain windowing and overlap-add synthesis
   - Manages spillover between audio blocks for continuous output
   - Uses lookahead buffer for proper grain extraction
   - Key challenge: Managing grain boundaries across processBlock calls
   - Test coverage in `TESTS/test_GrainShifter.cpp` with `GrainShifterTester` friend class

3. **PluginProcessor** (`SOURCE/PluginProcessor.h`): Main JUCE audio processor
   - Coordinates pitch detection and shifting components
   - Manages audio parameter state via APVTS
   - Handles lookahead buffering for grain processing

### Supporting Components (Currently Active)

- **GrainBuffer** (`SOURCE/GRAIN/GrainBuffer.h`): Buffer management for double-buffered grain storage
- **PitchMarkedCircularBuffer**: Circular buffer with pitch period markers for grain extraction
- **Window** (from RD submodule): Provides various window shapes for grain envelopes

### Inactive/Legacy Components (Ignore unless specifically requested)

- **Grain** (`SOURCE/Grain.h/cpp`) - Legacy grain implementation
- **GrainCorrector** (`SOURCE/PITCH/GrainCorrector.h/cpp`) - Alternative pitch correction approach
- **Granulator** (`SOURCE/Granulator.h/cpp`) - Alternative granular synthesis implementation

### Key Implementation Details

The grain shifting algorithm requires careful management of:
- Grain spillover between process blocks (stored in `mGrainProcessingBuffer`)
- Proper alignment of grain write positions accounting for previous spillover
- Fractional grain periods for smooth pitch shifting
- Lookahead buffering to ensure enough source material for grain extraction

## Submodules

The project uses the RD submodule (`SUBMODULES/RD/`) providing utility classes:
- BufferHelper, BufferMath, BufferRange - Audio buffer manipulation
- CircularBuffer - Ring buffer implementation
- Window - Windowing functions for grain envelopes
- Interpolator - Sample interpolation utilities

## Python Utilities

- `addClass.py` - Generate new C++ class boilerplate
- `addFunction.py` - Add functions to existing classes
- `gen_sine_wave.py` - Generate test audio signals
- `regenSource.py` - Regenerate CMake source lists

## Current Development Focus

The `grain_shifter` branch indicates active development on the GrainShifter component, particularly around:
- Buffer continuity between process blocks
- Spillover grain handling
- Proper write position calculation accounting for previous grain spillover

When working on pitch shifting, pay special attention to:
- `_calculateFirstGrainStartingPos()` - Determines where first grain starts in output buffer
- `_writeGrainBufferSpilloverToOutput()` - Handles grains that span block boundaries
- `PreviousBlockData` struct - Tracks state between processBlock calls