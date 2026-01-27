/**
 * test_PluginProcessor.cpp
 * Created by Ryan Devens
 *
 * Tests for PluginProcessor's getRange functions with explicit sample indices
 * for a clear mental model of the timing relationships.
 *
 * Setup:
 * - Sine buffer: 2048 samples, 2 channels, period of 256 samples
 * - Sample rate: 48000
 * - Block size: 128
 * - Process 128 samples at a time, 13 calls total
 * - Test sections occur on the 13th processBlock call
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../SOURCE/PluginProcessor.h"
#include "../SUBMODULES/RD/SOURCE/BufferFiller.h"
#include "../SUBMODULES/RD/SOURCE/BufferHelper.h"
#include "../SUBMODULES/RD/TESTS/TEST_UTILS/TestUtils.h"

//==============================================================================
// Test Constants - Explicit values for mental model
//==============================================================================
namespace TestConfig
{
	constexpr double sampleRate = 48000.0;
	constexpr int blockSize = 128;
	constexpr int sineBufferSize = 2048;
	constexpr int sinePeriod = 256;
	constexpr int numChannels = 2;
	constexpr int numProcessCalls = 13;
}

//==============================================================================
//==============================================================================
//========================= getProcessCounterRange() ===========================
//==============================================================================
//==============================================================================

TEST_CASE("PluginProcessor getProcessCounterRange() returns correct sample indices", "[PluginProcessor][getProcessCounterRange]")
{
	TestUtils::SetupAndTeardown setupAndTeardown;

	// Create processor and prepare
	PluginProcessor processor;
	processor.prepareToPlay(TestConfig::sampleRate, TestConfig::blockSize);

	// Create sine buffer: 2048 samples, 2 channels, period of 256
	juce::AudioBuffer<float> sineBuffer(TestConfig::numChannels, TestConfig::sineBufferSize);
	sineBuffer.clear();
	BufferFiller::generateSineCycles(sineBuffer, TestConfig::sinePeriod);

	// Create process buffer sized for one block
	juce::AudioBuffer<float> processBuffer(TestConfig::numChannels, TestConfig::blockSize);
	juce::MidiBuffer midiBuffer;

	// Process 13 blocks of 128 samples each
	// After each call, mSamplesProcessed increments by blockSize
	//
	// Call  | Before call mSamplesProcessed | Range returned by getProcessCounterRange()
	// ------|-------------------------------|--------------------------------------------
	//   1   |            0                  | (0, 127)
	//   2   |          128                  | (128, 255)
	//   3   |          256                  | (256, 383)
	//   4   |          384                  | (384, 511)
	//   5   |          512                  | (512, 639)
	//   6   |          640                  | (640, 767)
	//   7   |          768                  | (768, 895)
	//   8   |          896                  | (896, 1023)
	//   9   |         1024                  | (1024, 1151)
	//  10   |         1152                  | (1152, 1279)
	//  11   |         1280                  | (1280, 1407)
	//  12   |         1408                  | (1408, 1535)
	//  13   |         1536                  | (1536, 1663)

	// All setup, we are testing the 13th call
	for (int callIndex = 1; callIndex <= TestConfig::numProcessCalls - 1; ++callIndex)
	{
		// Calculate which portion of sineBuffer to copy into processBuffer
		int sourceStartSample = ((callIndex - 1) * TestConfig::blockSize) % TestConfig::sineBufferSize;

		// Copy 128 samples from sineBuffer into processBuffer
		for (int ch = 0; ch < TestConfig::numChannels; ++ch)
		{
			for (int sample = 0; sample < TestConfig::blockSize; ++sample)
			{
				int sourceIndex = (sourceStartSample + sample) % TestConfig::sineBufferSize;
				processBuffer.setSample(ch, sample, sineBuffer.getSample(ch, sourceIndex));
			}
		}
		// Process the block
		processor.processBlock(processBuffer, midiBuffer);
	}


	SECTION("Detected period is approximately 256 samples")
	{
		float expectedPeriod = static_cast<float>(TestConfig::sinePeriod); // 256
		float detectedPeriod = processor.getLastDetectedPeriod();

		// YIN algorithm may detect slightly different period (e.g., 255.5)
		CHECK(detectedPeriod == Catch::Approx(expectedPeriod).margin(1.0f));
	}

	SECTION("getProcessCounterRange: After 12th processBlock call, range is (1536, 1663)")
	{
		auto [start, end] = processor.getProcessCounterRange();

		// mSamplesProcessed = 12 * 128 = 1536 before 13th call
		// start = mSamplesProcessed = 1536
		// end = mSamplesProcessed + blockSize - 1 = 1536 + 128 - 1 = 1663
		CHECK(start == 1536);
		CHECK(end == 1663);
	}

	SECTION("getDetectionRange: After 12th processBlock call, range is (127, 1151)")
	{
		auto [start, end] = processor.getDetectionRange();

		// endProcessSample = mSamplesProcessed + mBlockSize - 1 = 1536 + 128 - 1 = 1663
		// endDetectionSample = endProcessSample - minLookaheadSize = 1663 - 512 = 1151
		// startDetectionSample = endDetectionSample - minDetectionSize = 1151 - 1024 = 127
		CHECK(start == 127);
		CHECK(end == 1151);
	}

	SECTION("getFirstPeakRange: With period 256, range is (895, 1151)")
	{
		float detectedPeriod = static_cast<float>(TestConfig::sinePeriod); // 256
		auto [start, end] = processor.getFirstPeakRange(detectedPeriod);

		// endProcessSample = mSamplesProcessed + mBlockSize - 1 = 1536 + 128 - 1 = 1663
		// endDetectionSample = endProcessSample - minLookaheadSize = 1663 - 512 = 1151
		// endFirstPeakRange = endDetectionSample = 1151
		// startFirstPeakRange = endFirstPeakRange - detectedPeriod = 1151 - 256 = 895
		CHECK(start == 895);
		CHECK(end == 1151);
	}

	SECTION("getAnalysisReadRange: With analysisMark 1000 and period 256, range is (744, 1000, 1255)")
	{
		juce::int64 analysisMark = 1000;
		float detectedPeriod = static_cast<float>(TestConfig::sinePeriod); // 256
		auto [start, mark, end] = processor.getAnalysisReadRange(analysisMark, detectedPeriod);

		// start = analysisMark - detectedPeriod = 1000 - 256 = 744
		// mark = analysisMark = 1000
		// end = analysisMark + detectedPeriod - 1 = 1000 + 256 - 1 = 1255
		CHECK(start == 744);
		CHECK(mark == 1000);
		CHECK(end == 1255);
	}

	SECTION("getAnalysisWriteRange: With analysisReadRange (744, 1000, 1255), range is (1256, 1512, 1767)")
	{
		// Get the read range first
		juce::int64 analysisMark = 1000;
		float detectedPeriod = static_cast<float>(TestConfig::sinePeriod); // 256
		auto analysisReadRange = processor.getAnalysisReadRange(analysisMark, detectedPeriod);

		auto [start, mark, end] = processor.getAnalysisWriteRange(analysisReadRange);

		// Write range = read range + minLookaheadSize (512)
		// start = 744 + 512 = 1256
		// mark = 1000 + 512 = 1512
		// end = 1255 + 512 = 1767
		CHECK(start == 1256);
		CHECK(mark == 1512);
		CHECK(end == 1767);
	}

	SECTION("getPrecisePeakRange: With predictedAnalysisMark 1256 and period 256, range is (1192, 1320)")
	{
		// Process one additional block (14th call)
		int sourceStartSample = (TestConfig::numProcessCalls * TestConfig::blockSize) % TestConfig::sineBufferSize;
		for (int ch = 0; ch < TestConfig::numChannels; ++ch)
		{
			for (int sample = 0; sample < TestConfig::blockSize; ++sample)
			{
				int sourceIndex = (sourceStartSample + sample) % TestConfig::sineBufferSize;
				processBuffer.setSample(ch, sample, sineBuffer.getSample(ch, sourceIndex));
			}
		}
		processor.processBlock(processBuffer, midiBuffer);

		// Predicted analysis mark from previous detection
		juce::int64 predictedAnalysisMark = 1256;
		float detectedPeriod = static_cast<float>(TestConfig::sinePeriod); // 256

		auto [start, end] = processor.getPrecisePeakRange(predictedAnalysisMark, detectedPeriod);

		// getPrecisePeakRange uses radius = period * 0.25 = 64
		// start = predictedAnalysisMark - radius = 1256 - 64 = 1192
		// end = predictedAnalysisMark + radius = 1256 + 64 = 1320
		CHECK(start == 1192);
		CHECK(end == 1320);
	}

}

//==============================================================================
//==============================================================================
// DO CORRECTION TESTS
//==============================================================================
/**
 * Test doCorrection() with circular buffer filled with all ones.
 *
 * Setup:
 * - Process multiple blocks of all-ones to fill circular buffer
 * - Clear process buffer before calling doCorrection
 * - Call doCorrection with detectedPeriod = 256
 */
TEST_CASE("PluginProcessor doCorrection() with all-ones circular buffer", "[PluginProcessor][doCorrection]")
{
	TestUtils::SetupAndTeardown setupAndTeardown;

	// Create processor and prepare
	PluginProcessor processor;
	processor.prepareToPlay(TestConfig::sampleRate, TestConfig::blockSize);

	// Create buffer filled with all ones
	juce::AudioBuffer<float> onesBuffer(TestConfig::numChannels, TestConfig::blockSize);
	BufferFiller::fillWithAllOnes(onesBuffer);

	juce::MidiBuffer midiBuffer;

	// Process enough blocks to fill the circular buffer with all ones
	// Need at least minLookaheadSize + minDetectionSize = 512 + 1024 = 1536 samples
	// With blockSize = 128, need at least 12 blocks
	for (int i = 0; i < TestConfig::numProcessCalls; ++i)
	{
		// Refill with ones since processBlock may modify the buffer
		BufferFiller::fillWithAllOnes(onesBuffer);
		processor.processBlock(onesBuffer, midiBuffer);
	}

	// Create a cleared process buffer for doCorrection
	juce::AudioBuffer<float> processBuffer(TestConfig::numChannels, TestConfig::blockSize);
	processBuffer.clear();

	// Call doCorrection with detectedPeriod = 256
	constexpr float detectedPeriod = 256.0f;
	processor.doCorrection(processBuffer, detectedPeriod);

	SECTION("Process state is tracking after valid period")
	{
		CHECK(processor.getCurrentState() == PluginProcessor::ProcessState::kTracking);
	}

	SECTION("Process buffer has non-zero samples after correction")
	{
		// With all-ones input and Hanning window, output should have windowed grain samples
		bool hasNonZero = false;
		for (int ch = 0; ch < TestConfig::numChannels; ++ch)
		{
			for (int i = 0; i < TestConfig::blockSize; ++i)
			{
				if (processBuffer.getSample(ch, i) != 0.0f)
				{
					hasNonZero = true;
					break;
				}
			}
			if (hasNonZero) break;
		}
		CHECK(hasNonZero);
	}
}

/**
 * Test Hanning window overlap-add with no pitch shifting.
 *
 * With all-ones input and no pitch shifting:
 * - Grain size = 2 * period = 512 samples
 * - Grains emitted every shiftedPeriod = detectedPeriod = 256 samples
 * - Hanning windows overlap by 50%
 *
 * When two 50%-overlapped Hanning windows are added (overlap-add),
 * their sum should be approximately 1.0 in the overlap region.
 * This is a key property that makes TD-PSOLA reconstruction work.
 *
 * Note: Must use processBlock (not doCorrection directly) to advance mSamplesProcessed.
 */
TEST_CASE("PluginProcessor Hanning window overlap sums to ~1.0", "[PluginProcessor][doCorrection][overlap]")
{
	TestUtils::SetupAndTeardown setupAndTeardown;

	// Create processor and prepare
	PluginProcessor processor;
	processor.prepareToPlay(TestConfig::sampleRate, TestConfig::blockSize);

	juce::MidiBuffer midiBuffer;

	// Create buffer filled with all ones
	// Using all-ones means output = windowValue * 1.0 = windowValue
	juce::AudioBuffer<float> onesBuffer(TestConfig::numChannels, TestConfig::blockSize);

	// Process many blocks to:
	// 1. Fill circular buffer with ones
	// 2. Establish tracking state
	// 3. Build up overlapping grains
	constexpr int numBlocks = 20; // Enough to get stable overlap
	juce::AudioBuffer<float> lastOutputBuffer(TestConfig::numChannels, TestConfig::blockSize);

	// processBlock calls needed for range increments, but not being tested here.
	for (int i = 0; i < numBlocks; ++i)
	{
		BufferFiller::fillWithAllOnes(onesBuffer);
		processor.processBlock(onesBuffer, midiBuffer);

		// Keep copy of output from later blocks where overlap should be established
		if (i >= numBlocks - 1)
		{
			for (int ch = 0; ch < TestConfig::numChannels; ++ch)
			{
				lastOutputBuffer.copyFrom(ch, 0, onesBuffer, ch, 0, TestConfig::blockSize);
			}
		}
	}



	SECTION("Overlap region samples sum to approximately 1.0")
	{
		// Check samples - with 50% overlap of Hanning windows, sum should be ~1.0
		int numSamplesNearOne = 0;
		int numSamplesChecked = 0;

		for (int i = 0; i < TestConfig::blockSize; ++i)
		{
			float sample = lastOutputBuffer.getSample(0, i);
			if (sample > 0.0f) // Only check non-zero samples
			{
				numSamplesChecked++;
				// Hanning overlap-add should give values close to 1.0
				if (sample >= 0.9f && sample <= 1.1f)
				{
					numSamplesNearOne++;
				}
			}
		}

		INFO("Samples checked: " << numSamplesChecked);
		if (numSamplesChecked > 0)
		{
			float ratioNearOne = static_cast<float>(numSamplesNearOne) / static_cast<float>(numSamplesChecked);
			INFO("Samples near 1.0: " << numSamplesNearOne << " / " << numSamplesChecked << " = " << ratioNearOne);
			CHECK(ratioNearOne >= 0.8f); // At least 80% should be near 1.0
		}
	}

	SECTION("Output values are bounded (no clipping)")
	{
		for (int ch = 0; ch < TestConfig::numChannels; ++ch)
		{
			for (int i = 0; i < TestConfig::blockSize; ++i)
			{
				float sample = lastOutputBuffer.getSample(ch, i);
				CHECK(sample >= 0.0f);
				CHECK(sample <= 1.5f); // Should not significantly exceed 1.0
			}
		}
	}
}

//==============================================================================
//==============================================================================
// END-TO-END PROCESSBLOCK TESTS
//==============================================================================
/**
 * End-to-end processBlock test with sine wave input.
 *
 * Setup:
 * - Sample rate: 48000
 * - Block size: 128
 * - Sine period: 256 samples
 * - No pitch shift (shiftedPeriod = detectedPeriod)
 *
 * Timeline (block size = 128):
 * - minLookaheadSize = 512 samples (4 blocks)
 * - minDetectionSize = 1024 samples (8 blocks)
 * - First valid detection possible after: 512 + 1024 = 1536 samples = 12 blocks
 *
 * Block | mSamplesProcessed | Detection Range      | State Expected
 * ------|-------------------|----------------------|----------------
 *  1-12 | 0 - 1408          | insufficient data    | kDetecting
 *  13+  | 1536+             | [127, 1151] valid    | kTracking
 *
 * With Hanning windowing and 50% overlap:
 * - Grain size = 2 * period = 512 samples
 * - Grain emission rate = period = 256 samples
 * - Overlapped Hanning windows sum to ~1.0
 * - Output should reconstruct the input sine wave
 */
TEST_CASE("PluginProcessor end-to-end processBlock with sine input", "[PluginProcessor][processBlock][e2e]")
{
	TestUtils::SetupAndTeardown setupAndTeardown;

	// Create processor and prepare
	PluginProcessor processor;
	processor.prepareToPlay(TestConfig::sampleRate, TestConfig::blockSize);

	// Create sine buffer: 2048 samples, 2 channels, period of 256
	juce::AudioBuffer<float> sineBuffer(TestConfig::numChannels, TestConfig::sineBufferSize);
	sineBuffer.clear();
	BufferFiller::generateSineCycles(sineBuffer, TestConfig::sinePeriod);

	// Create process buffer sized for one block
	juce::AudioBuffer<float> processBuffer(TestConfig::numChannels, TestConfig::blockSize);
	juce::MidiBuffer midiBuffer;

	// Helper lambda to fill processBuffer from sineBuffer at given call index
	auto fillProcessBuffer = [&](int callIndex) {
		int sourceStartSample = ((callIndex - 1) * TestConfig::blockSize) % TestConfig::sineBufferSize;
		for (int ch = 0; ch < TestConfig::numChannels; ++ch)
		{
			for (int sample = 0; sample < TestConfig::blockSize; ++sample)
			{
				int sourceIndex = (sourceStartSample + sample) % TestConfig::sineBufferSize;
				processBuffer.setSample(ch, sample, sineBuffer.getSample(ch, sourceIndex));
			}
		}
	};

	SECTION("State eventually transitions to kTracking with valid pitch input")
	{
		// Process blocks until tracking begins
		int trackingStartBlock = -1;
		for (int callIndex = 1; callIndex <= 20; ++callIndex)
		{
			fillProcessBuffer(callIndex);
			processor.processBlock(processBuffer, midiBuffer);

			if (processor.getCurrentState() == PluginProcessor::ProcessState::kTracking && trackingStartBlock < 0)
			{
				trackingStartBlock = callIndex;
			}
		}

		INFO("Tracking started on block: " << trackingStartBlock);
		CHECK(trackingStartBlock > 0); // Should eventually start tracking
		CHECK(processor.getCurrentState() == PluginProcessor::ProcessState::kTracking);
	}

	SECTION("Detected period is approximately 256 samples after tracking begins")
	{
		// Process enough blocks to ensure tracking
		for (int callIndex = 1; callIndex <= 20; ++callIndex)
		{
			fillProcessBuffer(callIndex);
			processor.processBlock(processBuffer, midiBuffer);
		}

		float detectedPeriod = processor.getLastDetectedPeriod();
		INFO("Detected period: " << detectedPeriod);
		CHECK(detectedPeriod == Catch::Approx(256.0f).margin(2.0f));
	}

	SECTION("Output has non-zero samples when tracking with sufficient warmup")
	{
		// Need enough warmup for circular buffer to be fully populated
		// CircularBuffer is 2 * pitchDetectBufferNumSamples = 2 * 1024 = 2048 samples
		// Need 2048 / 128 = 16 blocks minimum to fill buffer
		// Use 25 to be safe and ensure stable tracking
		constexpr int warmupBlocks = 25;

		for (int callIndex = 1; callIndex <= warmupBlocks; ++callIndex)
		{
			fillProcessBuffer(callIndex);
			processor.processBlock(processBuffer, midiBuffer);
		}

		CHECK(processor.getCurrentState() == PluginProcessor::ProcessState::kTracking);

		// Process additional blocks and check output
		constexpr int testBlocks = 4;
		int totalNonZeroSamples = 0;
		float maxAbsValue = 0.0f;

		for (int i = 0; i < testBlocks; ++i)
		{
			fillProcessBuffer(warmupBlocks + 1 + i);
			processor.processBlock(processBuffer, midiBuffer);

			for (int s = 0; s < TestConfig::blockSize; ++s)
			{
				float sample = processBuffer.getSample(0, s);
				if (std::abs(sample) > 0.001f)
					totalNonZeroSamples++;
				maxAbsValue = std::max(maxAbsValue, std::abs(sample));
			}
		}

		int totalSamples = testBlocks * TestConfig::blockSize;
		INFO("Non-zero samples: " << totalNonZeroSamples << " / " << totalSamples);
		INFO("Max absolute value: " << maxAbsValue);

		// Should have significant non-zero output when tracking with valid input
		CHECK(totalNonZeroSamples > totalSamples / 2);
		CHECK(maxAbsValue > 0.1f);
		CHECK(maxAbsValue <= 2.0f); // Should be bounded
	}

	SECTION("Output preserves periodic structure when no pitch shift")
	{
		// Large warmup to ensure stable operation
		constexpr int warmupBlocks = 30;

		for (int callIndex = 1; callIndex <= warmupBlocks; ++callIndex)
		{
			fillProcessBuffer(callIndex);
			processor.processBlock(processBuffer, midiBuffer);
		}

		// Collect output samples spanning multiple periods
		std::vector<float> outputSamples;
		constexpr int collectBlocks = 8; // 1024 samples = 4 full periods

		for (int i = 0; i < collectBlocks; ++i)
		{
			fillProcessBuffer(warmupBlocks + 1 + i);
			processor.processBlock(processBuffer, midiBuffer);

			for (int s = 0; s < TestConfig::blockSize; ++s)
			{
				outputSamples.push_back(processBuffer.getSample(0, s));
			}
		}

		// Count zero crossings - sine wave should have 2 per period
		int zeroCrossings = 0;
		for (size_t i = 1; i < outputSamples.size(); ++i)
		{
			if ((outputSamples[i-1] >= 0 && outputSamples[i] < 0) ||
				(outputSamples[i-1] < 0 && outputSamples[i] >= 0))
			{
				zeroCrossings++;
			}
		}

		// For a sine wave with period 256, expect 2 zero crossings per period
		// Over 1024 samples (4 periods), expect ~8 zero crossings
		int expectedCrossings = (collectBlocks * TestConfig::blockSize / TestConfig::sinePeriod) * 2;
		INFO("Zero crossings: " << zeroCrossings << ", expected ~" << expectedCrossings);

		// Allow tolerance - output might have some anomalies at grain boundaries
		CHECK(zeroCrossings >= expectedCrossings - 6);
		CHECK(zeroCrossings <= expectedCrossings + 6);
	}

	SECTION("Audio persists over many blocks - no dropout")
	{
		// This test checks that audio doesn't cut out over time
		// Simulates real-world usage where plugin runs for extended periods
		constexpr int warmupBlocks = 30;
		constexpr int testBlocks = 100; // Simulate longer runtime

		// Warmup
		for (int callIndex = 1; callIndex <= warmupBlocks; ++callIndex)
		{
			fillProcessBuffer(callIndex);
			processor.processBlock(processBuffer, midiBuffer);
		}

		CHECK(processor.getCurrentState() == PluginProcessor::ProcessState::kTracking);

		// Process many blocks and check for audio dropout
		int blocksWithAudio = 0;
		int blocksWithoutAudio = 0;
		int consecutiveDropout = 0;
		int maxConsecutiveDropout = 0;
		int firstDropoutBlock = -1;

		for (int i = 0; i < testBlocks; ++i)
		{
			fillProcessBuffer(warmupBlocks + 1 + i);
			processor.processBlock(processBuffer, midiBuffer);

			// Check if block has meaningful audio
			int nonZeroCount = 0;
			float maxAbs = 0.0f;
			for (int s = 0; s < TestConfig::blockSize; ++s)
			{
				float sample = std::abs(processBuffer.getSample(0, s));
				if (sample > 0.01f)
					nonZeroCount++;
				maxAbs = std::max(maxAbs, sample);
			}

			bool hasAudio = (nonZeroCount > TestConfig::blockSize / 4) && (maxAbs > 0.05f);

			if (hasAudio)
			{
				blocksWithAudio++;
				consecutiveDropout = 0;
			}
			else
			{
				blocksWithoutAudio++;
				consecutiveDropout++;
				maxConsecutiveDropout = std::max(maxConsecutiveDropout, consecutiveDropout);
				if (firstDropoutBlock < 0)
					firstDropoutBlock = warmupBlocks + 1 + i;
			}
		}

		INFO("Blocks with audio: " << blocksWithAudio << " / " << testBlocks);
		INFO("Blocks without audio: " << blocksWithoutAudio);
		INFO("Max consecutive dropout: " << maxConsecutiveDropout);
		INFO("First dropout at block: " << firstDropoutBlock);
		INFO("First dropout at sample: " << (firstDropoutBlock * TestConfig::blockSize));

		// Should have audio in most blocks
		CHECK(blocksWithAudio > testBlocks * 0.9);
		// Should not have more than 2 consecutive blocks without audio
		CHECK(maxConsecutiveDropout <= 2);
	}

	SECTION("Debug: Track grain activity and synthMark progression")
	{
		// This diagnostic test helps identify why audio drops out
		constexpr int warmupBlocks = 30;
		constexpr int testBlocks = 50;

		// Access granulator to check internal state
		// Note: This requires adding a getter for the granulator in PluginProcessor

		// Warmup
		for (int callIndex = 1; callIndex <= warmupBlocks; ++callIndex)
		{
			fillProcessBuffer(callIndex);
			processor.processBlock(processBuffer, midiBuffer);
		}

		// Track state over time
		std::vector<int> activeGrainCounts;
		std::vector<float> maxAmplitudes;
		std::vector<bool> isTracking;

		for (int i = 0; i < testBlocks; ++i)
		{
			fillProcessBuffer(warmupBlocks + 1 + i);
			processor.processBlock(processBuffer, midiBuffer);

			// Record state
			isTracking.push_back(processor.getCurrentState() == PluginProcessor::ProcessState::kTracking);

			float maxAbs = 0.0f;
			for (int s = 0; s < TestConfig::blockSize; ++s)
			{
				maxAbs = std::max(maxAbs, std::abs(processBuffer.getSample(0, s)));
			}
			maxAmplitudes.push_back(maxAbs);
		}

		// Analyze results
		int trackingCount = 0;
		int detectingCount = 0;
		for (bool tracking : isTracking)
		{
			if (tracking) trackingCount++;
			else detectingCount++;
		}

		INFO("Tracking blocks: " << trackingCount);
		INFO("Detecting blocks: " << detectingCount);

		// Find pattern of audio
		int audioBlocks = 0;
		int silentBlocks = 0;
		for (float amp : maxAmplitudes)
		{
			if (amp > 0.05f) audioBlocks++;
			else silentBlocks++;
		}

		INFO("Blocks with signal: " << audioBlocks);
		INFO("Blocks silent: " << silentBlocks);

		// Check if state switches correlate with audio dropout
		// All blocks should be tracking
		CHECK(trackingCount == testBlocks);
	}
}
