/**
 * Created by Ryan Devens 2025-10-13
 */

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
	granulator.makeGrain(circularBuffer, analysisReadRange, synthRange, detectedPeriod);

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
	granulator.makeGrain(circularBuffer, analysisReadRange, synthRange, detectedPeriod);

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
