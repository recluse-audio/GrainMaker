/**
 * Created by Ryan Devens 2025-10-13
 */

#define _USE_MATH_DEFINES
#include <cmath>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../SOURCE/GRAIN/Granulator.h"
#include "../SUBMODULES/RD/SOURCE/BufferFiller.h"
#include "../SUBMODULES/RD/SOURCE/BufferHelper.h"
#include "../SUBMODULES/RD/SOURCE/Window.h"
#include "../SUBMODULES/RD/TESTS/TEST_UTILS/TestUtils.h"

/**
 * @brief Test helper class to access private members of Granulator
 * Note: makeGrain() and processActiveGrains() are now public, call directly on Granulator
 */
class GranulatorTester
{
public:

};
//=======================================================================================
//=======================================================================================

/**
 * @brief Helper struct to encapsulate common test input parameters for Granulator
 * Provides configurable mock processor input with sensible defaults
 */
struct MockProcessorInput
{
	// Configuration parameters
	double testSampleRate;
	int testBlockSize;
	int testLookaheadSize;

	// Audio buffers
	juce::AudioBuffer<float> lookaheadBuffer;
	juce::AudioBuffer<float> processBuffer;

	// Period parameters
	float detectedPeriod;
	float shiftedPeriod;

	/**
	 * @brief Construct mock input with default values
	 * @param sampleRate Sample rate for processing (default: 44100.0)
	 * @param blockSize Process block size (default: 512)
	 * @param lookaheadSize Lookahead buffer size (default: 2048)
	 * @param period Detected period in samples (default: 128.0f)
	 * @param shiftedPer Shifted period in samples (default: same as period)
	 */
	MockProcessorInput(
		double sampleRate = 48000.0,
		int blockSize = 128,
		int lookaheadSize = 1024,
		float period = 256.0f,
		float shiftedPer = 256.0f
	)
		: testSampleRate(sampleRate)
		, testBlockSize(blockSize)
		, testLookaheadSize(lookaheadSize)
		, lookaheadBuffer(2, lookaheadSize)
		, processBuffer(2, blockSize)
		, detectedPeriod(period)
		, shiftedPeriod(shiftedPer)
	{
		lookaheadBuffer.clear();
		processBuffer.clear();
		BufferFiller::fillWithAllOnes(lookaheadBuffer);
		BufferFiller::fillWithAllOnes(processBuffer);
	}

	/**
	 * @brief Fill buffers with custom values
	 */
	void fillBuffers(float lookaheadValue, float processValue)
	{
		BufferFiller::fillWithValue(lookaheadBuffer, lookaheadValue);
		BufferFiller::fillWithValue(processBuffer, processValue);
	}

	/**
	 * @brief Clear both buffers
	 */
	void clearBuffers()
	{
		lookaheadBuffer.clear();
		processBuffer.clear();
	}
};

//=======================================================================================
//=======================================================================================
// PREPARE TESTS
//=======================================================================================
TEST_CASE("Granulator prepare() initializes all grains as inactive", "[Granulator]")
{
	Granulator granulator;
	granulator.prepare(48000.0, 512, 2048);

	auto& grains = granulator.getGrains();
	for (int i = 0; i < kNumGrains; ++i)
	{
		REQUIRE(grains[i].isActive == false);
	}

    CHECK(granulator.getSynthMark() == -1);
    CHECK(granulator.getWindow().getSize() == 48000);
    CHECK(granulator.getWindow().getReadPos() == 0);
    CHECK(granulator.getWindow().getPeriod() == 2048);
}

//=======================================================================================
//=======================================================================================
// PROCESS TRACKING RANGE TESTS
//=======================================================================================
/**
 * Test processTracking() grain creation with explicit sample indices.
 *
 * Setup (matching PluginProcessor test scenario):
 * - Sample rate: 48000
 * - Block size: 128
 * - Detected period: 256 samples
 * - Shifted period: 256 samples (no pitch shift)
 * - CircularBuffer size: 2048
 *
 * Input ranges (from PluginProcessor after 13th processBlock):
 * - processCounterRange: (1536, 1663)
 * - analysisReadRange: (744, 1000, 1255) - centered on analysisMark 1000
 * - analysisWriteRange: (1536, 1792, 2047) - offset to align with processBlock
 *
 * Expected grain creation:
 * - First grain synthMark = analysisWriteMark = 1792
 * - synthRange = (1792 - 256, 1792, 1792 + 256 - 1) = (1536, 1792, 2047)
 * - analysisRange copied from input: (744, 1000, 1255)
 */
TEST_CASE("Granulator processTracking() creates grain with correct ranges", "[Granulator][processTracking]")
{
	// Test constants matching PluginProcessor test
	constexpr double sampleRate = 48000.0;
	constexpr int blockSize = 128;
	constexpr int circularBufferSize = 2048;
	constexpr float detectedPeriod = 256.0f;
	constexpr float shiftedPeriod = 256.0f;

	// Create and prepare Granulator
	Granulator granulator;
	int maxGrainSize = static_cast<int>(detectedPeriod * 2); // 512
	granulator.prepare(sampleRate, blockSize, maxGrainSize);

	// Create CircularBuffer and fill with test signal
	CircularBuffer circularBuffer;
	circularBuffer.setSize(2, circularBufferSize);
	juce::AudioBuffer<float> sineBuffer(2, circularBufferSize);
	BufferFiller::generateSineCycles(sineBuffer, static_cast<int>(detectedPeriod));
	circularBuffer.pushBuffer(sineBuffer);

	// Create process buffer (output)
	juce::AudioBuffer<float> processBuffer(2, blockSize);
	processBuffer.clear();

	// Input ranges from PluginProcessor scenario
	std::tuple<juce::int64, juce::int64, juce::int64> analysisReadRange = {744, 1000, 1255};
	std::tuple<juce::int64, juce::int64, juce::int64> analysisWriteRange = {1536, 1792, 2047};
	std::tuple<juce::int64, juce::int64> processCounterRange = {1536, 1663};

	// Call processTracking
	granulator.processTracking(processBuffer, circularBuffer,
							   analysisReadRange, analysisWriteRange,
							   processCounterRange,
							   detectedPeriod, shiftedPeriod);

	SECTION("One grain is created and active")
	{
		auto& grains = granulator.getGrains();
		int activeCount = 0;
		for (int i = 0; i < kNumGrains; ++i)
		{
			if (grains[i].isActive)
				activeCount++;
		}
		CHECK(activeCount == 1);
	}

	SECTION("Grain has correct analysis range (744, 1000, 1255)")
	{
		auto& grains = granulator.getGrains();
		// Find the active grain
		for (int i = 0; i < kNumGrains; ++i)
		{
			if (grains[i].isActive)
			{
				auto [start, mark, end] = grains[i].mAnalysisRange;
				CHECK(start == 744);
				CHECK(mark == 1000);
				CHECK(end == 1255);
				break;
			}
		}
	}

	SECTION("Grain has correct synth range (1536, 1792, 2047)")
	{
		auto& grains = granulator.getGrains();
		// Find the active grain
		for (int i = 0; i < kNumGrains; ++i)
		{
			if (grains[i].isActive)
			{
				auto [start, mark, end] = grains[i].mSynthRange;
				// synthStart = synthMark - period = 1792 - 256 = 1536
				// synthEnd = synthMark + period - 1 = 1792 + 256 - 1 = 2047
				CHECK(start == 1536);
				CHECK(mark == 1792);
				CHECK(end == 2047);
				break;
			}
		}
	}

	SECTION("SynthMark is updated to next position (1792 + 256 = 2048)")
	{
		// After creating grain at synthMark 1792, it increments by shiftedPeriod
		CHECK(granulator.getSynthMark() == 2048);
	}
}

//=======================================================================================
//=======================================================================================
// MAKE GRAIN TESTS
//=======================================================================================
/**
 * Test _makeGrain() without windowing (kNone).
 *
 * Setup:
 * - CircularBuffer filled with incremental values (0, 1, 2, 3, ...)
 * - Window shape set to kNone (no windowing applied)
 * - analysisReadRange: (100, 200, 299) - 200 samples total
 * - detectedPeriod: 100 (so grainSize = 200)
 *
 * Expected:
 * - Grain buffer should contain values 100, 101, 102, ... 299
 *   matching the indices from the circular buffer
 */
TEST_CASE("Granulator _makeGrain() copies correct samples with no windowing", "[Granulator][makeGrain]")
{
	constexpr double sampleRate = 48000.0;
	constexpr int blockSize = 128;
	constexpr int circularBufferSize = 512;
	constexpr float detectedPeriod = 100.0f;
	constexpr int grainSize = static_cast<int>(detectedPeriod * 2); // 200

	// Create and prepare Granulator
	Granulator granulator;
	granulator.prepare(sampleRate, blockSize, grainSize);

	// Set window to kNone (no windowing)
	granulator.getWindow().setSizeShapePeriod(static_cast<int>(sampleRate), Window::Shape::kNone, grainSize);

	// Create CircularBuffer and fill with incremental values
	CircularBuffer circularBuffer;
	circularBuffer.setSize(2, circularBufferSize);
	juce::AudioBuffer<float> incrementalBuffer(2, circularBufferSize);
	BufferFiller::fillIncremental(incrementalBuffer);
	circularBuffer.pushBuffer(incrementalBuffer);

	// Define ranges
	// analysisReadRange: read samples 100-299 from circular buffer
	std::tuple<juce::int64, juce::int64, juce::int64> analysisReadRange = {100, 200, 299};
	std::tuple<juce::int64, juce::int64, juce::int64> synthRange = {1000, 1100, 1199};

	// Call _makeGrain through tester
	granulator.makeGrain(circularBuffer, analysisReadRange, synthRange, detectedPeriod, detectedPeriod);

	// Verify grain was created
	auto& grains = granulator.getGrains();
	REQUIRE(grains[0].isActive == true);

	// Verify grain buffer contains the correct values (matching circular buffer indices)
	const auto& grainBuffer = grains[0].getBuffer();
	for (int i = 0; i < grainSize; ++i)
	{
		// Expected value = start index + offset = 100 + i
		float expectedValue = static_cast<float>(100 + i);
		float actualCh0 = grainBuffer.getSample(0, i);
		float actualCh1 = grainBuffer.getSample(1, i);

		if (actualCh0 != expectedValue)
		{
			INFO("Mismatch at index " << i << ", channel 0: expected " << expectedValue << ", got " << actualCh0);
		}
		if (actualCh1 != expectedValue)
		{
			INFO("Mismatch at index " << i << ", channel 1: expected " << expectedValue << ", got " << actualCh1);
		}

		CHECK(actualCh0 == expectedValue);
		CHECK(actualCh1 == expectedValue);
	}
}

/**
 * Test _makeGrain() with Hanning windowing.
 *
 * Setup:
 * - CircularBuffer filled with all ones
 * - Window shape set to kHanning
 * - analysisReadRange: (0, 100, 199) - 200 samples total
 * - detectedPeriod: 100 (so grainSize = 200)
 *
 * Expected:
 * - Grain buffer should contain Hanning window values
 *   since 1.0 * windowValue = windowValue
 */
TEST_CASE("Granulator _makeGrain() applies Hanning window correctly", "[Granulator][makeGrain]")
{
	constexpr double sampleRate = 48000.0;
	constexpr int blockSize = 128;
	constexpr int circularBufferSize = 512;
	constexpr float detectedPeriod = 100.0f;
	constexpr int grainSize = static_cast<int>(detectedPeriod * 2); // 200

	// Create and prepare Granulator
	Granulator granulator;
	granulator.prepare(sampleRate, blockSize, grainSize);

	// Window is kHanning by default from prepare()
	REQUIRE(granulator.getWindow().getShape() == Window::Shape::kHanning);

	// Create CircularBuffer and fill with all ones
	CircularBuffer circularBuffer;
	circularBuffer.setSize(2, circularBufferSize);
	juce::AudioBuffer<float> onesBuffer(2, circularBufferSize);
	BufferFiller::fillWithAllOnes(onesBuffer);
	circularBuffer.pushBuffer(onesBuffer);

	// Define ranges
	std::tuple<juce::int64, juce::int64, juce::int64> analysisReadRange = {0, 100, 199};
	std::tuple<juce::int64, juce::int64, juce::int64> synthRange = {1000, 1100, 1199};

	// Call _makeGrain through tester
	granulator.makeGrain(circularBuffer, analysisReadRange, synthRange, detectedPeriod, detectedPeriod);

	// Verify grain was created
	auto& grains = granulator.getGrains();
	REQUIRE(grains[0].isActive == true);

	// Verify grain buffer contains the Hanning window values
	// Use getValueAtIndexInPeriod() to get expected values from the same Window lookup table
	const auto& grainBuffer = grains[0].getBuffer();
	for (int i = 0; i < grainSize; ++i)
	{
		float expectedValue = granulator.getWindow().getValueAtIndexInPeriod(i);
		CHECK(grainBuffer.getSample(0, i) == Catch::Approx(expectedValue).margin(0.001f));
		CHECK(grainBuffer.getSample(1, i) == Catch::Approx(expectedValue).margin(0.001f));
	}
}

//=======================================================================================
//=======================================================================================
// MULTIPLE PROCESS TRACKING CALLS - OVERLAPPING GRAINS
//=======================================================================================
/**
 * Test processTracking() called multiple times to verify:
 * - Multiple grains can be active simultaneously (overlapping)
 * - SynthMark progresses correctly across calls
 * - Grains are deactivated when their synthRange is fully processed
 *
 * Setup:
 * - Block size: 128 samples
 * - Detected period: 256 samples
 * - Shifted period: 256 samples (no pitch shift)
 * - Grain size: 512 samples (2 * period)
 *
 * Timeline (sample counts):
 *
 * Call 1: processCounterRange (1536, 1663)
 *   - Creates Grain 1: synthRange (1536, 1792, 2047)
 *   - synthMark after: 2048
 *   - Active grains: 1
 *
 * Call 2: processCounterRange (1664, 1791)
 *   - Creates Grain 2: synthRange (1792, 2048, 2303)
 *   - synthMark after: 2304
 *   - Active grains: 2 (both grains overlap in range 1792-2047)
 *
 * Call 3: processCounterRange (1792, 1919)
 *   - Creates Grain 3: synthRange (2048, 2304, 2559)
 *   - synthMark after: 2560
 *   - Active grains: 3
 *
 * Call 4: processCounterRange (1920, 2047)
 *   - No new grain (synthMark 2560 >= nextAnalysisWriteMark)
 *   - Grain 1 deactivated (synthEnd 2047 <= blockEnd 2047)
 *   - Active grains: 2
 */
TEST_CASE("Granulator processTracking() manages multiple overlapping grains", "[Granulator][processTracking]")
{
	constexpr double sampleRate = 48000.0;
	constexpr int blockSize = 128;
	constexpr int circularBufferSize = 4096;
	constexpr float detectedPeriod = 256.0f;
	constexpr float shiftedPeriod = 256.0f;

	// Create and prepare Granulator
	Granulator granulator;
	int maxGrainSize = static_cast<int>(detectedPeriod * 2); // 512
	granulator.prepare(sampleRate, blockSize, maxGrainSize);

	// Create CircularBuffer and fill with test signal
	CircularBuffer circularBuffer;
	circularBuffer.setSize(2, circularBufferSize);
	juce::AudioBuffer<float> sineBuffer(2, circularBufferSize);
	BufferFiller::generateSineCycles(sineBuffer, static_cast<int>(detectedPeriod));
	circularBuffer.pushBuffer(sineBuffer);

	// Create process buffer
	juce::AudioBuffer<float> processBuffer(2, blockSize);

	// Helper lambda to count active grains
	auto countActiveGrains = [&granulator]() {
		int count = 0;
		for (const auto& grain : granulator.getGrains())
		{
			if (grain.isActive)
				count++;
		}
		return count;
	};


	SECTION("Call processTracking() 2x: Second grain created, two grains active (overlapping)")
	{
		// First call
		processBuffer.clear();
		std::tuple<juce::int64, juce::int64, juce::int64> analysisReadRange1 = {744, 1000, 1255};
		std::tuple<juce::int64, juce::int64, juce::int64> analysisWriteRange1 = {1536, 1792, 2047};
		std::tuple<juce::int64, juce::int64> processCounterRange1 = {1536, 1663};

		granulator.processTracking(processBuffer, circularBuffer,
								   analysisReadRange1, analysisWriteRange1,
								   processCounterRange1,
								   detectedPeriod, shiftedPeriod);
		CHECK(countActiveGrains() == 1);
		CHECK(granulator.getSynthMark() == 2048);
        //=====================
        
		// Second call - advance ranges by one block
		processBuffer.clear();
		std::tuple<juce::int64, juce::int64, juce::int64> analysisReadRange2 = {1000, 1256, 1511};
		std::tuple<juce::int64, juce::int64, juce::int64> analysisWriteRange2 = {1792, 2048, 2303};
		std::tuple<juce::int64, juce::int64> processCounterRange2 = {1664, 1791};

		granulator.processTracking(processBuffer, circularBuffer,
								   analysisReadRange2, analysisWriteRange2,
								   processCounterRange2,
								   detectedPeriod, shiftedPeriod);

		CHECK(countActiveGrains() == 2);
		CHECK(granulator.getSynthMark() == 2304);

		// Verify both grains have correct synth ranges
		auto& grains = granulator.getGrains();
		bool foundGrain1 = false;
		bool foundGrain2 = false;
		for (const auto& grain : grains)
		{
			if (!grain.isActive) continue;

			auto [start, mark, end] = grain.mSynthRange;
			if (mark == 1792)
			{
				foundGrain1 = true;
				CHECK(start == 1536);
				CHECK(end == 2047);
			}
			else if (mark == 2048)
			{
				foundGrain2 = true;
				CHECK(start == 1792);
				CHECK(end == 2303);
			}
		}
		CHECK(foundGrain1);
		CHECK(foundGrain2);
	}

	SECTION("Grain deactivated when synthEnd <= blockEnd")
	{
		// Process through multiple blocks until Grain 1 should be deactivated

		// Call 1
		processBuffer.clear();
		granulator.processTracking(processBuffer, circularBuffer,
								   {744, 1000, 1255}, {1536, 1792, 2047},
								   {1536, 1663},
								   detectedPeriod, shiftedPeriod);
		CHECK(countActiveGrains() == 1);

		// Call 2
		processBuffer.clear();
		granulator.processTracking(processBuffer, circularBuffer,
								   {1000, 1256, 1511}, {1792, 2048, 2303},
								   {1664, 1791},
								   detectedPeriod, shiftedPeriod);
		CHECK(countActiveGrains() == 2);

		// Call 3
		processBuffer.clear();
		granulator.processTracking(processBuffer, circularBuffer,
								   {1256, 1512, 1767}, {2048, 2304, 2559},
								   {1792, 1919},
								   detectedPeriod, shiftedPeriod);
		CHECK(countActiveGrains() == 3);

		// Call 4 - Grain 1 (synthEnd = 2047) should be deactivated
		processBuffer.clear();
		granulator.processTracking(processBuffer, circularBuffer,
								   {1512, 1768, 2023}, {2304, 2560, 2815},
								   {1920, 2047},
								   detectedPeriod, shiftedPeriod);

		// Grain 1 should now be inactive (synthEnd 2047 <= blockEnd 2047)
		CHECK(countActiveGrains() == 3); // Grain 1 done, but Grain 4 created
		CHECK(granulator.getSynthMark() == 2816);
	}
}

//=======================================================================================
//=======================================================================================
// PITCH SHIFT UP - SHORTER SHIFTED PERIOD
//=======================================================================================
/**
 * Test processTracking() with shiftedPeriod < detectedPeriod (pitch shift UP).
 *
 * When shifting pitch up, we emit grains more frequently than we analyze them.
 * Multiple grains created in a single processTracking call should share the
 * same analysisReadRange (they read the same source audio).
 *
 * Setup:
 * - Detected period: 256 samples
 * - Shifted period: 192 samples (detectedPeriod - 64)
 * - Grain size: 512 samples (2 * detectedPeriod)
 *
 * Call 1: analysisWriteMark = 1792
 *   - nextAnalysisWriteMark = 1792 + 256 = 2048
 *   - Grain 1: synthMark = 1792, synthRange = (1536, 1792, 2047)
 *   - synthMark = 1792 + 192 = 1984
 *   - 1984 < 2048, so create another grain
 *   - Grain 2: synthMark = 1984, synthRange = (1728, 1984, 2239)
 *   - synthMark = 1984 + 192 = 2176
 *   - 2176 >= 2048, exit loop
 *
 * Call 2: analysisWriteMark = 2048
 *   - nextAnalysisWriteMark = 2048 + 256 = 2304
 *   - synthMark = 2176
 *   - Grain 3: synthMark = 2176, synthRange = (1920, 2176, 2431)
 *   - synthMark = 2176 + 192 = 2368
 *   - 2368 >= 2304, exit loop
 *
 * Both grains in Call 1 should have the SAME analysisReadRange since they
 * come from the same analysis window.
 */
TEST_CASE("Granulator processTracking() with pitch shift up creates multiple grains per call", "[Granulator][processTracking][pitchShift]")
{
	constexpr double sampleRate = 48000.0;
	constexpr int blockSize = 128;
	constexpr int circularBufferSize = 4096;
	constexpr float detectedPeriod = 256.0f;
	constexpr float shiftedPeriod = 192.0f; // Pitch shift UP (detectedPeriod - 64)

	// Create and prepare Granulator
	Granulator granulator;
	int maxGrainSize = static_cast<int>(detectedPeriod * 2); // 512
	granulator.prepare(sampleRate, blockSize, maxGrainSize);

	// Create CircularBuffer and fill with test signal
	CircularBuffer circularBuffer;
	circularBuffer.setSize(2, circularBufferSize);
	juce::AudioBuffer<float> sineBuffer(2, circularBufferSize);
	BufferFiller::generateSineCycles(sineBuffer, static_cast<int>(detectedPeriod));
	circularBuffer.pushBuffer(sineBuffer);

	// Create process buffer
	juce::AudioBuffer<float> processBuffer(2, blockSize);

	// Helper lambda to count active grains
	auto countActiveGrains = [&granulator]() {
		int count = 0;
		for (const auto& grain : granulator.getGrains())
		{
			if (grain.isActive)
				count++;
		}
		return count;
	};

	SECTION("Single call creates 2 grains with same analysisRange")
	{
		processBuffer.clear();
		std::tuple<juce::int64, juce::int64, juce::int64> analysisReadRange = {744, 1000, 1255};
		std::tuple<juce::int64, juce::int64, juce::int64> analysisWriteRange = {1536, 1792, 2047};
		std::tuple<juce::int64, juce::int64> processCounterRange = {1536, 1663};

		granulator.processTracking(processBuffer, circularBuffer,
								   analysisReadRange, analysisWriteRange,
								   processCounterRange,
								   detectedPeriod, shiftedPeriod);

		// Should have 2 grains from single call
		CHECK(countActiveGrains() == 2);

		// synthMark should be 2176 (1792 + 192 + 192)
		CHECK(granulator.getSynthMark() == 2176);

		// Verify both grains share the same analysisReadRange
		auto& grains = granulator.getGrains();
		bool foundGrain1 = false;
		bool foundGrain2 = false;

		for (const auto& grain : grains)
		{
			if (!grain.isActive) continue;

			// Both grains should have the same analysis range
			auto [aStart, aMark, aEnd] = grain.mAnalysisRange;
			CHECK(aStart == 744);
			CHECK(aMark == 1000);
			CHECK(aEnd == 1255);

			auto [sStart, sMark, sEnd] = grain.mSynthRange;
			if (sMark == 1792)
			{
				foundGrain1 = true;
				CHECK(sStart == 1536);
				CHECK(sEnd == 2047);
			}
			else if (sMark == 1984)
			{
				foundGrain2 = true;
				CHECK(sStart == 1728);
				CHECK(sEnd == 2239);
			}
		}

		CHECK(foundGrain1);
		CHECK(foundGrain2);
	}

	SECTION("Second call creates 1 more grain, total 3 active")
	{
		// First call - creates 2 grains
		processBuffer.clear();
		granulator.processTracking(processBuffer, circularBuffer,
								   {744, 1000, 1255}, {1536, 1792, 2047},
								   {1536, 1663},
								   detectedPeriod, shiftedPeriod);
		CHECK(countActiveGrains() == 2);
		CHECK(granulator.getSynthMark() == 2176);

		// Second call - advance ranges
		// synthMark = 2176, nextAnalysisWriteMark = 2048 + 256 = 2304
		// 2176 < 2304, so one grain created
		// Grain 3: synthMark = 2176, synthRange = (1920, 2176, 2431)
		// synthMark = 2176 + 192 = 2368 >= 2304, exit loop
		processBuffer.clear();
		std::tuple<juce::int64, juce::int64, juce::int64> analysisReadRange2 = {1000, 1256, 1511};
		std::tuple<juce::int64, juce::int64, juce::int64> analysisWriteRange2 = {1792, 2048, 2303};
		std::tuple<juce::int64, juce::int64> processCounterRange2 = {1664, 1791};

		granulator.processTracking(processBuffer, circularBuffer,
								   analysisReadRange2, analysisWriteRange2,
								   processCounterRange2,
								   detectedPeriod, shiftedPeriod);

		// Should now have 3 grains (2 from first call, 1 from second call)
		CHECK(countActiveGrains() == 3);

		// synthMark should be 2368 (2176 + 192)
		CHECK(granulator.getSynthMark() == 2368);

		// Verify grain from second call has the new analysisReadRange
		auto& grains = granulator.getGrains();
		int grainsWithNewAnalysisRange = 0;

		for (const auto& grain : grains)
		{
			if (!grain.isActive) continue;

			auto [aStart, aMark, aEnd] = grain.mAnalysisRange;
			if (aStart == 1000 && aMark == 1256 && aEnd == 1511)
			{
				grainsWithNewAnalysisRange++;

				// Verify this grain's synth range
				auto [sStart, sMark, sEnd] = grain.mSynthRange;
				CHECK(sMark == 2176);
				CHECK(sStart == 1920);
				CHECK(sEnd == 2431);
			}
		}

		// 1 grain from second call should have the new analysis range
		CHECK(grainsWithNewAnalysisRange == 1);
	}
}

//=======================================================================================
//=======================================================================================
// GRAIN BUFFER VERIFICATION AROUND 4096 BOUNDARY
//=======================================================================================
/**
 * Test that grain buffers are filled correctly when sample indices approach 4096.
 *
 * The circular buffer is 2048 samples. At sample ~4096 (2 * buffer size), we've
 * wrapped around twice. This test verifies that:
 * 1. makeGrain correctly reads from the circular buffer when indices wrap
 * 2. The grain buffer contains the expected audio data
 * 3. Grains are created at the expected times
 *
 * This tests the specific scenario where audio dropout was occurring.
 */
TEST_CASE("Granulator makeGrain() fills grain buffer correctly near 4096 sample boundary", "[Granulator][makeGrain][boundary]")
{
	constexpr double sampleRate = 48000.0;
	constexpr int blockSize = 128;
	constexpr int circularBufferSize = 2048;
	constexpr float detectedPeriod = 256.0f;
	constexpr int grainSize = static_cast<int>(detectedPeriod * 2); // 512

	// Create and prepare Granulator
	Granulator granulator;
	granulator.prepare(sampleRate, blockSize, grainSize);

	// Create CircularBuffer and fill with incremental values so we can verify reads
	CircularBuffer circularBuffer;
	circularBuffer.setSize(2, circularBufferSize);

	// Fill buffer with values that encode the sample index modulo buffer size
	// This lets us verify which positions are being read
	juce::AudioBuffer<float> incrementalBuffer(2, circularBufferSize);
	for (int i = 0; i < circularBufferSize; ++i)
	{
		float value = static_cast<float>(i);
		incrementalBuffer.setSample(0, i, value);
		incrementalBuffer.setSample(1, i, value);
	}
	circularBuffer.pushBuffer(incrementalBuffer);

	// Use no windowing so we can verify exact values
	granulator.getWindow().setSizeShapePeriod(static_cast<int>(sampleRate), Window::Shape::kNone, grainSize);

	SECTION("makeGrain at sample 3200 (before 4096 boundary)")
	{
		// analysisReadRange centered at 3200: (3200-256, 3200, 3200+255) = (2944, 3200, 3455)
		// These wrap to buffer indices: (896, 1152, 1407)
		std::tuple<juce::int64, juce::int64, juce::int64> analysisReadRange = {2944, 3200, 3455};
		std::tuple<juce::int64, juce::int64, juce::int64> synthRange = {3456, 3712, 3967};

		granulator.makeGrain(circularBuffer, analysisReadRange, synthRange, detectedPeriod, detectedPeriod);

		// Verify grain was created
		auto& grains = granulator.getGrains();
		REQUIRE(grains[0].isActive == true);

		// Verify grain buffer contains values matching wrapped circular buffer indices
		const auto& grainBuffer = grains[0].getBuffer();
		bool allCorrect = true;
		for (int i = 0; i < grainSize && allCorrect; ++i)
		{
			juce::int64 readIndex = 2944 + i;
			int expectedWrappedIndex = static_cast<int>(readIndex % circularBufferSize);
			float expectedValue = static_cast<float>(expectedWrappedIndex);
			float actualValue = grainBuffer.getSample(0, i);

			if (actualValue != expectedValue)
			{
				INFO("Grain buffer mismatch at grain index " << i
					 << ": readIndex=" << readIndex
					 << ", expectedWrappedIndex=" << expectedWrappedIndex
					 << ", expected=" << expectedValue
					 << ", actual=" << actualValue);
				allCorrect = false;
			}
		}
		CHECK(allCorrect);
	}

	SECTION("makeGrain at sample 3900 (range spans 4096 boundary)")
	{
		// analysisReadRange centered at 3900: (3900-256, 3900, 3900+255) = (3644, 3900, 4155)
		// 3644 % 2048 = 1596
		// 3900 % 2048 = 1852
		// 4155 % 2048 = 59  (wraps around!)
		std::tuple<juce::int64, juce::int64, juce::int64> analysisReadRange = {3644, 3900, 4155};
		std::tuple<juce::int64, juce::int64, juce::int64> synthRange = {4156, 4412, 4667};

		granulator.makeGrain(circularBuffer, analysisReadRange, synthRange, detectedPeriod, detectedPeriod);

		// Verify grain was created
		auto& grains = granulator.getGrains();
		REQUIRE(grains[0].isActive == true);

		// Verify grain buffer handles the wrap correctly
		const auto& grainBuffer = grains[0].getBuffer();
		bool allCorrect = true;
		int mismatchCount = 0;
		for (int i = 0; i < grainSize; ++i)
		{
			juce::int64 readIndex = 3644 + i;
			int expectedWrappedIndex = static_cast<int>(((readIndex % circularBufferSize) + circularBufferSize) % circularBufferSize);
			float expectedValue = static_cast<float>(expectedWrappedIndex);
			float actualValue = grainBuffer.getSample(0, i);

			if (actualValue != expectedValue)
			{
				if (mismatchCount < 5) // Only print first 5 mismatches
				{
					INFO("Grain buffer mismatch at grain index " << i
						 << ": readIndex=" << readIndex
						 << ", expectedWrappedIndex=" << expectedWrappedIndex
						 << ", expected=" << expectedValue
						 << ", actual=" << actualValue);
				}
				allCorrect = false;
				mismatchCount++;
			}
		}
		if (mismatchCount > 0)
		{
			INFO("Total mismatches: " << mismatchCount << " / " << grainSize);
		}
		CHECK(allCorrect);
	}

	SECTION("makeGrain at sample 4100 (entirely past 4096 boundary)")
	{
		// analysisReadRange centered at 4100: (4100-256, 4100, 4100+255) = (3844, 4100, 4355)
		// 3844 % 2048 = 1796
		// 4100 % 2048 = 4
		// 4355 % 2048 = 259
		std::tuple<juce::int64, juce::int64, juce::int64> analysisReadRange = {3844, 4100, 4355};
		std::tuple<juce::int64, juce::int64, juce::int64> synthRange = {4356, 4612, 4867};

		granulator.makeGrain(circularBuffer, analysisReadRange, synthRange, detectedPeriod, detectedPeriod);

		auto& grains = granulator.getGrains();
		REQUIRE(grains[0].isActive == true);

		const auto& grainBuffer = grains[0].getBuffer();
		bool allCorrect = true;
		int mismatchCount = 0;
		for (int i = 0; i < grainSize; ++i)
		{
			juce::int64 readIndex = 3844 + i;
			int expectedWrappedIndex = static_cast<int>(((readIndex % circularBufferSize) + circularBufferSize) % circularBufferSize);
			float expectedValue = static_cast<float>(expectedWrappedIndex);
			float actualValue = grainBuffer.getSample(0, i);

			if (actualValue != expectedValue)
			{
				if (mismatchCount < 5)
				{
					INFO("Grain buffer mismatch at grain index " << i
						 << ": readIndex=" << readIndex
						 << ", expectedWrappedIndex=" << expectedWrappedIndex
						 << ", expected=" << expectedValue
						 << ", actual=" << actualValue);
				}
				allCorrect = false;
				mismatchCount++;
			}
		}
		if (mismatchCount > 0)
		{
			INFO("Total mismatches: " << mismatchCount << " / " << grainSize);
		}
		CHECK(allCorrect);
	}
}

/**
 * Test that processTracking creates grains at the correct times around the 4096 boundary.
 *
 * This verifies that the grain emission logic works correctly when sample counts
 * approach and pass 2 * circular buffer size.
 */
TEST_CASE("Granulator processTracking() creates grains correctly near 4096 boundary", "[Granulator][processTracking][boundary]")
{
	constexpr double sampleRate = 48000.0;
	constexpr int blockSize = 128;
	constexpr int circularBufferSize = 2048;
	constexpr float detectedPeriod = 256.0f;
	constexpr float shiftedPeriod = 256.0f;
	constexpr int grainSize = static_cast<int>(detectedPeriod * 2);

	Granulator granulator;
	granulator.prepare(sampleRate, blockSize, grainSize);

	CircularBuffer circularBuffer;
	circularBuffer.setSize(2, circularBufferSize);
	juce::AudioBuffer<float> sineBuffer(2, circularBufferSize);
	BufferFiller::generateSineCycles(sineBuffer, static_cast<int>(detectedPeriod));
	circularBuffer.pushBuffer(sineBuffer);

	juce::AudioBuffer<float> processBuffer(2, blockSize);

	auto countActiveGrains = [&granulator]() {
		int count = 0;
		for (const auto& grain : granulator.getGrains())
			if (grain.isActive) count++;
		return count;
	};

	SECTION("Track grain creation across multiple blocks spanning 4096")
	{
		// Simulate processing from block 28 to block 35 (samples 3456 to 4480)
		// This spans the critical 4096 boundary

		std::vector<int> grainCountPerBlock;
		std::vector<juce::int64> synthMarksPerBlock;

		for (int block = 28; block <= 35; ++block)
		{
			processBuffer.clear();

			// Calculate ranges as PluginProcessor would
			juce::int64 samplesProcessed = (block - 1) * blockSize;
			juce::int64 processStart = samplesProcessed;
			juce::int64 processEnd = samplesProcessed + blockSize - 1;

			// Detection happens at delayed position
			juce::int64 detectionEnd = processEnd - 512; // minLookaheadSize
			juce::int64 detectionStart = detectionEnd - 1024; // minDetectionSize

			// Mock analysis mark somewhere in detection range
			juce::int64 analysisMark = detectionEnd - 128; // Arbitrary position
			juce::int64 analysisStart = analysisMark - static_cast<juce::int64>(detectedPeriod);
			juce::int64 analysisEnd = analysisMark + static_cast<juce::int64>(detectedPeriod) - 1;

			// Write range = read range + lookahead
			juce::int64 writeStart = analysisStart + 512;
			juce::int64 writeMark = analysisMark + 512;
			juce::int64 writeEnd = analysisEnd + 512;

			std::tuple<juce::int64, juce::int64, juce::int64> analysisReadRange = {analysisStart, analysisMark, analysisEnd};
			std::tuple<juce::int64, juce::int64, juce::int64> analysisWriteRange = {writeStart, writeMark, writeEnd};
			std::tuple<juce::int64, juce::int64> processCounterRange = {processStart, processEnd};

			granulator.processTracking(processBuffer, circularBuffer,
									   analysisReadRange, analysisWriteRange,
									   processCounterRange,
									   detectedPeriod, shiftedPeriod);

			grainCountPerBlock.push_back(countActiveGrains());
			synthMarksPerBlock.push_back(granulator.getSynthMark());

			INFO("Block " << block << " (samples " << processStart << "-" << processEnd << "):"
				 << " activeGrains=" << countActiveGrains()
				 << " synthMark=" << granulator.getSynthMark()
				 << " writeMark=" << writeMark);
		}

		// Verify grains are being created consistently
		// Should have at least 1 grain active after each block
		for (size_t i = 0; i < grainCountPerBlock.size(); ++i)
		{
			INFO("Block " << (28 + i) << " had " << grainCountPerBlock[i] << " active grains");
			CHECK(grainCountPerBlock[i] >= 1);
		}

		// Verify synthMark is progressing (not stuck or jumping backwards)
		for (size_t i = 1; i < synthMarksPerBlock.size(); ++i)
		{
			INFO("synthMark progression: " << synthMarksPerBlock[i-1] << " -> " << synthMarksPerBlock[i]);
			CHECK(synthMarksPerBlock[i] >= synthMarksPerBlock[i-1]);
		}
	}
}

//=======================================================================================
//=======================================================================================
// PROCESS ACTIVE GRAINS TESTS
//=======================================================================================
/**
 * Test processActiveGrains() with no active grains.
 *
 * When no grains are active, the processBlock should remain unchanged.
 */
TEST_CASE("Granulator processActiveGrains() with no active grains leaves buffer unchanged", "[Granulator][processActiveGrains]")
{
	constexpr double sampleRate = 48000.0;
	constexpr int blockSize = 128;
	constexpr int maxGrainSize = 512;

	// Create and prepare Granulator
	Granulator granulator;
	granulator.prepare(sampleRate, blockSize, maxGrainSize);

	// Verify no grains are active after prepare
	auto& grains = granulator.getGrains();
	for (int i = 0; i < kNumGrains; ++i)
	{
		REQUIRE(grains[i].isActive == false);
	}

	// Create process buffer and fill with zeros
	juce::AudioBuffer<float> processBuffer(2, blockSize);
	processBuffer.clear();

	// Call processActiveGrains with no active grains
	std::tuple<juce::int64, juce::int64> processCounterRange = {0, blockSize - 1};
	granulator.processActiveGrains(processBuffer, processCounterRange);

	// Verify buffer is still all zeros
	for (int ch = 0; ch < 2; ++ch)
	{
		for (int i = 0; i < blockSize; ++i)
		{
			CHECK(processBuffer.getSample(ch, i) == 0.0f);
		}
	}
}

/**
 * Test processActiveGrains() with a single active grain.
 *
 * Verifies that a single grain correctly writes its windowed samples
 * to the process buffer at the correct positions.
 */
TEST_CASE("Granulator processActiveGrains() with single active grain", "[Granulator][processActiveGrains]")
{
	constexpr double sampleRate = 48000.0;
	constexpr int blockSize = 128;
	constexpr int circularBufferSize = 2048;
	constexpr float detectedPeriod = 256.0f;
	constexpr int grainSize = static_cast<int>(detectedPeriod * 2); // 512

	Granulator granulator;
	granulator.prepare(sampleRate, blockSize, grainSize);

	// Use no windowing for predictable values
	granulator.getWindow().setSizeShapePeriod(static_cast<int>(sampleRate), Window::Shape::kNone, grainSize);

	// Create CircularBuffer with all ones
	CircularBuffer circularBuffer;
	circularBuffer.setSize(2, circularBufferSize);
	juce::AudioBuffer<float> onesBuffer(2, circularBufferSize);
	BufferFiller::fillWithAllOnes(onesBuffer);
	circularBuffer.pushBuffer(onesBuffer);

	// Create a grain manually
	std::tuple<juce::int64, juce::int64, juce::int64> analysisReadRange = {0, 256, 511};
	std::tuple<juce::int64, juce::int64, juce::int64> synthRange = {100, 356, 611}; // Grain spans samples 100-611

	granulator.makeGrain(circularBuffer, analysisReadRange, synthRange, detectedPeriod, detectedPeriod);

	auto& grains = granulator.getGrains();
	REQUIRE(grains[0].isActive == true);

	SECTION("Block fully within grain: processCounterRange (200, 327)")
	{
		juce::AudioBuffer<float> processBuffer(2, blockSize);
		processBuffer.clear();

		std::tuple<juce::int64, juce::int64> processCounterRange = {200, 327};
		granulator.processActiveGrains(processBuffer, processCounterRange);

		// All samples should have grain data written (value 1.0 with no window)
		int nonZeroCount = 0;
		for (int i = 0; i < blockSize; ++i)
		{
			if (processBuffer.getSample(0, i) != 0.0f)
				nonZeroCount++;
		}
		CHECK(nonZeroCount == blockSize);

		// Grain should still be active (not fully processed)
		CHECK(grains[0].isActive == true);
	}
}

/**
 * Test processActiveGrains() when grain starts before block and ends inside it.
 *
 * Grain: synthRange (100, 356, 611) - spans samples 100-611
 * Block: processCounterRange (500, 627) - spans samples 500-627
 *
 * Expected: Grain ends at 611, block ends at 627
 * - Samples 500-611 should have grain data (112 samples)
 * - Samples 612-627 should be zero (16 samples)
 * - Grain should be deactivated after processing
 */
TEST_CASE("Granulator processActiveGrains() grain ends mid-block", "[Granulator][processActiveGrains]")
{
	constexpr double sampleRate = 48000.0;
	constexpr int blockSize = 128;
	constexpr int circularBufferSize = 2048;
	constexpr float detectedPeriod = 256.0f;
	constexpr int grainSize = static_cast<int>(detectedPeriod * 2); // 512

	Granulator granulator;
	granulator.prepare(sampleRate, blockSize, grainSize);
	granulator.getWindow().setSizeShapePeriod(static_cast<int>(sampleRate), Window::Shape::kNone, grainSize);

	CircularBuffer circularBuffer;
	circularBuffer.setSize(2, circularBufferSize);
	juce::AudioBuffer<float> onesBuffer(2, circularBufferSize);
	BufferFiller::fillWithAllOnes(onesBuffer);
	circularBuffer.pushBuffer(onesBuffer);

	// Grain spans samples 100-611
	std::tuple<juce::int64, juce::int64, juce::int64> analysisReadRange = {0, 256, 511};
	std::tuple<juce::int64, juce::int64, juce::int64> synthRange = {100, 356, 611};

	granulator.makeGrain(circularBuffer, analysisReadRange, synthRange, detectedPeriod, detectedPeriod);

	auto& grains = granulator.getGrains();
	REQUIRE(grains[0].isActive == true);

	juce::AudioBuffer<float> processBuffer(2, blockSize);
	processBuffer.clear();

	// Block spans 500-627, grain ends at 611
	std::tuple<juce::int64, juce::int64> processCounterRange = {500, 627};
	granulator.processActiveGrains(processBuffer, processCounterRange);

	// Count samples with grain data
	// Overlap is samples 500-611 = 112 samples (indices 0-111 in processBuffer)
	int samplesWithData = 0;
	int samplesWithoutData = 0;
	for (int i = 0; i < blockSize; ++i)
	{
		juce::int64 sampleIndex = 500 + i;
		if (sampleIndex <= 611)
		{
			// Should have grain data
			if (processBuffer.getSample(0, i) != 0.0f)
				samplesWithData++;
		}
		else
		{
			// Should be zero
			if (processBuffer.getSample(0, i) == 0.0f)
				samplesWithoutData++;
		}
	}

	INFO("Samples with data: " << samplesWithData << " (expected 112)");
	INFO("Samples without data: " << samplesWithoutData << " (expected 16)");
	CHECK(samplesWithData == 112);
	CHECK(samplesWithoutData == 16);

	// Grain should be deactivated (synthEnd 611 <= blockEnd 627)
	CHECK(grains[0].isActive == false);
}

/**
 * Test processActiveGrains() when grain starts mid-block and continues past it.
 *
 * Grain: synthRange (550, 806, 1061) - spans samples 550-1061
 * Block: processCounterRange (500, 627) - spans samples 500-627
 *
 * Expected: Grain starts at 550, block starts at 500
 * - Samples 500-549 should be zero (50 samples)
 * - Samples 550-627 should have grain data (78 samples)
 * - Grain should remain active (continues past block)
 */
TEST_CASE("Granulator processActiveGrains() grain starts mid-block", "[Granulator][processActiveGrains]")
{
	constexpr double sampleRate = 48000.0;
	constexpr int blockSize = 128;
	constexpr int circularBufferSize = 2048;
	constexpr float detectedPeriod = 256.0f;
	constexpr int grainSize = static_cast<int>(detectedPeriod * 2); // 512

	Granulator granulator;
	granulator.prepare(sampleRate, blockSize, grainSize);
	granulator.getWindow().setSizeShapePeriod(static_cast<int>(sampleRate), Window::Shape::kNone, grainSize);

	CircularBuffer circularBuffer;
	circularBuffer.setSize(2, circularBufferSize);
	juce::AudioBuffer<float> onesBuffer(2, circularBufferSize);
	BufferFiller::fillWithAllOnes(onesBuffer);
	circularBuffer.pushBuffer(onesBuffer);

	// Grain spans samples 550-1061
	std::tuple<juce::int64, juce::int64, juce::int64> analysisReadRange = {0, 256, 511};
	std::tuple<juce::int64, juce::int64, juce::int64> synthRange = {550, 806, 1061};

	granulator.makeGrain(circularBuffer, analysisReadRange, synthRange, detectedPeriod, detectedPeriod);

	auto& grains = granulator.getGrains();
	REQUIRE(grains[0].isActive == true);

	juce::AudioBuffer<float> processBuffer(2, blockSize);
	processBuffer.clear();

	// Block spans 500-627, grain starts at 550
	std::tuple<juce::int64, juce::int64> processCounterRange = {500, 627};
	granulator.processActiveGrains(processBuffer, processCounterRange);

	// Count samples
	// Before grain (500-549): 50 samples should be zero
	// Overlap (550-627): 78 samples should have data
	int samplesBeforeGrain = 0;
	int samplesWithData = 0;
	for (int i = 0; i < blockSize; ++i)
	{
		juce::int64 sampleIndex = 500 + i;
		if (sampleIndex < 550)
		{
			if (processBuffer.getSample(0, i) == 0.0f)
				samplesBeforeGrain++;
		}
		else
		{
			if (processBuffer.getSample(0, i) != 0.0f)
				samplesWithData++;
		}
	}

	INFO("Samples before grain (zero): " << samplesBeforeGrain << " (expected 50)");
	INFO("Samples with data: " << samplesWithData << " (expected 78)");
	CHECK(samplesBeforeGrain == 50);
	CHECK(samplesWithData == 78);

	// Grain should remain active (synthEnd 1061 > blockEnd 627)
	CHECK(grains[0].isActive == true);
}

/**
 * Test processActiveGrains() when grain starts and ends within one block.
 *
 * Using a smaller grain size to fit within one block.
 * Grain: synthRange (520, 584, 647) - spans samples 520-647 (128 samples, using period 64)
 * Block: processCounterRange (500, 700) - spans samples 500-700 (201 samples)
 *
 * Expected:
 * - Samples 500-519 should be zero (20 samples)
 * - Samples 520-647 should have grain data (128 samples)
 * - Samples 648-700 should be zero (53 samples)
 * - Grain should be deactivated
 */
TEST_CASE("Granulator processActiveGrains() grain starts and ends within block", "[Granulator][processActiveGrains]")
{
	constexpr double sampleRate = 48000.0;
	constexpr int blockSize = 201; // Larger block to contain entire grain
	constexpr int circularBufferSize = 2048;
	constexpr float detectedPeriod = 64.0f; // Smaller period for smaller grain
	constexpr int grainSize = static_cast<int>(detectedPeriod * 2); // 128

	Granulator granulator;
	granulator.prepare(sampleRate, blockSize, grainSize);
	granulator.getWindow().setSizeShapePeriod(static_cast<int>(sampleRate), Window::Shape::kNone, grainSize);

	CircularBuffer circularBuffer;
	circularBuffer.setSize(2, circularBufferSize);
	juce::AudioBuffer<float> onesBuffer(2, circularBufferSize);
	BufferFiller::fillWithAllOnes(onesBuffer);
	circularBuffer.pushBuffer(onesBuffer);

	// Grain spans samples 520-647 (128 samples)
	std::tuple<juce::int64, juce::int64, juce::int64> analysisReadRange = {0, 64, 127};
	std::tuple<juce::int64, juce::int64, juce::int64> synthRange = {520, 584, 647};

	granulator.makeGrain(circularBuffer, analysisReadRange, synthRange, detectedPeriod, detectedPeriod);

	auto& grains = granulator.getGrains();
	REQUIRE(grains[0].isActive == true);

	juce::AudioBuffer<float> processBuffer(2, blockSize);
	processBuffer.clear();

	// Block spans 500-700
	std::tuple<juce::int64, juce::int64> processCounterRange = {500, 700};
	granulator.processActiveGrains(processBuffer, processCounterRange);

	// Count samples in each region
	int samplesBeforeGrain = 0;  // 500-519
	int samplesWithData = 0;     // 520-647
	int samplesAfterGrain = 0;   // 648-700

	for (int i = 0; i < blockSize; ++i)
	{
		juce::int64 sampleIndex = 500 + i;
		float sample = processBuffer.getSample(0, i);

		if (sampleIndex < 520)
		{
			if (sample == 0.0f) samplesBeforeGrain++;
		}
		else if (sampleIndex <= 647)
		{
			if (sample != 0.0f) samplesWithData++;
		}
		else
		{
			if (sample == 0.0f) samplesAfterGrain++;
		}
	}

	INFO("Samples before grain (zero): " << samplesBeforeGrain << " (expected 20)");
	INFO("Samples with data: " << samplesWithData << " (expected 128)");
	INFO("Samples after grain (zero): " << samplesAfterGrain << " (expected 53)");
	CHECK(samplesBeforeGrain == 20);
	CHECK(samplesWithData == 128);
	CHECK(samplesAfterGrain == 53);

	// Grain should be deactivated
	CHECK(grains[0].isActive == false);
}

/**
 * Test processActiveGrains() with two overlapping grains.
 *
 * Grain 1: synthRange (100, 356, 611) - spans samples 100-611
 * Grain 2: synthRange (300, 556, 811) - spans samples 300-811
 * Block: processCounterRange (400, 527) - spans samples 400-527
 *
 * Both grains overlap with the block. Output should be the sum of both grains.
 */
TEST_CASE("Granulator processActiveGrains() with overlapping grains", "[Granulator][processActiveGrains]")
{
	constexpr double sampleRate = 48000.0;
	constexpr int blockSize = 128;
	constexpr int circularBufferSize = 2048;
	constexpr float detectedPeriod = 256.0f;
	constexpr int grainSize = static_cast<int>(detectedPeriod * 2); // 512

	Granulator granulator;
	granulator.prepare(sampleRate, blockSize, grainSize);
	granulator.getWindow().setSizeShapePeriod(static_cast<int>(sampleRate), Window::Shape::kNone, grainSize);

	CircularBuffer circularBuffer;
	circularBuffer.setSize(2, circularBufferSize);
	juce::AudioBuffer<float> onesBuffer(2, circularBufferSize);
	BufferFiller::fillWithAllOnes(onesBuffer);
	circularBuffer.pushBuffer(onesBuffer);

	// Create two overlapping grains
	// Grain 1: spans 100-611
	granulator.makeGrain(circularBuffer,
						 {0, 256, 511},
						 {100, 356, 611},
						 detectedPeriod,
						 detectedPeriod);

	// Grain 2: spans 300-811
	granulator.makeGrain(circularBuffer,
						 {0, 256, 511},
						 {300, 556, 811},
						 detectedPeriod,
						 detectedPeriod);

	auto& grains = granulator.getGrains();
	int activeCount = 0;
	for (int i = 0; i < kNumGrains; ++i)
		if (grains[i].isActive) activeCount++;
	REQUIRE(activeCount == 2);

	juce::AudioBuffer<float> processBuffer(2, blockSize);
	processBuffer.clear();

	// Block spans 400-527, both grains overlap entirely
	std::tuple<juce::int64, juce::int64> processCounterRange = {400, 527};
	granulator.processActiveGrains(processBuffer, processCounterRange);

	// With two grains of all-ones and no windowing, overlap-add should give 2.0
	int samplesWithOverlap = 0;
	for (int i = 0; i < blockSize; ++i)
	{
		float sample = processBuffer.getSample(0, i);
		// Both grains contribute 1.0, so sum should be 2.0
		if (sample == 2.0f)
			samplesWithOverlap++;
	}

	INFO("Samples with overlap sum (2.0): " << samplesWithOverlap << " (expected " << blockSize << ")");
	CHECK(samplesWithOverlap == blockSize);

	// Both grains should still be active (neither ends within this block)
	activeCount = 0;
	for (int i = 0; i < kNumGrains; ++i)
		if (grains[i].isActive) activeCount++;
	CHECK(activeCount == 2);
}

//=======================================================================================
//=======================================================================================
// WAVEFORM CONTINUITY TESTS - PHASE-SHIFTED GRAIN CREATION
//=======================================================================================
/**
 * Test that grains are created with proper phase offsets when pitch shifting.
 *
 * The Granulator uses mCumulativePhase to track phase continuity. When creating grains,
 * the audio data is read with a sample offset based on this cumulative phase, ensuring
 * waveform continuity across grain boundaries during pitch shifting.
 *
 * For pitch shift UP (detected period 256, shifted period 192):
 * - Grain 1: phase offset = 0 samples (starts at cycle beginning)
 * - Grain 2: phase offset = 192 samples worth of phase = 3π/2 → ~192 sample offset in period
 * - Grain 3: cumulative phase wraps, continues the pattern
 *
 * This test verifies that multiple grains are created when pitch shifting.
 */
TEST_CASE("Granulator creates multiple grains with phase continuity when pitch shifting up", "[Granulator][makeGrain][phase]")
{
	constexpr double sampleRate = 48000.0;
	constexpr int blockSize = 128;
	constexpr int circularBufferSize = 4096;
	constexpr float detectedPeriod = 256.0f;
	constexpr float shiftedPeriod = 192.0f; // Pitch shift UP

	// Create and prepare Granulator
	Granulator granulator;
	int maxGrainSize = static_cast<int>(detectedPeriod * 2);
	granulator.prepare(sampleRate, blockSize, maxGrainSize);

	// Create CircularBuffer and fill with test signal
	CircularBuffer circularBuffer;
	circularBuffer.setSize(2, circularBufferSize);
	juce::AudioBuffer<float> sineBuffer(2, circularBufferSize);
	BufferFiller::generateSineCycles(sineBuffer, static_cast<int>(detectedPeriod));
	circularBuffer.pushBuffer(sineBuffer);

	// Create process buffer
	juce::AudioBuffer<float> processBuffer(2, blockSize);
	processBuffer.clear();

	// First call to processTracking - should create 2 grains due to shorter shifted period
	std::tuple<juce::int64, juce::int64, juce::int64> analysisReadRange1 = {744, 1000, 1255};
	std::tuple<juce::int64, juce::int64, juce::int64> analysisWriteRange1 = {1536, 1792, 2047};
	std::tuple<juce::int64, juce::int64> processCounterRange1 = {1536, 1663};

	granulator.processTracking(processBuffer, circularBuffer,
							   analysisReadRange1, analysisWriteRange1,
							   processCounterRange1,
							   detectedPeriod, shiftedPeriod);

	auto& grains = granulator.getGrains();

	// Count active grains
	int activeCount = 0;
	for (const auto& grain : grains)
		if (grain.isActive) activeCount++;

	// Should have created 2 grains in one call when pitch shifting up
	CHECK(activeCount == 2);

	// Verify the grains were created at expected synth marks
	bool foundGrain1 = false;
	bool foundGrain2 = false;
	for (const auto& grain : grains)
	{
		if (!grain.isActive) continue;
		auto [sStart, sMark, sEnd] = grain.mSynthRange;
		if (sMark == 1792) foundGrain1 = true;
		if (sMark == 1984) foundGrain2 = true;
	}
	CHECK(foundGrain1);
	CHECK(foundGrain2);
}

/**
 * Test waveform phase alignment across multiple grains with sine wave input.
 *
 * This test verifies that when grains are created with phase offsets, the audio data
 * in each grain buffer correctly represents a phase-shifted version of the source waveform.
 *
 * We use a sine wave with period=256 samples and no windowing to directly observe the
 * phase continuity. Each grain's buffer should contain the sine wave starting at the
 * phase determined by mCumulativePhase.
 */
TEST_CASE("Granulator grain buffers contain correctly phase-shifted sine waves", "[Granulator][makeGrain][phase][continuity]")
{
	constexpr double sampleRate = 48000.0;
	constexpr int blockSize = 128;
	constexpr int circularBufferSize = 4096;
	constexpr float detectedPeriod = 256.0f;
	constexpr int grainSize = static_cast<int>(detectedPeriod * 2); // 512
	constexpr float tolerance = 0.05f;

	SECTION("Pitch shift UP: grains should have incrementing phase offsets")
	{
		constexpr float shiftedPeriod = 192.0f; // Shift up

		Granulator granulator;
		granulator.prepare(sampleRate, blockSize, grainSize);
		granulator.getWindow().setSizeShapePeriod(static_cast<int>(sampleRate), Window::Shape::kNone, grainSize);

		CircularBuffer circularBuffer;
		circularBuffer.setSize(2, circularBufferSize);
		juce::AudioBuffer<float> sineBuffer(2, circularBufferSize);
		BufferFiller::generateSineCycles(sineBuffer, static_cast<int>(detectedPeriod));
		circularBuffer.pushBuffer(sineBuffer);

		juce::AudioBuffer<float> processBuffer(2, blockSize);
		processBuffer.clear();

		// First processTracking call - creates grain 1 and grain 2
		// analysisWriteRange centered at 512 (2 periods from start)
		granulator.processTracking(processBuffer, circularBuffer,
								   {0, 256, 511},      // analysisReadRange
								   {256, 512, 767},    // analysisWriteRange (synthMark starts at 512)
								   {256, 383},         // processCounterRange
								   detectedPeriod, shiftedPeriod);

		auto& grains = granulator.getGrains();

		// Grain 1: synthMark = 512, phase = 0 (512 % 256 = 0)
		// Grain 2: synthMark = 704 (512 + 192), phase = (704 % 256) / 256 * 2π = (192/256) * 2π = 3π/2
		double expectedPhase1 = 0.0;
		double expectedPhase2 = (192.0 / 256.0) * 2.0 * M_PI; // 3π/2

		// Create reference sine buffers for comparison
		juce::AudioBuffer<float> referenceSine1(2, grainSize);
		juce::AudioBuffer<float> referenceSine2(2, grainSize);
		BufferFiller::generateSineWithPhase(referenceSine1, detectedPeriod, expectedPhase1);
		BufferFiller::generateSineWithPhase(referenceSine2, detectedPeriod, expectedPhase2);

		for (const auto& grain : grains)
		{
			if (!grain.isActive) continue;

			auto [sStart, sMark, sEnd] = grain.mSynthRange;
			const auto& grainBuffer = grain.getBuffer();

			if (sMark == 512)
			{
				std::cout << "\n>>> Verifying Grain 1 (synthMark=512) has phase offset 0 <<<" << std::endl;
				std::cout << "Expected phase: " << expectedPhase1 << " (0)" << std::endl;

				// Print first 10 samples
				std::cout << "First 10 samples comparison:" << std::endl;
				int mismatchCount = 0;
				for (int i = 0; i < std::min(10, grainSize); ++i)
				{
					auto [matches, val1, val2] = BufferHelper::samplesMatchAtIndex(grainBuffer, referenceSine1, i, 0, tolerance);
					std::cout << "  [" << i << "] grain=" << val1 << ", ref=" << val2 << ", diff=" << (val1 - val2) << std::endl;
				}

				// Check middle section
				std::cout << "Checking samples 10 to 50:" << std::endl;
				for (int i = 10; i < std::min(50, grainSize - 10); ++i)
				{
					auto [matches, val1, val2] = BufferHelper::samplesMatchAtIndex(grainBuffer, referenceSine1, i, 0, tolerance);
					if (!matches)
					{
						mismatchCount++;
						if (mismatchCount <= 10)
							std::cout << "  MISMATCH [" << i << "] grain=" << val1 << ", ref=" << val2 << ", diff=" << (val1 - val2) << std::endl;
					}
				}
				std::cout << "Total mismatches: " << mismatchCount << "\n" << std::endl;
				CHECK(mismatchCount == 0);
			}
			else if (sMark == 704)
			{
				std::cout << "\n>>> Verifying Grain 2 (synthMark=704) has phase offset 3π/2 <<<" << std::endl;
				std::cout << "Expected phase: " << expectedPhase2 << " (" << (expectedPhase2 / M_PI) << "π)" << std::endl;

				// Print first 10 samples
				std::cout << "First 10 samples comparison:" << std::endl;
				int mismatchCount = 0;
				for (int i = 0; i < std::min(10, grainSize); ++i)
				{
					auto [matches, val1, val2] = BufferHelper::samplesMatchAtIndex(grainBuffer, referenceSine2, i, 0, tolerance);
					std::cout << "  [" << i << "] grain=" << val1 << ", ref=" << val2 << ", diff=" << (val1 - val2) << std::endl;
				}

				// Check middle section
				std::cout << "Checking samples 10 to 50:" << std::endl;
				for (int i = 10; i < std::min(50, grainSize - 10); ++i)
				{
					auto [matches, val1, val2] = BufferHelper::samplesMatchAtIndex(grainBuffer, referenceSine2, i, 0, tolerance);
					if (!matches)
					{
						mismatchCount++;
						if (mismatchCount <= 10)
							std::cout << "  MISMATCH [" << i << "] grain=" << val1 << ", ref=" << val2 << ", diff=" << (val1 - val2) << std::endl;
					}
				}
				std::cout << "Total mismatches: " << mismatchCount << "\n" << std::endl;
				CHECK(mismatchCount == 0);
			}
		}

		// Second call - creates grain 3
		// mSynthMark is now at 896 (704 + 192)
		processBuffer.clear();
		granulator.processTracking(processBuffer, circularBuffer,
								   {256, 512, 767},    // analysisReadRange
								   {512, 768, 1023},   // analysisWriteRange
								   {384, 511},         // processCounterRange
								   detectedPeriod, shiftedPeriod);

		// Grain 3: synthMark = 896, phase = (896 % 256) / 256 * 2π = (128/256) * 2π = π
		double expectedPhase3 = (128.0 / 256.0) * 2.0 * M_PI; // π
		juce::AudioBuffer<float> referenceSine3(2, grainSize);
		BufferFiller::generateSineWithPhase(referenceSine3, detectedPeriod, expectedPhase3);

		for (const auto& grain : grains)
		{
			if (!grain.isActive) continue;

			auto [sStart, sMark, sEnd] = grain.mSynthRange;
			if (sMark == 896)
			{
				const auto& grainBuffer = grain.getBuffer();
				std::cout << "\n>>> Verifying Grain 3 (synthMark=896) has phase offset π <<<" << std::endl;
				std::cout << "Expected phase: " << expectedPhase3 << " (" << (expectedPhase3 / M_PI) << "π)" << std::endl;

				// Print first 10 samples
				std::cout << "First 10 samples comparison:" << std::endl;
				int mismatchCount = 0;
				for (int i = 0; i < std::min(10, grainSize); ++i)
				{
					auto [matches, val1, val2] = BufferHelper::samplesMatchAtIndex(grainBuffer, referenceSine3, i, 0, tolerance);
					std::cout << "  [" << i << "] grain=" << val1 << ", ref=" << val2 << ", diff=" << (val1 - val2) << std::endl;
				}

				// Check middle section
				std::cout << "Checking samples 10 to 50:" << std::endl;
				for (int i = 10; i < std::min(50, grainSize - 10); ++i)
				{
					auto [matches, val1, val2] = BufferHelper::samplesMatchAtIndex(grainBuffer, referenceSine3, i, 0, tolerance);
					if (!matches)
					{
						mismatchCount++;
						if (mismatchCount <= 10)
							std::cout << "  MISMATCH [" << i << "] grain=" << val1 << ", ref=" << val2 << ", diff=" << (val1 - val2) << std::endl;
					}
				}
				std::cout << "Total mismatches: " << mismatchCount << "\n" << std::endl;
				CHECK(mismatchCount == 0);
			}
		}
	}

	SECTION("Pitch shift DOWN: grains should have incrementing phase offsets")
	{
		constexpr float shiftedPeriod = 320.0f; // Shift down

		Granulator granulator;
		granulator.prepare(sampleRate, blockSize, grainSize);
		granulator.getWindow().setSizeShapePeriod(static_cast<int>(sampleRate), Window::Shape::kNone, grainSize);

		CircularBuffer circularBuffer;
		circularBuffer.setSize(2, circularBufferSize);
		juce::AudioBuffer<float> sineBuffer(2, circularBufferSize);
		BufferFiller::generateSineCycles(sineBuffer, static_cast<int>(detectedPeriod));
		circularBuffer.pushBuffer(sineBuffer);

		juce::AudioBuffer<float> processBuffer(2, blockSize);

		// First call - creates grain 1
		// synthMark starts at 512 (512 % 256 = 0, so phase = 0)
		processBuffer.clear();
		granulator.processTracking(processBuffer, circularBuffer,
								   {0, 256, 511},      // analysisReadRange
								   {256, 512, 767},    // analysisWriteRange (synthMark starts at 512)
								   {256, 383},         // processCounterRange
								   detectedPeriod, shiftedPeriod);

		auto& grains = granulator.getGrains();

		// Grain 1: synthMark = 512, phase = 0
		double expectedPhase1 = 0.0;
		juce::AudioBuffer<float> referenceSine1(2, grainSize);
		BufferFiller::generateSineWithPhase(referenceSine1, detectedPeriod, expectedPhase1);

		for (const auto& grain : grains)
		{
			if (!grain.isActive) continue;

			auto [sStart, sMark, sEnd] = grain.mSynthRange;
			if (sMark == 512)
			{
				const auto& grainBuffer = grain.getBuffer();
				std::cout << "\n>>> Verifying Grain 1 (synthMark=512) has phase offset 0 <<<" << std::endl;

				int mismatchCount = 0;
				for (int i = 10; i < std::min(50, grainSize - 10); ++i)
				{
					auto [matches, val1, val2] = BufferHelper::samplesMatchAtIndex(grainBuffer, referenceSine1, i, 0, tolerance);
					if (!matches) mismatchCount++;
				}
				std::cout << "Mismatch count: " << mismatchCount << "\n" << std::endl;
				CHECK(mismatchCount == 0);
			}
		}

		// Second call - creates grain 2
		// mSynthMark is now at 832 (512 + 320)
		// 832 % 256 = 64, so phase = (64/256) * 2π = π/2
		processBuffer.clear();
		granulator.processTracking(processBuffer, circularBuffer,
								   {256, 512, 767},    // analysisReadRange
								   {512, 768, 1023},   // analysisWriteRange
								   {384, 511},         // processCounterRange
								   detectedPeriod, shiftedPeriod);

		// Grain 2: synthMark = 832, phase = (64/256) * 2π = π/2
		double expectedPhase2 = (64.0 / 256.0) * 2.0 * M_PI; // π/2
		juce::AudioBuffer<float> referenceSine2(2, grainSize);
		BufferFiller::generateSineWithPhase(referenceSine2, detectedPeriod, expectedPhase2);

		for (const auto& grain : grains)
		{
			if (!grain.isActive) continue;

			auto [sStart, sMark, sEnd] = grain.mSynthRange;
			if (sMark == 832)
			{
				const auto& grainBuffer = grain.getBuffer();
				std::cout << "\n>>> Verifying Grain 2 (synthMark=832) has phase offset π/2 <<<" << std::endl;

				int mismatchCount = 0;
				for (int i = 10; i < std::min(50, grainSize - 10); ++i)
				{
					auto [matches, val1, val2] = BufferHelper::samplesMatchAtIndex(grainBuffer, referenceSine2, i, 0, tolerance);
					if (!matches) mismatchCount++;
				}
				std::cout << "Mismatch count: " << mismatchCount << "\n" << std::endl;
				CHECK(mismatchCount == 0);
			}
		}
	}

	SECTION("No pitch shift: all grains should have phase offset 0")
	{
		constexpr float shiftedPeriod = 256.0f; // No shift

		Granulator granulator;
		granulator.prepare(sampleRate, blockSize, grainSize);
		granulator.getWindow().setSizeShapePeriod(static_cast<int>(sampleRate), Window::Shape::kNone, grainSize);

		CircularBuffer circularBuffer;
		circularBuffer.setSize(2, circularBufferSize);
		juce::AudioBuffer<float> sineBuffer(2, circularBufferSize);
		BufferFiller::generateSineCycles(sineBuffer, static_cast<int>(detectedPeriod));
		circularBuffer.pushBuffer(sineBuffer);

		juce::AudioBuffer<float> processBuffer(2, blockSize);

		// Create multiple grains
		// Grain 1: synthMark = 512 (512 % 256 = 0, phase = 0)
		processBuffer.clear();
		granulator.processTracking(processBuffer, circularBuffer,
								   {0, 256, 511},      // analysisReadRange
								   {256, 512, 767},    // analysisWriteRange
								   {256, 383},         // processCounterRange
								   detectedPeriod, shiftedPeriod);

		// Grain 2: synthMark = 768 (512 + 256), 768 % 256 = 0, phase = 0
		processBuffer.clear();
		granulator.processTracking(processBuffer, circularBuffer,
								   {256, 512, 767},    // analysisReadRange
								   {512, 768, 1023},   // analysisWriteRange
								   {384, 511},         // processCounterRange
								   detectedPeriod, shiftedPeriod);

		auto& grains = granulator.getGrains();

		// With no pitch shift, phase increment = (256/256) * 2π = 2π
		// So every grain wraps back to phase 0
		// Expected synthMarks: 512, 768 (both are multiples of 256, so phase = 0)
		juce::AudioBuffer<float> referenceSine(2, grainSize);
		BufferFiller::generateSineWithPhase(referenceSine, detectedPeriod, 0.0);

		for (const auto& grain : grains)
		{
			if (!grain.isActive) continue;

			auto [sStart, sMark, sEnd] = grain.mSynthRange;
			const auto& grainBuffer = grain.getBuffer();
			std::cout << "\n>>> Verifying grain at synthMark=" << sMark << " has phase offset 0 <<<" << std::endl;

			int mismatchCount = 0;
			for (int i = 10; i < std::min(50, grainSize - 10); ++i)
			{
				auto [matches, val1, val2] = BufferHelper::samplesMatchAtIndex(grainBuffer, referenceSine, i, 0, tolerance);
				if (!matches) mismatchCount++;
			}
			std::cout << "Mismatch count: " << mismatchCount << "\n" << std::endl;
			CHECK(mismatchCount == 0);
		}
	}
}
