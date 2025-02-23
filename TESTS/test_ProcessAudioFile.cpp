#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "../SOURCE/PITCH/PitchDetector.h"
#include "../SUBMODULES/RD/SOURCE/AudioFileProcessor.h"

// Use the RD/SOURCE/AudioFileProcessor to process a known audio file and get expected results

static const juce::String Somewhere_Golden_Wav_Path = "GOLDEN_Somewhere_Mono_441k.wav"; 

TEST_CASE("Process file, no correction.")
{
    
}