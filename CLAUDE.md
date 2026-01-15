# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

GrainMaker is a JUCE-based audio plugin implementing pitch detection (YIN algorithm) and pitch shifting using granular synthesis techniques (TD-PSOLA). The project builds as VST3, AU, AUv3, and Standalone formats.

## Build Commands

### Building the Plugin
```bash
# Build VST3 plugin (Release mode)
./SCRIPTS/build_vst3.sh
# Or use cross-platform Python script
python SCRIPTS/build_vst3.py

# Build Standalone application
./SCRIPTS/build_app.sh
# Or use cross-platform Python script
python SCRIPTS/build_app.py

# Build AU (macOS only)
python SCRIPTS/build_au.py

# Full rebuild (clean and build all targets)
./SCRIPTS/rebuild_all.sh
# Or use cross-platform Python script
python SCRIPTS/rebuild_all.py

# Manual CMake build from BUILD directory
cd BUILD
cmake ..
cmake --build . --target GrainMaker_VST3
```

**Note**: Python scripts (`.py`) are the preferred cross-platform option and handle Windows/macOS/Linux. Shell scripts (`.sh`) exist for Unix systems but may not work on Windows.

### Testing
```bash
# Build and run all tests
./SCRIPTS/build_tests.sh
# Or use cross-platform Python script
python SCRIPTS/build_tests.py

# Run all tests
./BUILD/Tests

# Run specific test suite
./BUILD/Tests "[Granulator]"
./BUILD/Tests "[PitchDetector]"
./BUILD/Tests "[GrainBuffer]"

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

2. **Granulator** (`SOURCE/GRAIN/Granulator.h`): **PRIMARY DEVELOPMENT FOCUS** - Performs pitch shifting using granular synthesis
   - Handles grain windowing and overlap-add synthesis
   - Manages double-buffered grain storage (`mGrainBuffer1`, `mGrainBuffer2`)
   - Key method: `granulateBuffer()` - granulates input buffer with specified grain and emission periods
   - Supports both time-preserving and varispeed/tape speedup techniques
   - Test coverage in `TESTS/test_Granulator.cpp`

3. **PluginProcessor** (`SOURCE/PluginProcessor.h`): Main JUCE audio processor
   - Coordinates pitch detection and shifting components
   - Manages audio parameter state via APVTS
   - Handles lookahead buffering for grain processing

### Supporting Components (Currently Active)

- **GrainBuffer** (`SOURCE/GRAIN/GrainBuffer.h`): Buffer management for double-buffered grain storage
- **CircularBuffer** (from RD submodule): Ring buffer implementation used for lookahead buffering
- **Window** (from RD submodule): Provides various window shapes for grain envelopes

### Inactive/Legacy Components (Ignore unless specifically requested)

- **GrainShifter** (`SOURCE/GRAIN/GrainShifter.h/cpp`) - Alternative pitch shifting implementation (currently commented out)
- **Grain** (`SOURCE/Grain.h/cpp`) - Legacy grain implementation

### Key Implementation Details

The granular synthesis algorithm in Granulator requires careful management of:
- Double-buffered grain storage to handle continuous audio stream
- Grain read/write position tracking across buffer boundaries
- Fractional grain periods for smooth pitch shifting
- Window function application to prevent clicks and pops
- Handling final grain phase carryover between processBlock calls

## Submodules

The project uses the RD submodule (`SUBMODULES/RD/`) providing utility classes:
- **BufferHelper, BufferMath, BufferRange** - Audio buffer manipulation utilities
- **CircularBuffer** - Ring buffer implementation
- **Window** - Windowing functions for grain envelopes (Hanning, Hamming, Blackman, etc.)
- **Interpolator** - Sample interpolation utilities
- **BufferFiller** - Test utilities for filling buffers with test signals

## Python Utilities

Located in the root directory:
- `addClass.py` - Generate new C++ class boilerplate
- `addFunction.py` - Add functions to existing classes
- `gen_sine_wave.py` - Generate test audio signals
- `regenSource.py` - Regenerate CMake source lists

## Current Development Focus

The project is actively developing the Granulator component. Based on recent commits:
- All tests passing (some temporarily commented out in test_Granulator.cpp)
- Focus on ensuring proper grain handling across buffer boundaries
- Working on time-preserving vs. varispeed granulation modes

When working on pitch shifting, pay special attention to:
- `granulateBuffer()` - Core granulation algorithm
- `_shouldGranulateToActiveBuffer()` / `_shouldGranulateToInactiveBuffer()` - Buffer switching logic
- `mGrainReadPos` / `mGrainWritePos` - Position tracking
- `mFinalGrainNormalizedPhase` - Phase tracking for grain continuity
