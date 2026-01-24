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
 */
class GranulatorTester
{
public:
	static void makeGrain(Granulator& g, CircularBuffer& circularBuffer,
						  std::tuple<juce::int64, juce::int64, juce::int64> analysisReadRange,
						  std::tuple<juce::int64, juce::int64, juce::int64> synthRange,
						  float detectedPeriod)
	{
		g._makeGrain(circularBuffer, analysisReadRange, synthRange, detectedPeriod);
	}
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
	GranulatorTester::makeGrain(granulator, circularBuffer, analysisReadRange, synthRange, detectedPeriod);

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
	GranulatorTester::makeGrain(granulator, circularBuffer, analysisReadRange, synthRange, detectedPeriod);

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
