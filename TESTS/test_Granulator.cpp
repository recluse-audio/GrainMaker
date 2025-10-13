/**
 * Created by Ryan Devens 2025-10-13
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../SOURCE/GRAIN/Granulator.h"
#include "../SUBMODULES/RD/SOURCE/BufferFiller.h"

TEST_CASE("Granulator basic granulateBuffer functionality", "[test_Granulator]")
{
	Granulator granulator;

	const int periodSize = 128;
	const int numPeriods = 8;
	const int totalSamples = periodSize * numPeriods; // 1024 samples

	// Create single period buffer and fill with incremental values 0-127
	juce::AudioBuffer<float> singlePeriodBuffer(1, periodSize);
	BufferFiller::fillIncremental(singlePeriodBuffer);

	// Verify single period buffer is filled correctly
	for (int i = 0; i < periodSize; ++i)
	{
		CHECK(singlePeriodBuffer.getSample(0, i) == static_cast<float>(i));
	}

	// Create lookahead buffer 1024 samples long
	juce::AudioBuffer<float> lookaheadBuffer(1, totalSamples);
	lookaheadBuffer.clear();

	// Fill lookahead buffer by copying the single period 8 times
	for (int period = 0; period < numPeriods; ++period)
	{
		int destStartSample = period * periodSize;
		for (int sample = 0; sample < periodSize; ++sample)
		{
			lookaheadBuffer.setSample(0, destStartSample + sample, singlePeriodBuffer.getSample(0, sample));
		}
	}

	// Verify lookahead buffer is filled correctly BEFORE granulation
	SECTION("Lookahead buffer filled with 8 cycles of 0-127")
	{
		for (int period = 0; period < numPeriods; ++period)
		{
			for (int sample = 0; sample < periodSize; ++sample)
			{
				int bufferIndex = period * periodSize + sample;
				float expectedValue = static_cast<float>(sample);
				float actualValue = lookaheadBuffer.getSample(0, bufferIndex);
				CHECK(actualValue == expectedValue);
			}
		}
	}

	// Create output buffer and verify it's all zeros
	juce::AudioBuffer<float> outputBuffer(1, totalSamples);
	outputBuffer.clear();

	SECTION("Output buffer is all zeros before granulation")
	{
		for (int i = 0; i < totalSamples; ++i)
		{
			CHECK(outputBuffer.getSample(0, i) == 0.0f);
		}
	}

	// Call granulateBuffer with no windowing
	float grainPeriod = static_cast<float>(periodSize);
	float emissionPeriod = static_cast<float>(periodSize);
	granulator.granulateBuffer(lookaheadBuffer, outputBuffer, grainPeriod, emissionPeriod, Window::Shape::kNone);

	// Verify output buffer contains the expected pattern
	SECTION("Output buffer contains 8 cycles of 0-127 after granulation")
	{
		for (int period = 0; period < numPeriods; ++period)
		{
			for (int sample = 0; sample < periodSize; ++sample)
			{
				int bufferIndex = period * periodSize + sample;
				float expectedValue = static_cast<float>(sample);
				float actualValue = outputBuffer.getSample(0, bufferIndex);
				CHECK(actualValue == Catch::Approx(expectedValue));
			}
		}
	}
}