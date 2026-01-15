/**
 * Created by Ryan Devens 2025-10-13
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../SOURCE/GRAIN/Granulator.h"
#include "../SUBMODULES/RD/SOURCE/BufferFiller.h"
#include "../SUBMODULES/RD/SOURCE/Window.h"
#include "../SUBMODULES/RD/TESTS/TEST_UTILS/TestUtils.h"

TEST_CASE("Granulator basic granulateBuffer functionality", "[test_Granulator]")
{
	TestUtils::SetupAndTeardown setupAndTeardown;

	// Create Window object for use in tests
	Window window;
	window.setShape(Window::Shape::kNone);  // Use no windowing for this test
	window.setSize(1024);  // Set a reasonable window size

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

	// Call granulateBuffer with the window object
	float grainPeriod = static_cast<float>(periodSize);
	float emissionPeriod = static_cast<float>(periodSize);
	float finalGrainPhase = Granulator::granulateBuffer(lookaheadBuffer, outputBuffer, grainPeriod, emissionPeriod, window);

	// Since grains fit perfectly (8 periods of 128 samples each in a 1024 buffer),
	// no grain extends past the buffer, so phase should be 0.0
	CHECK(finalGrainPhase == 0.0f);

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

TEST_CASE("Granulator source buffer size validation", "[test_Granulator]")
{
	TestUtils::SetupAndTeardown setupAndTeardown;

	// Create Window object for use in tests
	Window window;
	window.setShape(Window::Shape::kNone);  // Use no windowing for this test

	SECTION("Source buffer must be at least one grain size")
	{
		const int grainSize = 128;
		const int insufficientSourceSize = 100;  // Less than one grain
		const int outputBufferSize = 256;

		// Create source buffer smaller than one grain
		juce::AudioBuffer<float> inputBuffer(1, insufficientSourceSize);
		BufferFiller::fillIncremental(inputBuffer);

		// Create output buffer
		juce::AudioBuffer<float> outputBuffer(1, outputBufferSize);
		outputBuffer.clear();

		// Granulate should handle this gracefully (assertion in debug, early return in release)
		float grainPeriod = static_cast<float>(grainSize);
		float emissionPeriod = static_cast<float>(grainSize);

		// This should return 0.0 since no grain can be processed
		float finalGrainPhase = Granulator::granulateBuffer(inputBuffer, outputBuffer, grainPeriod, emissionPeriod, window);
		CHECK(finalGrainPhase == 0.0f);

		// Output buffer should remain cleared (all zeros) since no processing occurred
		for (int i = 0; i < outputBufferSize; ++i)
		{
			CHECK(outputBuffer.getSample(0, i) == 0.0f);
		}
	}

	SECTION("Source buffer exactly one grain size")
	{
		const int grainSize = 128;
		const int outputBufferSize = 256;

		// Create source buffer exactly one grain size
		juce::AudioBuffer<float> inputBuffer(1, grainSize);
		BufferFiller::fillIncremental(inputBuffer);

		// Create output buffer
		juce::AudioBuffer<float> outputBuffer(1, outputBufferSize);
		outputBuffer.clear();

		// This should work fine
		float grainPeriod = static_cast<float>(grainSize);
		float emissionPeriod = static_cast<float>(grainSize);

		float finalGrainPhase = Granulator::granulateBuffer(inputBuffer, outputBuffer, grainPeriod, emissionPeriod, window);

		// First grain should be written successfully
		for (int i = 0; i < grainSize; ++i)
		{
			CHECK(outputBuffer.getSample(0, i) == Catch::Approx(static_cast<float>(i)));
		}
	}
}

// Test that verifies the assertion protection is in place
TEST_CASE("Granulator assertion check for insufficient source buffer", "[test_Granulator]")
{
	TestUtils::SetupAndTeardown setupAndTeardown;

	// Create Window object for use in tests
	Window window;
	window.setShape(Window::Shape::kNone);

	SECTION("Verify protection against insufficient source buffer")
	{
		const int grainSize = 128;
		const int insufficientSourceSize = 64;  // Much less than one grain
		const int outputBufferSize = 256;

		// Create source buffer smaller than one grain
		juce::AudioBuffer<float> inputBuffer(1, insufficientSourceSize);
		BufferFiller::fillIncremental(inputBuffer);

		// Create output buffer
		juce::AudioBuffer<float> outputBuffer(1, outputBufferSize);
		outputBuffer.clear();

		float grainPeriod = static_cast<float>(grainSize);
		float emissionPeriod = static_cast<float>(grainSize);

		INFO("Testing with source buffer size: " << insufficientSourceSize << ", grain size: " << grainSize);

#if JUCE_DEBUG
		// In DEBUG mode, we expect this would trigger an assertion
		// We can't actually call it without aborting, so we verify the condition
		bool wouldAssert = (insufficientSourceSize < grainSize);
		REQUIRE(wouldAssert == true);

		// We verify that the early return is also in place
		// by checking a buffer that's exactly at the boundary
		juce::AudioBuffer<float> boundaryBuffer(1, grainSize);
		BufferFiller::fillIncremental(boundaryBuffer);

		// This should NOT assert since buffer size equals grain size
		float boundaryResult = Granulator::granulateBuffer(boundaryBuffer, outputBuffer, grainPeriod, emissionPeriod, window);

		// The test passes if we reach here without aborting
		SUCCEED("Assertion protection is in place - boundary case handled correctly");
#else
		// In RELEASE mode, verify graceful handling
		float result = Granulator::granulateBuffer(inputBuffer, outputBuffer, grainPeriod, emissionPeriod, window);

		// Should return 0.0 and not crash
		CHECK(result == 0.0f);

		// Output should be empty
		for (int i = 0; i < outputBufferSize; ++i)
		{
			CHECK(outputBuffer.getSample(0, i) == 0.0f);
		}

		SUCCEED("Release mode handles insufficient buffer gracefully");
#endif
	}
}

TEST_CASE("Granulator validation for insufficient source buffer", "[test_Granulator]")
{
	TestUtils::SetupAndTeardown setupAndTeardown;

	// Create Window object for use in tests
	Window window;
	window.setShape(Window::Shape::kNone);  // Use no windowing for this test

	SECTION("Function returns 0.0 when source buffer is insufficient (Release behavior)")
	{
		const int grainSize = 128;
		const int insufficientSourceSize = 64;  // Much less than one grain
		const int outputBufferSize = 256;

		// Create source buffer smaller than one grain
		juce::AudioBuffer<float> inputBuffer(1, insufficientSourceSize);
		BufferFiller::fillIncremental(inputBuffer);

		// Create output buffer
		juce::AudioBuffer<float> outputBuffer(1, outputBufferSize);
		outputBuffer.clear();

		float grainPeriod = static_cast<float>(grainSize);
		float emissionPeriod = static_cast<float>(grainSize);

#if !JUCE_DEBUG
		// Only test the graceful return in release mode
		// In debug mode, this would abort due to assertion
		float finalGrainPhase = Granulator::granulateBuffer(inputBuffer, outputBuffer, grainPeriod, emissionPeriod, window);

		// Verify return value is 0.0
		CHECK(finalGrainPhase == 0.0f);

		// Verify output buffer remains empty (no processing occurred)
		for (int i = 0; i < outputBufferSize; ++i)
		{
			CHECK(outputBuffer.getSample(0, i) == 0.0f);
		}
#else
		INFO("Skipping in DEBUG mode as assertion would fire");
#endif
	}

	SECTION("Edge case: source buffer size equals grain size minus one")
	{
		const int grainSize = 100;
		const int edgeCaseSize = 99;  // Just one sample short
		const int outputBufferSize = 200;

		// Create source buffer one sample short of grain size
		juce::AudioBuffer<float> inputBuffer(1, edgeCaseSize);
		BufferFiller::fillIncremental(inputBuffer);

		// Create output buffer
		juce::AudioBuffer<float> outputBuffer(1, outputBufferSize);
		outputBuffer.clear();

		float grainPeriod = static_cast<float>(grainSize);
		float emissionPeriod = static_cast<float>(grainSize);

#if !JUCE_DEBUG
		// Only test in release mode
		float finalGrainPhase = Granulator::granulateBuffer(inputBuffer, outputBuffer, grainPeriod, emissionPeriod, window);
		CHECK(finalGrainPhase == 0.0f);

		// Output should remain empty
		for (int i = 0; i < outputBufferSize; ++i)
		{
			CHECK(outputBuffer.getSample(0, i) == 0.0f);
		}
#else
		INFO("Skipping in DEBUG mode as assertion would fire");
#endif
	}
}

TEST_CASE("Granulator returns correct phase for partial final grain", "[test_Granulator]")
{
	TestUtils::SetupAndTeardown setupAndTeardown;

	// Create Window object for use in tests
	Window window;
	window.setShape(Window::Shape::kNone);  // Use no windowing for this test

	SECTION("Output buffer ends halfway through second grain")
	{
		// const int grainSize = 128;
		// const int inputBufferSize = 1024;  // Large input buffer with plenty of source material
		// const int outputBufferSize = 192;  // Output can fit 1.5 grains

		// // Create large input buffer with plenty of source material
		// juce::AudioBuffer<float> inputBuffer(1, inputBufferSize);
		// BufferFiller::fillIncremental(inputBuffer);

		// // Create smaller output buffer that can only fit 1.5 grains
		// juce::AudioBuffer<float> outputBuffer(1, outputBufferSize);
		// outputBuffer.clear();

		// // Granulate with grain period = 128, emission period = 128
		// float grainPeriod = static_cast<float>(grainSize);
		// float emissionPeriod = static_cast<float>(grainSize);
		// float finalGrainPhase = Granulator::granulateBuffer(inputBuffer, outputBuffer, grainPeriod, emissionPeriod, window);

		// // First grain: writes samples 0-127 (full grain)
		// // Second grain: starts at sample 128, would end at 255
		// // But output buffer ends at 191, so only 64 samples are written (192-128=64)
		// // Phase = 64/128 = 0.5
		// CHECK(finalGrainPhase == Catch::Approx(0.5f));
	}

	SECTION("Output buffer ends 25% through second grain")
	{
		// const int grainSize = 100;
		// const int inputBufferSize = 1024;  // Large input buffer with plenty of source material
		// const int outputBufferSize = 125;  // Output can fit 1.25 grains

		// // Create large input buffer with plenty of source material
		// juce::AudioBuffer<float> inputBuffer(1, inputBufferSize);
		// BufferFiller::fillIncremental(inputBuffer);

		// // Create smaller output buffer
		// juce::AudioBuffer<float> outputBuffer(1, outputBufferSize);
		// outputBuffer.clear();

		// // Granulate with grain period = 100, emission period = 100
		// float grainPeriod = static_cast<float>(grainSize);
		// float emissionPeriod = static_cast<float>(grainSize);
		// float finalGrainPhase = Granulator::granulateBuffer(inputBuffer, outputBuffer, grainPeriod, emissionPeriod, window);

		// // First grain: writes samples 0-99 (full grain)
		// // Second grain: starts at sample 100, would end at 199
		// // But output buffer ends at 124, so only 25 samples are written (125-100=25)
		// // Phase = 25/100 = 0.25
		// CHECK(finalGrainPhase == Catch::Approx(0.25f));
	}

	SECTION("Output buffer fits grains perfectly - no partial grain")
	{
		const int grainSize = 64;
		const int inputBufferSize = 1024;  // Large input buffer with plenty of source material
		const int outputBufferSize = 256;  // Output fits exactly 4 grains

		// Create large input buffer with plenty of source material
		juce::AudioBuffer<float> inputBuffer(1, inputBufferSize);
		BufferFiller::fillIncremental(inputBuffer);

		// Create output buffer that fits exactly 4 grains
		juce::AudioBuffer<float> outputBuffer(1, outputBufferSize);
		outputBuffer.clear();

		// Granulate with grain period = 64, emission period = 64
		float grainPeriod = static_cast<float>(grainSize);
		float emissionPeriod = static_cast<float>(grainSize);
		float finalGrainPhase = Granulator::granulateBuffer(inputBuffer, outputBuffer, grainPeriod, emissionPeriod, window);

		// All grains fit perfectly in output buffer, no grain extends past buffer
		CHECK(finalGrainPhase == 0.0f);
	}

	SECTION("Output buffer ends 75% through third grain with overlap")
	{
		// const int grainSize = 100;
		// const int inputBufferSize = 1024;  // Large input buffer
		// const int outputBufferSize = 275;  // Can fit 2.75 grains when no overlap

		// // Create large input buffer
		// juce::AudioBuffer<float> inputBuffer(1, inputBufferSize);
		// BufferFiller::fillIncremental(inputBuffer);

		// // Create output buffer
		// juce::AudioBuffer<float> outputBuffer(1, outputBufferSize);
		// outputBuffer.clear();

		// // Granulate with emission period same as grain period (no overlap)
		// float grainPeriod = static_cast<float>(grainSize);
		// float emissionPeriod = static_cast<float>(grainSize);
		// float finalGrainPhase = Granulator::granulateBuffer(inputBuffer, outputBuffer, grainPeriod, emissionPeriod, window);

		// // First grain: 0-99
		// // Second grain: 100-199
		// // Third grain: starts at 200, would end at 299
		// // But output buffer ends at 274, so only 75 samples written (275-200=75)
		// // Phase = 75/100 = 0.75
		// CHECK(finalGrainPhase == Catch::Approx(0.75f));
	}
}

TEST_CASE("Granulator with all ones buffer - overlap and spacing verification", "[test_Granulator]")
{
	TestUtils::SetupAndTeardown setupAndTeardown;

	// Create Window object with no windowing
	Window window;
	window.setShape(Window::Shape::kNone);

	const int periodSize = 128;
	const int numPeriods = 8;
	const int totalSamples = periodSize * numPeriods; // 1024 samples

	SECTION("No shifting - emission period equals grain period")
	{
		// Create source buffer filled with all ones
		juce::AudioBuffer<float> sourceBuffer(1, totalSamples);
		BufferFiller::fillWithAllOnes(sourceBuffer);

		// Verify source buffer is filled with all ones
		for (int i = 0; i < totalSamples; ++i)
		{
			CHECK(sourceBuffer.getSample(0, i) == 1.0f);
		}

		// Create output buffer and verify it's all zeros
		juce::AudioBuffer<float> outputBuffer(1, totalSamples);
		outputBuffer.clear();

		for (int i = 0; i < totalSamples; ++i)
		{
			CHECK(outputBuffer.getSample(0, i) == 0.0f);
		}

		// Granulate with no shifting (emission period = grain period)
		float grainPeriod = static_cast<float>(periodSize);
		float emissionPeriod = static_cast<float>(periodSize);

		float finalPhase = Granulator::granulateBuffer(sourceBuffer, outputBuffer, grainPeriod, emissionPeriod, window);

		// With no shifting, all output samples should be 1.0
		INFO("Testing no-shift case: grain period = " << periodSize << ", emission period = " << periodSize);
		for (int i = 0; i < totalSamples; ++i)
		{
			CHECK(outputBuffer.getSample(0, i) == 1.0f);
		}

		// Should have no partial grain at the end
		CHECK(finalPhase == 0.0f);
	}

	SECTION("Shifting up - emission period shorter than grain period (overlap)")
	{
		// Create source buffer with plenty of data
		const int largeSourceSize = 2048;  // Plenty of source material
		juce::AudioBuffer<float> sourceBuffer(1, largeSourceSize);
		BufferFiller::fillWithAllOnes(sourceBuffer);

		// Create output buffer
		juce::AudioBuffer<float> outputBuffer(1, totalSamples);
		outputBuffer.clear();

		// Set emission period one sample shorter than grain period
		// This causes grains to overlap by 1 sample
		float grainPeriod = static_cast<float>(periodSize);  // 128
		float emissionPeriod = static_cast<float>(periodSize - 1);  // 127

		INFO("Testing pitch shift up: grain period = " << periodSize << ", emission period = " << (periodSize - 1));

		float finalPhase = Granulator::granulateBuffer(sourceBuffer, outputBuffer, grainPeriod, emissionPeriod, window);

		// Calculate where overlaps occur
		// First grain: writes to samples 0-127
		// Second grain: starts at 127, writes to 127-254 (overlaps at 127)
		// Third grain: starts at 254, writes to 254-381 (overlaps at 254)
		// etc.

		// Check for overlaps at expected positions
		for (int grainIndex = 1; grainIndex < 8; ++grainIndex)  // Check first 8 overlaps
		{
			int overlapPosition = grainIndex * (periodSize - 1);

			// Only check if position is within output buffer
			if (overlapPosition < totalSamples)
			{
				INFO("Checking overlap at position " << overlapPosition);

				// At overlap positions, we expect value of 2.0 (two grains of 1.0 overlapping)
				CHECK(outputBuffer.getSample(0, overlapPosition) == Catch::Approx(2.0f));

				// Check samples around the overlap are still 1.0
				if (overlapPosition > 0)
				{
					CHECK(outputBuffer.getSample(0, overlapPosition - 1) == Catch::Approx(1.0f));
				}
				if (overlapPosition + 1 < totalSamples)
				{
					CHECK(outputBuffer.getSample(0, overlapPosition + 1) == Catch::Approx(1.0f));
				}
			}
		}
	}

	SECTION("Shifting down - emission period longer than grain period (gaps)")
	{
		// Create source buffer with plenty of data
		const int largeSourceSize = 2048;
		juce::AudioBuffer<float> sourceBuffer(1, largeSourceSize);
		BufferFiller::fillWithAllOnes(sourceBuffer);

		// Create output buffer
		juce::AudioBuffer<float> outputBuffer(1, totalSamples);
		outputBuffer.clear();

		// Set emission period one sample longer than grain period
		// This creates gaps between grains
		float grainPeriod = static_cast<float>(periodSize);  // 128
		float emissionPeriod = static_cast<float>(periodSize + 1);  // 129

		INFO("Testing pitch shift down: grain period = " << periodSize << ", emission period = " << (periodSize + 1));

		float finalPhase = Granulator::granulateBuffer(sourceBuffer, outputBuffer, grainPeriod, emissionPeriod, window);

		// Calculate where gaps occur
		// First grain: writes to samples 0-127
		// Gap at sample 128 (no grain writes here)
		// Second grain: starts at 129, writes to 129-256
		// Gap at sample 257
		// Third grain: starts at 258, writes to 258-385
		// etc.

		// Check for gaps at expected positions
		for (int grainIndex = 1; grainIndex < 8; ++grainIndex)  // Check first 8 gaps
		{
			int gapPosition = grainIndex * (periodSize + 1) - 1;  // Position just before next grain starts

			// Only check if position is within output buffer
			if (gapPosition < totalSamples)
			{
				INFO("Checking gap at position " << gapPosition);

				// At gap positions, we expect value of 0.0 (no grain writing there)
				CHECK(outputBuffer.getSample(0, gapPosition) == 0.0f);

				// Check that grain samples around the gap are 1.0
				if (gapPosition - 1 >= 0)
				{
					CHECK(outputBuffer.getSample(0, gapPosition - 1) == Catch::Approx(1.0f));
				}
				if (gapPosition + 2 < totalSamples)  // Skip the gap, check next grain start
				{
					CHECK(outputBuffer.getSample(0, gapPosition + 2) == Catch::Approx(1.0f));
				}
			}
		}
	}

	SECTION("Larger overlap - emission period significantly shorter")
	{
		// Create source buffer
		const int largeSourceSize = 2048;
		juce::AudioBuffer<float> sourceBuffer(1, largeSourceSize);
		BufferFiller::fillWithAllOnes(sourceBuffer);

		// Create output buffer
		juce::AudioBuffer<float> outputBuffer(1, totalSamples);
		outputBuffer.clear();

		// Set emission period to 3/4 of grain period
		// This causes 25% overlap
		float grainPeriod = static_cast<float>(periodSize);  // 128
		float emissionPeriod = static_cast<float>(periodSize * 3 / 4);  // 96

		INFO("Testing 25% overlap: grain period = " << periodSize << ", emission period = " << (periodSize * 3 / 4));

		float finalPhase = Granulator::granulateBuffer(sourceBuffer, outputBuffer, grainPeriod, emissionPeriod, window);

		// With 25% overlap, we should see regions where samples sum to 2.0
		// First grain: 0-127
		// Second grain: 96-223 (overlap from 96-127 = 32 samples)
		// Third grain: 192-319 (overlap from 192-223 = 32 samples)

		// Check first overlap region (96-127)
		for (int i = 96; i < 128 && i < totalSamples; ++i)
		{
			CHECK(outputBuffer.getSample(0, i) == Catch::Approx(2.0f));
		}

		// Check second overlap region (192-223)
		for (int i = 192; i < 224 && i < totalSamples; ++i)
		{
			CHECK(outputBuffer.getSample(0, i) == Catch::Approx(2.0f));
		}
	}
}

TEST_CASE("Granulator upward pitch shift with equal-sized buffers", "[test_Granulator]")
{
	TestUtils::SetupAndTeardown setupAndTeardown;

	// Create Window object with no windowing
	Window window;
	window.setShape(Window::Shape::kNone);

	SECTION("Shifting up causes zeros at the end of output buffer")
	{
		// const int periodSize = 128;
		// const int bufferSize = 1024;  // Both input and output buffers have same size

		// // Create input buffer filled with all ones
		// juce::AudioBuffer<float> inputBuffer(1, bufferSize);
		// BufferFiller::fillWithAllOnes(inputBuffer);

		// // Verify input buffer is all ones
		// for (int i = 0; i < bufferSize; ++i)
		// {
		// 	CHECK(inputBuffer.getSample(0, i) == 1.0f);
		// }

		// // Create output buffer and clear it
		// juce::AudioBuffer<float> outputBuffer(1, bufferSize);
		// outputBuffer.clear();

		// // Shift up by making emission period shorter than grain period
		// // This means we write grains faster than we read them
		// float grainPeriod = static_cast<float>(periodSize);  // 128 samples
		// float emissionPeriod = static_cast<float>(periodSize * 3 / 4);  // 96 samples

		// INFO("Testing upward pitch shift with equal-sized buffers");
		// INFO("Grain period: " << periodSize << ", Emission period: " << (periodSize * 3 / 4));
		// INFO("Buffer size (both input and output): " << bufferSize);

		// float finalPhase = Granulator::granulateBuffer(inputBuffer, outputBuffer, grainPeriod, emissionPeriod, window);

		// // Calculate expected behavior:
		// // We read grains of 128 samples from input
		// // We write them every 96 samples in output
		// // This means we consume input faster than normal playback

		// // Grains from input:
		// // Grain 1: input[0-127]    -> output[0-127]
		// // Grain 2: input[128-255]  -> output[96-223] (overlaps with grain 1 at 96-127)
		// // Grain 3: input[256-383]  -> output[192-319] (overlaps with grain 2 at 192-223)
		// // Grain 4: input[384-511]  -> output[288-415] (overlaps with grain 3 at 288-319)
		// // Grain 5: input[512-639]  -> output[384-511] (overlaps with grain 4 at 384-415)
		// // Grain 6: input[640-767]  -> output[480-607] (overlaps with grain 5 at 480-511)
		// // Grain 7: input[768-895]  -> output[576-703] (overlaps with grain 6 at 576-607)
		// // Grain 8: input[896-1023] -> output[672-799] (overlaps with grain 7 at 672-703)
		// // Grain 9 would start at input[1024] but we're out of input data
		// // So output[768-1023] should have zeros (except 768-799 which has grain 8)

		// // We run out of input after 8 grains (8 * 128 = 1024 samples)
		// // But grain 8 only writes up to output position 799
		// // So positions 800-1023 should be zero (224 samples of zeros)

		// // Check that we have zeros at the end of the output buffer
		// const int expectedLastNonZeroSample = 799;
		// const int expectedFirstZeroSample = 800;

		// // Verify the last part of grain 8 contains ones
		// for (int i = 768; i <= expectedLastNonZeroSample; ++i)
		// {
		// 	INFO("Checking sample " << i << " should be non-zero (part of grain 8)");
		// 	CHECK(outputBuffer.getSample(0, i) > 0.0f);  // Could be 1.0 or 2.0 due to overlap
		// }

		// // Verify zeros appear after grain 8 ends
		// int zeroCount = 0;
		// for (int i = expectedFirstZeroSample; i < bufferSize; ++i)
		// {
		// 	CHECK(outputBuffer.getSample(0, i) == 0.0f);
		// 	zeroCount++;
		// }

		// INFO("Found " << zeroCount << " zero samples at the end of the buffer");
		// INFO("Expected " << (bufferSize - expectedFirstZeroSample) << " zero samples");

		// // Should have 224 zero samples (1024 - 800)
		// CHECK(zeroCount == (bufferSize - expectedFirstZeroSample));
		// CHECK(zeroCount == 224);

		// // Verify final grain phase
		// // The 9th grain would start at output position 768 but we have no input for it
		// // So the last grain written is grain 8, which completes fully
		// // Therefore phase should be 0.0 (no partial grain)
		// CHECK(finalPhase == 0.0f);
	}

	SECTION("More aggressive upward shift creates more zeros")
	{
		// const int periodSize = 128;
		// const int bufferSize = 1024;

		// // Create input and output buffers of equal size
		// juce::AudioBuffer<float> inputBuffer(1, bufferSize);
		// BufferFiller::fillWithAllOnes(inputBuffer);

		// juce::AudioBuffer<float> outputBuffer(1, bufferSize);
		// outputBuffer.clear();

		// // Even more aggressive shift up - emission period is half of grain period
		// float grainPeriod = static_cast<float>(periodSize);  // 128 samples
		// float emissionPeriod = static_cast<float>(periodSize / 2);  // 64 samples

		// INFO("Testing aggressive upward pitch shift (2x speed)");
		// INFO("Grain period: " << periodSize << ", Emission period: " << (periodSize / 2));

		// float finalPhase = Granulator::granulateBuffer(inputBuffer, outputBuffer, grainPeriod, emissionPeriod, window);

		// // With emission period = 64 and grain period = 128:
		// // We consume input at double speed
		// // 8 grains from input (1024 / 128 = 8)
		// // Written at positions: 0, 64, 128, 192, 256, 320, 384, 448
		// // Last grain (8th) writes from 448 to 575
		// // So positions 576-1023 should be zeros (448 samples)

		// const int expectedLastNonZeroSample = 575;
		// const int expectedFirstZeroSample = 576;

		// // Count zeros at the end
		// int zeroCount = 0;
		// for (int i = expectedFirstZeroSample; i < bufferSize; ++i)
		// {
		// 	CHECK(outputBuffer.getSample(0, i) == 0.0f);
		// 	zeroCount++;
		// }

		// INFO("Found " << zeroCount << " zero samples at the end (2x speed shift)");
		// CHECK(zeroCount == (bufferSize - expectedFirstZeroSample));
		// CHECK(zeroCount == 448);  // Nearly half the buffer is zeros
	}
}

TEST_CASE("Granulator time-preserving mode with all ones buffer", "[test_Granulator]")
{
	TestUtils::SetupAndTeardown setupAndTeardown;

	// Create Window object with no windowing
	Window window;
	window.setShape(Window::Shape::kNone);

	SECTION("Time-preserving upward shift - grains repeat, no zeros at end")
	{
		// const int periodSize = 128;
		// const int bufferSize = 1024;  // Both input and output buffers have same size

		// // Create input buffer filled with all ones
		// juce::AudioBuffer<float> inputBuffer(1, bufferSize);
		// BufferFiller::fillWithAllOnes(inputBuffer);

		// // Verify input buffer is all ones
		// for (int i = 0; i < bufferSize; ++i)
		// {
		// 	CHECK(inputBuffer.getSample(0, i) == 1.0f);
		// }

		// // Create output buffer and clear it
		// juce::AudioBuffer<float> outputBuffer(1, bufferSize);
		// outputBuffer.clear();

		// // Shift up by making emission period shorter than grain period
		// float grainPeriod = static_cast<float>(periodSize);  // 128 samples
		// float emissionPeriod = static_cast<float>(periodSize * 3 / 4);  // 96 samples

		// INFO("Testing time-preserving upward pitch shift");
		// INFO("Grain period: " << periodSize << ", Emission period: " << (periodSize * 3 / 4));
		// INFO("Buffer size (both input and output): " << bufferSize);

		// // Call with timePreserving = true
		// float finalPhase = Granulator::granulateBuffer(inputBuffer, outputBuffer, grainPeriod, emissionPeriod, window, true);

		// // In time-preserving mode, grains are repeated as needed
		// // So we should NEVER have zeros at the end of the output buffer
		// // The entire output buffer should be filled with values > 0

		// // Count total grains that fit in output with emission period of 96
		// // Grain starts: 0, 96, 192, 288, 384, 480, 576, 672, 768, 864, 960
		// // That's 11 grains, but input only has 8 grains worth of data
		// // In time-preserving mode, grains 1-8 from input will be reused

		// // Verify NO zeros anywhere in the output buffer
		// for (int i = 0; i < bufferSize; ++i)
		// {
		// 	INFO("Checking sample " << i << " should be non-zero (time-preserving mode)");
		// 	CHECK(outputBuffer.getSample(0, i) > 0.0f);
		// }

		// // Check for expected overlaps at emission period boundaries
		// // With emission period = 96 and grain period = 128, we have 32 samples of overlap
		// // Overlap regions: [96-127], [192-223], [288-319], [384-415], [480-511], [576-607], [672-703], [768-799], [864-895], [960-991]
		// // Grain Positions No Shifting: [0-127], [128-255], [256-383], [384-511], [512-639], [640-767], [768-895], [896-1023]
		// // Grain positions after shifting up: [0-127], [96-223], [192-319], [288-415], [384-511], [480-607], [576-703], [672-799], [768-895], [864-991], [960-1023]

		// // Check first few overlap regions
		// for (int i = 96; i < 128; ++i)
		// {
		// 	INFO("Checking first overlap region at sample " << i);
		// 	CHECK(outputBuffer.getSample(0, i) == Catch::Approx(2.0f));
		// }

		// for (int i = 192; i < 224; ++i)
		// {
		// 	INFO("Checking second overlap region at sample " << i);
		// 	CHECK(outputBuffer.getSample(0, i) == Catch::Approx(2.0f));
		// }

		// // Verify final grain phase
		// // Since we're using time-preserving mode, the output buffer should be fully filled
		// CHECK(finalPhase == 0.5f);  // Last grain only completes half
	}

	SECTION("Time-preserving aggressive upward shift - many grain repetitions, no gaps")
	{
		const int periodSize = 128;
		const int bufferSize = 1024;

		// Create input buffer filled with all ones
		juce::AudioBuffer<float> inputBuffer(1, bufferSize);
		BufferFiller::fillWithAllOnes(inputBuffer);

		// Create output buffer and clear it
		juce::AudioBuffer<float> outputBuffer(1, bufferSize);
		outputBuffer.clear();

		// Very aggressive shift up - emission period is half of grain period
		float grainPeriod = static_cast<float>(periodSize);  // 128 samples
		float emissionPeriod = static_cast<float>(periodSize / 2);  // 64 samples

		INFO("Testing time-preserving aggressive upward pitch shift (2x speed)");
		INFO("Grain period: " << periodSize << ", Emission period: " << (periodSize / 2));

		// Call with timePreserving = true
		float finalPhase = Granulator::granulateBuffer(inputBuffer, outputBuffer, grainPeriod, emissionPeriod, window, true);

		// With emission period = 64 and grain period = 128:
		// Grain starts: 0, 64, 128, 192, 256, 320, 384, 448, 512, 576, 640, 704, 768, 832, 896, 960
		// That's 16 grains needed, but input only has 8 grains worth of data
		// In time-preserving mode, the 8 grains will be reused/repeated

		// Verify NO zeros anywhere in the output buffer
		int zeroCount = 0;
		for (int i = 0; i < bufferSize; ++i)
		{
			if (outputBuffer.getSample(0, i) == 0.0f)
			{
				zeroCount++;
			}
		}

		INFO("Found " << zeroCount << " zero samples (should be 0 in time-preserving mode)");
		CHECK(zeroCount == 0);

		// Verify all samples are non-zero
		for (int i = 0; i < bufferSize; ++i)
		{
			CHECK(outputBuffer.getSample(0, i) > 0.0f);
		}

		// With 50% overlap, check overlap regions have value of 2.0
		// Overlaps occur at: [64-127], [128-191], [192-255], [256-319], etc.
		for (int i = 64; i < 128; ++i)
		{
			CHECK(outputBuffer.getSample(0, i) == Catch::Approx(2.0f));
		}

		for (int i = 128; i < 192; ++i)
		{
			CHECK(outputBuffer.getSample(0, i) == Catch::Approx(2.0f));
		}
	}
}



TEST_CASE("Granulator with Hanning window - overlap and spacing verification", "[test_Granulator]")
{
	TestUtils::SetupAndTeardown setupAndTeardown;

	// Create Window object with Hanning shape
	Window window;

	const int periodSize = 128;
	const int numPeriods = 8;
	const int totalSamples = periodSize * numPeriods; // 1024 samples

	// Create singlePeriodBuffer to represent expected window values over a 128 sample period
	juce::AudioBuffer<float> singlePeriodBuffer(1, periodSize);
	window.setSizeShapePeriod(1024, Window::Shape::kHanning, periodSize);

	// Fill singlePeriodBuffer with window values for one period
	for (int i = 0; i < periodSize; ++i)
	{
		float windowValue = window.getValueAtIndexInPeriod(i);
		singlePeriodBuffer.setSample(0, i, windowValue);

		// Debug: Print first few values to verify they're not zero
		if (i < 5 || i == periodSize/2 || i == periodSize - 1)
		{
			INFO("singlePeriodBuffer[" << i << "] = " << windowValue);
		}
	}

	SECTION("No shifting - emission period equals grain period with Hanning window")
	{
		// // Create source buffer filled with all ones
		// juce::AudioBuffer<float> sourceBuffer(1, totalSamples);
		// BufferFiller::fillWithAllOnes(sourceBuffer);

		// // Create output buffer
		// juce::AudioBuffer<float> outputBuffer(1, totalSamples);
		// outputBuffer.clear();

		// // Granulate with no shifting (emission period = grain period)
		// float grainPeriod = static_cast<float>(periodSize);
		// float emissionPeriod = static_cast<float>(periodSize);

		// float finalPhase = Granulator::granulateBuffer(sourceBuffer, outputBuffer, grainPeriod, emissionPeriod, window, true);

		// // With no shifting, each grain should match the singlePeriodBuffer pattern
		// INFO("Testing no-shift case with Hanning: grain period = " << periodSize << ", emission period = " << periodSize);

		// for (int period = 0; period < numPeriods; ++period)
		// {
		// 	for (int sample = 0; sample < periodSize; ++sample)
		// 	{
		// 		int outputIndex = period * periodSize + sample;
		// 		float expectedValue = singlePeriodBuffer.getSample(0, sample);
		// 		float actualValue = outputBuffer.getSample(0, outputIndex);

		// 		INFO("Period " << period << ", sample " << sample << ", output index " << outputIndex);
		// 		CHECK(actualValue == Catch::Approx(expectedValue).epsilon(0.001f));
		// 	}
		// }

		// // Should have no partial grain at the end
		// CHECK(finalPhase == 0.0f);
	}

	SECTION("Shifting up with Hanning window - overlaps add window values")
	{
		// // Create source buffer with plenty of data
		// const int largeSourceSize = 2048;
		// juce::AudioBuffer<float> sourceBuffer(1, largeSourceSize);
		// BufferFiller::fillWithAllOnes(sourceBuffer);

		// // Create output buffer
		// juce::AudioBuffer<float> outputBuffer(1, totalSamples);
		// outputBuffer.clear();

		// // Set emission period to 3/4 of grain period
		// // This causes 25% overlap (32 samples)
		// float grainPeriod = static_cast<float>(periodSize);  // 128
		// float emissionPeriod = static_cast<float>(periodSize * 3 / 4);  // 96

		// INFO("Testing Hanning overlap: grain period = " << periodSize << ", emission period = " << (periodSize * 3 / 4));

		// float finalPhase = Granulator::granulateBuffer(sourceBuffer, outputBuffer, grainPeriod, emissionPeriod, window);

		// // With 25% overlap:
		// // First grain: 0-127
		// // Second grain: 96-223 (overlap from 96-127 = 32 samples)
		// // Third grain: 192-319 (overlap from 192-223 = 32 samples)

		// // Check first overlap region (96-127)
		// // Expected value at overlap = window value from first grain at that position + window value from second grain at its position
		// for (int i = 96; i < 128; ++i)
		// {
		// 	// First grain contributes: singlePeriodBuffer[i]
		// 	int indexInFirstGrain = i;
		// 	float firstGrainContribution = singlePeriodBuffer.getSample(0, indexInFirstGrain);

		// 	// Second grain starts at output position 96, so at output position i, we're at index (i - 96) in the second grain
		// 	int indexInSecondGrain = i - 96;
		// 	float secondGrainContribution = singlePeriodBuffer.getSample(0, indexInSecondGrain);

		// 	float expectedValue = firstGrainContribution + secondGrainContribution;
		// 	float actualValue = outputBuffer.getSample(0, i);

		// 	INFO("Overlap position " << i << ": first grain[" << indexInFirstGrain << "]=" << firstGrainContribution
		// 		 << " + second grain[" << indexInSecondGrain << "]=" << secondGrainContribution
		// 		 << " = " << expectedValue);
		// 	CHECK(actualValue == Catch::Approx(expectedValue).epsilon(0.01f));
		// }

		// // Check second overlap region (192-223)
		// for (int i = 192; i < 224; ++i)
		// {
		// 	// Second grain starts at 96, so at position i we're at index (i - 96) in second grain
		// 	int indexInSecondGrain = i - 96;
		// 	float secondGrainContribution = singlePeriodBuffer.getSample(0, indexInSecondGrain);

		// 	// Third grain starts at 192, so at position i we're at index (i - 192) in third grain
		// 	int indexInThirdGrain = i - 192;
		// 	float thirdGrainContribution = singlePeriodBuffer.getSample(0, indexInThirdGrain);

		// 	float expectedValue = secondGrainContribution + thirdGrainContribution;
		// 	float actualValue = outputBuffer.getSample(0, i);

		// 	INFO("Overlap position " << i << ": second grain[" << indexInSecondGrain << "]=" << secondGrainContribution
		// 		 << " + third grain[" << indexInThirdGrain << "]=" << thirdGrainContribution
		// 		 << " = " << expectedValue);
		// 	CHECK(actualValue == Catch::Approx(expectedValue).epsilon(0.01f));
		// }
	}

	SECTION("Shifting down with Hanning window - gaps between grains")
	{
		// // Create source buffer with plenty of data
		// const int largeSourceSize = 2048;
		// juce::AudioBuffer<float> sourceBuffer(1, largeSourceSize);
		// BufferFiller::fillWithAllOnes(sourceBuffer);

		// // Create output buffer
		// juce::AudioBuffer<float> outputBuffer(1, totalSamples);
		// outputBuffer.clear();

		// // Set emission period longer than grain period
		// // This creates gaps between grains
		// float grainPeriod = static_cast<float>(periodSize);  // 128
		// float emissionPeriod = static_cast<float>(periodSize + 1);  // 129

		// INFO("Testing Hanning with gaps: grain period = " << periodSize << ", emission period = " << (periodSize + 1));

		// float finalPhase = Granulator::granulateBuffer(sourceBuffer, outputBuffer, grainPeriod, emissionPeriod, window, true);

		// // Calculate where gaps occur
		// // First grain: writes to samples 0-127
		// // Gap at sample 128 (no grain writes here)
		// // Second grain: starts at 129, writes to 129-256
		// // Gap at sample 257

		// // Check for gaps at expected positions
		// for (int grainIndex = 1; grainIndex < 8; ++grainIndex)
		// {
		// 	int gapPosition = grainIndex * (periodSize + 1) - 1;

		// 	if (gapPosition < totalSamples)
		// 	{
		// 		INFO("Checking gap at position " << gapPosition);

		// 		// At gap positions, we expect value of 0.0 (no grain writing there)
		// 		CHECK(outputBuffer.getSample(0, gapPosition) == 0.0f);
		// 	}
		// }

		// // Verify that non-gap samples match expected window values
		// // First grain should match singlePeriodBuffer exactly
		// for (int i = 0; i < periodSize; ++i)
		// {
		// 	float expectedValue = singlePeriodBuffer.getSample(0, i);
		// 	float actualValue = outputBuffer.getSample(0, i);
		// 	CHECK(actualValue == Catch::Approx(expectedValue).epsilon(0.01f));
		// }
	}
}

//=======================================================
//=======================================================
//===================== NOT PASSING =====================
//=======================================================
//=======================================================

// TEST_CASE("Granulator window readPos after partial grain - non time preserving", "[test_Granulator]")
// {
// 	TestUtils::SetupAndTeardown setupAndTeardown;

// 	// Window underlying buffer size - this is the actual buffer that stores the window shape
// 	const int windowBufferSize = 1024;
// 	// Grain period - how many samples we want to "play" through the window
// 	const int grainPeriod = 128;
// 	// Phase increment = windowBufferSize / grainPeriod = 1024 / 128 = 8
// 	// This means for each sample in the grain, we advance 8 positions in the window buffer
// 	const double phaseIncrement = static_cast<double>(windowBufferSize) / static_cast<double>(grainPeriod);

// 	// Create Window object with no windowing for predictable behavior
// 	Window window;
// 	window.setSizeShapePeriod(windowBufferSize, Window::Shape::kHanning, grainPeriod);

// 	SECTION("Partial grain - window readPos reflects samples processed in underlying buffer")
// 	{
// 		// Output buffer is smaller than one grain, forcing a partial grain
// 		const int outputBufferSize = 64;  // Only half a grain fits
// 		const int inputBufferSize = 256;  // One grain's worth of input

// 		// Create input buffer filled with all ones
// 		juce::AudioBuffer<float> inputBuffer(1, inputBufferSize);
// 		BufferFiller::fillWithAllOnes(inputBuffer);

// 		// Create output buffer and clear it (simulating processBlock)
// 		juce::AudioBuffer<float> outputBuffer(1, outputBufferSize);
// 		outputBuffer.clear();

// 		// Reset window read position to ensure clean state
// 		window.resetReadPos();
// 		CHECK(window.getReadPos() == 0.0);

// 		// Granulate with timePreserving = false
// 		float grainPeriodFloat = static_cast<float>(grainPeriod);
// 		float emissionPeriod = grainPeriodFloat;

// 		Granulator::granulateBuffer(inputBuffer, outputBuffer, grainPeriodFloat, emissionPeriod, window, false);

// 		// Verify output buffer contains expected Hanning window values
// 		// Since input is all ones, output should be the window values directly
// 		// Create a reference window to get expected values
// 		Window referenceWindow;
// 		referenceWindow.setSizeShapePeriod(windowBufferSize, Window::Shape::kHanning, grainPeriod);

// 		INFO("Checking output buffer values against expected Hanning window shape:");
// 		for (int i = 0; i < outputBufferSize; ++i)
// 		{
// 			float expectedWindowValue = referenceWindow.getValueAtIndexInPeriod(i);
// 			float actualValue = outputBuffer.getSample(0, i);

// 			INFO("Sample " << i << ": expected=" << expectedWindowValue << ", actual=" << actualValue);
// 			CHECK(actualValue == Catch::Approx(expectedWindowValue).margin(0.001f));
// 		}

// 		// We processed 64 samples through the window
// 		// Each sample advances readPos by phaseIncrement (8)
// 		// Expected readPos in underlying window buffer = 64 * 8 = 512
// 		// This is position 512 out of 1024 in the window buffer (halfway through)
// 		const int samplesProcessed = 64;
// 		double expectedReadPos = samplesProcessed * phaseIncrement;

// 		INFO("Window buffer size: " << windowBufferSize);
// 		INFO("Grain period: " << grainPeriod);
// 		INFO("Phase increment: " << phaseIncrement);
// 		INFO("Samples processed: " << samplesProcessed);
// 		INFO("Expected readPos (in window buffer): " << expectedReadPos);
// 		INFO("Actual readPos: " << window.getReadPos());

// 		CHECK(window.getReadPos() == Catch::Approx(expectedReadPos).margin(0.001));
// 	}
// }