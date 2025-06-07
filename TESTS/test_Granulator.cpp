#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/catch_approx.hpp>  // For Approx in Catch2 v3+


#include "../SOURCE/Granulator.h"

#include "../SUBMODULES/RD/SOURCE/BufferFiller.h"
#include "../SUBMODULES/RD/SOURCE/Window.h"
#include "../SUBMODULES/RD/SOURCE/RelativeFilePath.h"
#include "../SUBMODULES/RD/SOURCE/BufferHelper.h"


static const double kDefaultSampleRate = 48000;

class GranulatorTester
{
public:
	GranulatorTester(Granulator& g) : mGranulator(g)
	{

	}

	Window& getWindow()
	{
		return mGranulator.mWindow;
	}
private:
	Granulator& mGranulator;
};

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
    // CHECK(granulator.getEmissionPeriodInSamples() == 96000);


}


//======================
TEST_CASE("Can process buffer with no window")
{
    // juce::AudioBuffer<float> buffer(1, kDefaultSampleRate);
    // BufferFiller::fillWithAllOnes(buffer);

    // Granulator granulator;
    // granulator.prepare(kDefaultSampleRate);
    // granulator.setEmissionRateInHz(1);
    // granulator.setGrainLengthInMs(500);

    // granulator.process(buffer);

    // for(int sampleIndex = 0; sampleIndex < (kDefaultSampleRate * 0.5); sampleIndex++)
    // {
    //     auto sample = buffer.getSample(0, sampleIndex);
    //     CHECK(sample == 1.f);
    // }

    // for(int sampleIndex = (kDefaultSampleRate * 0.5); sampleIndex < kDefaultSampleRate; sampleIndex++)
    // {
    //     auto sample = buffer.getSample(0, sampleIndex);
    //     CHECK(sample == 0.f);
    // }
}




//======================
TEST_CASE("Can granulate buffer with no window into multiple grains")
{
    double sampleRate = 48000.0;  // redundant local val, but expecting values below that depend on 48k
    juce::AudioBuffer<float> buffer(1, sampleRate);
    BufferFiller::fillWithAllOnes(buffer);

    Granulator granulator;
    granulator.prepare(sampleRate);
    // 10 emiisions/sec, each grain is half length
    granulator.setEmissionRateInHz(10);
    granulator.setGrainLengthInMs(50);

    granulator.process(buffer);

    int grainNumSamples = granulator.getGrainLengthInSamples();
    int emissionNumSamples = granulator.getEmissionPeriodInSamples();

    for(int sampleIndex = 0; sampleIndex < sampleRate; sampleIndex++)
    {
        auto sample = buffer.getSample(0, sampleIndex);

        // if(sampleIndex < grainNumSamples)
        //     CHECK(sample == 1.f);
        // else if(sampleIndex >= grainNumSamples && sampleIndex < emissionNumSamples)
        //     CHECK(sample == 0.f);


        // throwing in some by hand expected values.  This applies to 48k only.
        // jumping around to locations I know should have grains or should be silent.
        // if(sampleIndex < 2400)
        //     CHECK(sample == 1.f);
        // else if(sampleIndex >= 2400 && sampleIndex < 4799)
        //     CHECK(sample == 0.f);
        // else if(sampleIndex >= 19200 && sampleIndex < 21599)
        //     CHECK(sample == 1.f);
        // else if(sampleIndex >= 21600 && sampleIndex < 23999)
        //     CHECK(sample == 0.f);
        // else if(sampleIndex >= 43200 && sampleIndex < 45599)
        //     CHECK(sample == 1.f);
        // else if(sampleIndex >= 45600 && sampleIndex < 45599)
        //     CHECK(sample == 0.f);
    }


}



//======================
TEST_CASE("Can process buffer with hanning window")
{
    juce::AudioBuffer<float> buffer(1, 1024);
    BufferFiller::fillWithAllOnes(buffer);

    Granulator granulator;
    granulator.prepare(kDefaultSampleRate);
    granulator.setEmissionPeriodInSamples(1024);
    granulator.setGrainLengthInSamples(1024);
    granulator.setGrainShape(Window::Shape::kHanning);
    granulator.process(buffer);

    // Get golden hanning buffer from .csv
    auto goldenPath = RelativeFilePath::getGoldenFilePath("GOLDEN_HanningBuffer_1024.csv");
    juce::AudioBuffer<float> goldenBuffer;
    BufferFiller::loadFromCSV(goldenBuffer, goldenPath);

    CHECK(BufferHelper::buffersAreIdentical(goldenBuffer, buffer, 0.01) == true);

}

//==========
TEST_CASE("Grains are correctly written from lookahead buffer to output buffer with no shifting")
{
	// Granulator granulator;
	// granulator.prepare(kDefaultSampleRate);
	// granulator.setGrainShape(Window::Shape::kNone); // don't window the incremental values stored at index duh!

	// int period = 128;
	// int outputNumSamples = 1024;
	// int lookaheadNumSamples = period * 2;
	// float shiftRatio = 1.f; // no shift this test

	// // Fill lookaheadBuffer with incremental value 0-127 and looping once we reach 128
	// juce::AudioBuffer<float> lookaheadBuffer(1, outputNumSamples+lookaheadNumSamples);
	// lookaheadBuffer.clear();
	// BufferFiller::fillIncremental(lookaheadBuffer);

	// juce::AudioBuffer<float> outputBuffer(1, outputNumSamples);
	// outputBuffer.clear();
	
	// granulator.processShifting(lookaheadBuffer, outputBuffer, period, 1.f);

    // for(int sampleIndex = 0; sampleIndex < outputNumSamples; sampleIndex++)
    // {
	// 	int expectedSample = sampleIndex + lookaheadNumSamples;
    //     int sample = (int)outputBuffer.getSample(0, sampleIndex);
    //     REQUIRE(sample == expectedSample);
    // }
}

//==========
TEST_CASE("Correct range is read from lookaheadBuffer and written to outputBuffer with no shifting")
{
	// Granulator granulator;
	// granulator.prepare(kDefaultSampleRate);
	// granulator.setGrainShape(Window::Shape::kNone); // don't window the incremental values stored at index duh!

	// int period = 128;
	// int outputNumSamples = 1024;
	// int lookaheadNumSamples = period * 2;
	// float shiftRatio = 1.f; // no shift this test

	// // Fill lookaheadBuffer with incremental value 0-127 and looping once we reach 128
	// juce::AudioBuffer<float> lookaheadBuffer(1, outputNumSamples+lookaheadNumSamples);
	// lookaheadBuffer.clear();
	// BufferFiller::fillIncremental(lookaheadBuffer);

	// juce::AudioBuffer<float> outputBuffer(1, outputNumSamples);
	// outputBuffer.clear();
	
	// granulator.processShifting(lookaheadBuffer, outputBuffer, period, 1.f);

    // for(int sampleIndex = 0; sampleIndex < outputNumSamples; sampleIndex++)
    // {
	// 	int expectedSample = sampleIndex + lookaheadNumSamples;
    //     int sample = (int)outputBuffer.getSample(0, sampleIndex);
    //     REQUIRE(sample == expectedSample);
    // }
}

//==========
TEST_CASE("Test read/writing hanning windowed grains with no shift")
{
	// Granulator granulator;
	// granulator.prepare(kDefaultSampleRate);
	// granulator.setGrainShape(Window::Shape::kHanning); // don't window the incremental values stored at index duh!

	// int period = 128;
	// int outputNumSamples = 1024;
	// int lookaheadNumSamples = period * 2;
	// float shiftRatio = 1.f; // no shift this test

	// // Fill lookaheadBuffer with incremental value 0-127 and looping once we reach 128
	// juce::AudioBuffer<float> lookaheadBuffer(1, outputNumSamples+lookaheadNumSamples);
	// lookaheadBuffer.clear();
	// BufferFiller::fillWithAllOnes(lookaheadBuffer);

	// juce::AudioBuffer<float> outputBuffer(1, outputNumSamples);
	// outputBuffer.clear();
	
	// granulator.processShifting(lookaheadBuffer, outputBuffer, period, shiftRatio);
	// int shiftInSamples = period - (period / shiftRatio);

	// // create a hanningBuffer at the size of the period being used.  This should match the buffers written to outputBuffer 
	// juce::AudioBuffer<float> hanningBuffer;
	// hanningBuffer.clear();
	// hanningBuffer.setSize(1, period);
	// BufferFiller::generateHanning(hanningBuffer);

	// GranulatorTester granulatorTester(granulator);
	// Window& window = granulatorTester.getWindow();

	// int indexInPeriod = 0;
	
    // for(int sampleIndex = 0; sampleIndex < outputNumSamples; sampleIndex++)
    // {
	// 	float hanningSample = hanningBuffer.getSample(0, indexInPeriod);
	// 	indexInPeriod++;
	// 	indexInPeriod = indexInPeriod % period;

    //     float sample = outputBuffer.getSample(0, sampleIndex);
    //     REQUIRE(sample == Catch::Approx(hanningSample).margin(0.1)); // high tolerance b/c these are interpolating a lot
    // }
}

//==========
TEST_CASE("Correct range is read from lookaheadBuffer and written to outputBuffer while shifting up")
{
	// Granulator granulator;
	// granulator.prepare(kDefaultSampleRate);
	// granulator.setGrainShape(Window::Shape::kHanning); // don't window the incremental values stored at index duh!

	// int period = 128;
	// int outputNumSamples = 1024;
	// int lookaheadNumSamples = period * 2;
	// float shiftRatio = 1.f; // no shift this test

	// // Fill lookaheadBuffer with incremental value 0-127 and looping once we reach 128
	// juce::AudioBuffer<float> lookaheadBuffer(1, outputNumSamples+lookaheadNumSamples);
	// lookaheadBuffer.clear();
	// BufferFiller::fillWithAllOnes(lookaheadBuffer);

	// juce::AudioBuffer<float> outputBuffer(1, outputNumSamples);
	// outputBuffer.clear();
	
	// float shiftRatio = 1.25f;
	// granulator.processShifting(lookaheadBuffer, outputBuffer, period, shiftRatio);
	// int shiftInSamples = period - (period / shiftRatio);

	// // create a hanningBuffer at the size of the period being used.  This should match the buffers written to outputBuffer 
	// juce::AudioBuffer<float> hanningBuffer;
	// hanningBuffer.clear();
	// hanningBuffer.setSize(1, period);
	// BufferFiller::generateHanning(hanningBuffer);

    // for(int sampleIndex = 0; sampleIndex < outputNumSamples; sampleIndex++)
    // {
		
    //     float sample = (int)outputBuffer.getSample(0, sampleIndex);
    //     REQUIRE(sample == expectedSample);
    // }
}