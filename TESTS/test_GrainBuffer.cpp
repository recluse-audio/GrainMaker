/**
 * test_GrainBuffer.cpp
 * Created by Ryan Devens on 2025-10-12
 */

#include <catch2/catch_test_macros.hpp>
#include "../SOURCE/GRAIN/GrainBuffer.h"
#include <catch2/catch_approx.hpp>  // For Approx in Catch2 v3+
#include "../SUBMODULES/RD/TESTS/TEST_UTILS/TestUtils.h"

TEST_CASE("GrainBuffer Basic Functionality", "[test_GrainBuffer]")
{
	TestUtils::SetupAndTeardown setupAndTeardown;
	GrainBuffer grainBuffer;

	SECTION("Initial state")
	{
		// Check initial length is 0
		REQUIRE(grainBuffer.getLengthInSamples() == 0);

		// Check buffer reference is accessible
		auto& buffer = grainBuffer.getBufferReference();
		REQUIRE(buffer.getNumChannels() == 0);
		REQUIRE(buffer.getNumSamples() == 0);
	}

	SECTION("Set and get length in samples")
	{
		juce::int64 testLength = 1024;
		grainBuffer.setLengthInSamples(testLength);

		REQUIRE(grainBuffer.getLengthInSamples() == testLength);
	}

	SECTION("Multiple length updates")
	{
		juce::int64 length1 = 512;
		juce::int64 length2 = 2048;
		juce::int64 length3 = 128;

		grainBuffer.setLengthInSamples(length1);
		REQUIRE(grainBuffer.getLengthInSamples() == length1);

		grainBuffer.setLengthInSamples(length2);
		REQUIRE(grainBuffer.getLengthInSamples() == length2);

		grainBuffer.setLengthInSamples(length3);
		REQUIRE(grainBuffer.getLengthInSamples() == length3);
	}

	SECTION("Buffer reference access and modification")
	{
		// Get buffer reference and modify it
		auto& buffer = grainBuffer.getBufferReference();

		// Set buffer size
		int numChannels = 2;
		int numSamples = 512;
		buffer.setSize(numChannels, numSamples);

		// Verify the buffer was modified
		REQUIRE(buffer.getNumChannels() == numChannels);
		REQUIRE(buffer.getNumSamples() == numSamples);

		// Write some test data
		for (int channel = 0; channel < numChannels; ++channel)
		{
			auto* channelData = buffer.getWritePointer(channel);
			for (int sample = 0; sample < numSamples; ++sample)
			{
				channelData[sample] = static_cast<float>(sample) / static_cast<float>(numSamples);
			}
		}

		// Verify the data can be read back
		for (int channel = 0; channel < numChannels; ++channel)
		{
			const auto* channelData = buffer.getReadPointer(channel);
			for (int sample = 0; sample < numSamples; ++sample)
			{
				float expectedValue = static_cast<float>(sample) / static_cast<float>(numSamples);
				REQUIRE(channelData[sample] == Catch::Approx(expectedValue));
			}
		}
	}

	SECTION("Const buffer reference")
	{
		// Set up the buffer
		auto& buffer = grainBuffer.getBufferReference();
		buffer.setSize(1, 100);

		// Get const reference
		const GrainBuffer& constGrainBuffer = grainBuffer;
		const auto& constBuffer = constGrainBuffer.getBufferReference();

		// Verify const access works
		REQUIRE(constBuffer.getNumChannels() == 1);
		REQUIRE(constBuffer.getNumSamples() == 100);
	}
}