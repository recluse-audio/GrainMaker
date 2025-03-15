#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/catch_approx.hpp>  // For Approx in Catch2 v3+


#include "../SOURCE/Granulator.h"

#include "../SUBMODULES/RD/SOURCE/BufferFiller.h"

static const double kDefaultSampleRate = 48000;

TEST_CASE("Can set sample rate in Granulator")
{
    const double testSampleRate = 48000;
    Granulator granulator;
    granulator.prepare(testSampleRate);
    CHECK(granulator.getCurrentSampleRate() == testSampleRate);
}

TEST_CASE("Can set grain length in milliseconds in Granulator")
{
    const double testSampleRate = 48000;
    Granulator granulator;
    granulator.prepare(testSampleRate);
    granulator.setGrainLengthInMs(1000); 
    CHECK(granulator.getGrainLengthInSamples() == testSampleRate);
}

//==================
TEST_CASE("Can set grain emission rate in hertz in Granulator")
{
    const double testSampleRate = 48000;
    Granulator granulator;
    granulator.prepare(testSampleRate);
    granulator.setEmissionRateInHz(1); 
    CHECK(granulator.getEmissionPeriodInSamples() == testSampleRate);

    granulator.setEmissionRateInHz(100);
    CHECK(granulator.getEmissionPeriodInSamples() == 480);

    granulator.prepare(44100);
    granulator.setEmissionRateInHz(1);
    CHECK(granulator.getEmissionPeriodInSamples() == 44100);

    granulator.setEmissionRateInHz(2);
    CHECK(granulator.getEmissionPeriodInSamples() == 22050);

    granulator.setEmissionRateInHz(-1);
    CHECK(granulator.getEmissionPeriodInSamples() == 0);

    // This part ensures that a new sample rate in prepare causes an update of the emission period
    granulator.setEmissionRateInHz(1);
    granulator.prepare(96000); 
    CHECK(granulator.getEmissionPeriodInSamples() == 96000);


}


//======================
TEST_CASE("Can process buffer with no window")
{
    juce::AudioBuffer<float> buffer(1, kDefaultSampleRate);
    BufferFiller::fillWithAllOnes(buffer);

    Granulator granulator;
    granulator.prepare(kDefaultSampleRate);
    granulator.setEmissionRateInHz(1);
    granulator.setGrainLengthInMs(500);

    granulator.process(buffer);

    for(int sampleIndex = 0; sampleIndex < (kDefaultSampleRate * 0.5); sampleIndex++)
    {
        auto sample = buffer.getSample(0, sampleIndex);
        CHECK(sample == 1.f);
    }

    for(int sampleIndex = (kDefaultSampleRate * 0.5); sampleIndex < kDefaultSampleRate; sampleIndex++)
    {
        auto sample = buffer.getSample(0, sampleIndex);
        CHECK(sample == 0.f);
    }
}