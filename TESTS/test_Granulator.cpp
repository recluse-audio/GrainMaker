#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/catch_approx.hpp>  // For Approx in Catch2 v3+


#include "../SOURCE/Granulator.h"

#include "../SUBMODULES/RD/SOURCE/BufferFiller.h"
#include "../SUBMODULES/RD/SOURCE/Window.h"
#include "../SUBMODULES/RD/SOURCE/RelativeFilePath.h"
#include "../SUBMODULES/RD/SOURCE/BufferHelper.h"
#include "../SUBMODULES/RD/SOURCE/BufferRange.h"


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

	juce::int64 getPitchShiftOffset(float detectedPeriod, float shiftRatio)
	{
		return mGranulator._calculatePitchShiftOffset(detectedPeriod, shiftRatio);
	}

	// setters aka "updaters" for Granulators internal GrainData ranges
	void updateSourceRange(const juce::AudioBuffer<float>& lookaheadBuffer) { mGranulator._updateSourceRange(lookaheadBuffer); }
	void updateOutputRange(const juce::AudioBuffer<float>& outputBuffer) { mGranulator._updateOutputRange(outputBuffer); }
	void updateNumGrainsToOutput(float detectedPeriod, float shiftRatio) { mGranulator._updateNumGrainsToOutput(detectedPeriod, shiftRatio); }
	void updateFullGrainRange(float startIndex, float lengthInSamples)	{ mGranulator._updateFullGrainRange(startIndex, lengthInSamples); }
	void updateClippedGrainRange() { mGranulator._updateClippedGrainRange(); }
	void updateShiftedRange(float detectedPeriod, float shiftRatio) { mGranulator._updateShiftedRange(detectedPeriod, shiftRatio); }

	//==========================
	void updateGrainReadRange(RD::BufferRange& readRange, const RD::BufferRange& sourceRangeNeededForShifting, float grainNumber, float detectedPeriod)
	{ 
		mGranulator._updateGrainReadRange(readRange, sourceRangeNeededForShifting, grainNumber, detectedPeriod);
	}

	//==========================
	void updateGrainWriteRange(RD::BufferRange& writeRange, const RD::BufferRange& outputRange, float grainNumber, float detectedPeriod, float periodAfterShifting) 
	{
		 mGranulator._updateGrainWriteRange(writeRange, outputRange, grainNumber, detectedPeriod,  periodAfterShifting);
	}


	void applyWindowToFullGrain(juce::dsp::AudioBlock<float>& block) { mGranulator._applyWindowToFullGrain(block);  }

	void setShiftedRange(juce::int64 start, juce::int64 length) 
	{
		 mGranulator.mCurrentGrainData.mShiftedRange.setStartIndex(start);
		 mGranulator.mCurrentGrainData.mShiftedRange.setLengthInSamples(length); 
	}

	void getWriteRangeInOutputAndSpillover(const RD::BufferRange& wholeGrainRange, 
											const RD::BufferRange& outputRange,
											RD::BufferRange& grainRangeForOutputBuffer,
											RD::BufferRange& grainRangeForSpilloverBuffer,
											RD::BufferRange& outputBufferWriteRange, 
											RD::BufferRange& spilloverBufferWriteRange	)
	{
		mGranulator._getWriteRangeInOutputAndSpillover(wholeGrainRange, outputRange, grainRangeForOutputBuffer, grainRangeForSpilloverBuffer, outputBufferWriteRange, spilloverBufferWriteRange);
	}

	void getSourceRangeNeededForNumGrains(int numGrains, float detectedPeriod
											, const RD::BufferRange& sourceRange
											, RD::BufferRange& sourceRangeForShifting)
	{
		mGranulator._getSourceRangeNeededForNumGrains(numGrains, detectedPeriod, sourceRange, sourceRangeForShifting);
	}


	//===========
	float getWindowSampleAtIndexInPeriod(int indexInPeriod, float period)
	{
		return mGranulator._getWindowSampleAtIndexInPeriod(indexInPeriod, period);
	}

	// getters for GrainData ranges used to read/write shifted grains
	RD::BufferRange& getSourceRange()       			{ return mGranulator.mCurrentGrainData.mSourceRange; }
	RD::BufferRange& getOutputRange()       			{ return mGranulator.mCurrentGrainData.mOutputRange; }
	const float& getNumGrainsToOutput()       					{ return mGranulator.mCurrentGrainData.mNumGrainsToOutput; }
	RD::BufferRange& getOutputRangeInSource()       	{ return mGranulator.mCurrentGrainData.mOutputRangeInSource; }
	RD::BufferRange& getSourceRangeNeededForShifting()   { return mGranulator.mCurrentGrainData.mSourceRangeNeededForShifting; }

	RD::BufferRange& getFullGrainRange()    { return mGranulator.mCurrentGrainData.mFullGrainRange; }
	RD::BufferRange& getClippedGrainRange() { return mGranulator.mCurrentGrainData.mClippedGrainRange; }
	RD::BufferRange& getShiftedRange()      { return mGranulator.mCurrentGrainData.mShiftedRange; }
	RD::BufferRange& getReadRange()         { return mGranulator.mCurrentGrainData.mGrainReadRange; }
	RD::BufferRange& getWriteRange()        { return mGranulator.mCurrentGrainData.mGrainWriteRange; }


private:
	Granulator& mGranulator;
};


//========================================
//
TEST_CASE("Process Shifting")
{
	Granulator granulator;
	GranulatorTester granulatorTester(granulator);
	juce::AudioBuffer<float> lookaheadBuffer (1, 1024);
	juce::AudioBuffer<float> outputBuffer (1, 512);

	lookaheadBuffer.clear();
	outputBuffer.clear();


	double sampleRate = 44100; // need this to prepare
	int blockSize = outputBuffer.getNumSamples();
	granulator.prepare(sampleRate, blockSize);

	SECTION("No shifting, no window, incremental lookahead buffer")
	{
		granulator.setGrainShape(Window::Shape::kNone);
		BufferFiller::fillIncremental(lookaheadBuffer);

		float detectedPeriod = 128.f;
		float shiftRatio = 1.f;

		granulator.processShifting(lookaheadBuffer, outputBuffer, detectedPeriod, shiftRatio);


		for(int sampleIndex = 0; sampleIndex < outputBuffer.getNumSamples(); sampleIndex++)
		{

			for(int ch = 0; ch < outputBuffer.getNumChannels(); ch++)
			{
				float indexOffset = (float)outputBuffer.getNumSamples() - detectedPeriod;
				float outputSample = outputBuffer.getSample(ch, sampleIndex);
				float expectedValue = (float)sampleIndex + indexOffset;

				//if(expectedValue != 511.f && expectedValue != 639.f && expectedValue != 767.f && expectedValue != 895.f)
				//CHECK(outputSample == expectedValue);
			}
		}

	}

	SECTION("No shifting buffer of all ones results in cycles of window values written to outputBuffer")
	{
		// BufferFiller::fillWithAllOnes(lookaheadBuffer);

		// float detectedPeriod = 128.f;
		// float shiftRatio = 1.f;

		// granulator.processShifting(lookaheadBuffer, outputBuffer, detectedPeriod, shiftRatio);

		// int indexInPeriod = 0;

		// for(int sampleIndex = 0; sampleIndex < outputBuffer.getNumSamples(); sampleIndex++)
		// {

		// 	float windowValue = granulatorTester.getWindowSampleAtIndexInPeriod(indexInPeriod, detectedPeriod);

		// 	indexInPeriod++;
		// 	if(indexInPeriod >= (int)detectedPeriod)
		// 		indexInPeriod = 0;

		// 	for(int ch = 0; ch < outputBuffer.getNumChannels(); ch++)
		// 	{
		// 		float outputSample = outputBuffer.getSample(ch, sampleIndex);
		// 		CHECK(outputSample == Catch::Approx(windowValue).epsilon(0.1));
		// 	}
		// }
	}


}

//=======================================
//
TEST_CASE("Get Source range needed for shifting by number of grains")
{
	Granulator granulator;
	GranulatorTester granulatorTester(granulator);
	RD::BufferRange fullSourceRange(0, 99);
	float detectedPeriod = 10.f;
	int numGrains = 6;
	RD::BufferRange rangeNeededForShifting;

	granulatorTester.getSourceRangeNeededForNumGrains(numGrains, detectedPeriod, fullSourceRange, rangeNeededForShifting);

	CHECK(rangeNeededForShifting.getStartIndex() == 40);
	CHECK(rangeNeededForShifting.getEndIndex() == 99);


}

//=======================================
//
TEST_CASE("Can handle grains that cross processBlocks")
{
	Granulator granulator;
	GranulatorTester granulatorTester(granulator);

	RD::BufferRange outputRange(0, 99);
	SECTION("grain fully in outputBuffer, none in spillover")
	{
		RD::BufferRange wholeGrainRange(0, 9);
		RD::BufferRange grainRangeForOutput;
		RD::BufferRange grainRangeForSpillover;
		RD::BufferRange writeRangeInOutput;
		RD::BufferRange writeRangeInSpillover;

		granulatorTester.getWriteRangeInOutputAndSpillover(wholeGrainRange, outputRange, grainRangeForOutput, grainRangeForSpillover, writeRangeInOutput, writeRangeInSpillover);
		CHECK(grainRangeForOutput.getStartIndex() == 0);
		CHECK(grainRangeForOutput.getEndIndex() == 9);

		CHECK(grainRangeForSpillover.isEmpty() == true);
		
		CHECK(writeRangeInOutput.getLengthInSamples() == grainRangeForOutput.getLengthInSamples());
		CHECK(writeRangeInSpillover.getLengthInSamples() == grainRangeForSpillover.getLengthInSamples());

	}

	SECTION("grain partially in outputBuffer")
	{
		RD::BufferRange wholeGrainRange(95, 104);
		RD::BufferRange grainRangeForOutput;
		RD::BufferRange grainRangeForSpillover;
		RD::BufferRange writeRangeInOutput;
		RD::BufferRange writeRangeInSpillover;

		granulatorTester.getWriteRangeInOutputAndSpillover(wholeGrainRange, outputRange, grainRangeForOutput, grainRangeForSpillover, writeRangeInOutput, writeRangeInSpillover);

		CHECK(grainRangeForOutput.getStartIndex() == 0);
		CHECK(grainRangeForOutput.getEndIndex() == 4);
		CHECK(grainRangeForSpillover.getStartIndex() == 5);
		CHECK(grainRangeForSpillover.getEndIndex() == 9);

		CHECK(writeRangeInOutput.getStartIndex() == 95);
		CHECK(writeRangeInOutput.getEndIndex() == 99);

		CHECK(writeRangeInSpillover.getStartIndex() == 0);
		CHECK(writeRangeInSpillover.getEndIndex() == 4);

	}
}


//=============================================================================
//========= GRANULATOR's internal ranges associated with GrainData ============
//=============================================================================
//
//=================================================================
//========================== START PER BLOCK ======================
//=================================================================

//=================================================================
//
TEST_CASE("Granulator::mCurrentGrain::mSourceRange is updated correctly")
{
	Granulator granulator;
	GranulatorTester granulatorTester(granulator);
	juce::AudioBuffer<float> buffer (1, 100);

	// must do this first
	granulatorTester.updateSourceRange(buffer);
	RD::BufferRange range = granulatorTester.getSourceRange();
	CHECK(range.getStartIndex() == (juce::int64)0);
	CHECK(range.getEndIndex() == (juce::int64)buffer.getNumSamples() - 1);
}


//=================================================================
//
TEST_CASE("Granulator::mCurrentGrain::mOutputRange is updated correctly")
{
	Granulator granulator;
	GranulatorTester granulatorTester(granulator);
	juce::AudioBuffer<float> buffer (1, 100);

	granulatorTester.updateOutputRange(buffer);
	RD::BufferRange range = granulatorTester.getOutputRange();
	CHECK(range.getStartIndex() == (juce::int64)0);
	CHECK(range.getEndIndex() == (juce::int64)buffer.getNumSamples() - 1);

}

//=================================================================
//
TEST_CASE("Granulator::mCurrentGrain::mNumGrainsToOutput is updated correctly")
{
	Granulator granulator;
	GranulatorTester granulatorTester(granulator);
	juce::AudioBuffer<float> outputBuffer (1, 50);
	granulatorTester.updateOutputRange(outputBuffer);

	float shiftRatio = 1.f;
	float detectedPeriod = 10.f;
	granulatorTester.updateNumGrainsToOutput(detectedPeriod, shiftRatio);

	const double expected = 5.f;
	const double result = granulatorTester.getNumGrainsToOutput();

	CHECK(result == expected );

}






TEST_CASE("Can apply window to grain")
{
	int grainPeriod = 128;
	int numChannels = 1;
	double sampleRate = 44100;

	Granulator granulator;
	GranulatorTester granulatorTester(granulator);
	juce::AudioBuffer<float> oneGrainBuffer (numChannels, grainPeriod);
	BufferFiller::fillWithAllOnes(oneGrainBuffer);
	juce::dsp::AudioBlock<float> oneGrainBlock(oneGrainBuffer);

	granulator.prepare(44100);
	// check all ones with no window
	for(int sampleIndex = 0; sampleIndex < grainPeriod; sampleIndex++)
	{
		
		for(int ch = 0; ch < numChannels; ch++)
		{
			auto grainSample = oneGrainBlock.getSample(ch, sampleIndex);
			CHECK(grainSample == 1.f);
		}
	}

	granulatorTester.applyWindowToFullGrain(oneGrainBlock);

	Window testWindow;
	testWindow.setSizeShapePeriod(sampleRate, Window::Shape::kHanning, grainPeriod);

	for(int sampleIndex = 0; sampleIndex < grainPeriod; sampleIndex++)
	{
		float windowSample = testWindow.getNextSample();
		for(int ch = 0; ch < numChannels; ch++)
		{
			float grainSample = oneGrainBlock.getSample(ch, sampleIndex);
			CHECK(windowSample == grainSample);
		}
	}

}



//========================== END PER BLOCK ========================
//=================================================================

//=================================================================
//













//*****************IN PROGRESS************************* */

//=======================================
//
TEST_CASE("Can calculate read/write ranges with no shifting")
{
	Granulator granulator;
	GranulatorTester granulatorTester(granulator);
	juce::AudioBuffer<float> lookaheadBuffer (1, 100);
	juce::AudioBuffer<float> outputBuffer (1, 50);


	// Step #1, this relies on source range being updated first
	granulatorTester.updateSourceRange(lookaheadBuffer);
	granulatorTester.updateOutputRange(outputBuffer);


	juce::int64 startInSourceRange = 512;
	RD::BufferRange sourceRangeNeededForShifting(startInSourceRange, 1023);
	RD::BufferRange outputRange(0, 511);
	REQUIRE(sourceRangeNeededForShifting.getLengthInSamples() == outputRange.getLengthInSamples());

	SECTION("Read/Write first grain - no shifting")
	{
		RD::BufferRange grainReadRange;
		granulatorTester.updateGrainReadRange(grainReadRange, sourceRangeNeededForShifting, 0, 128);
		CHECK(grainReadRange.getStartIndex() == startInSourceRange);
		CHECK(grainReadRange.getLengthInSamples() == 128);

		RD::BufferRange grainWriteRange;
		granulatorTester.updateGrainWriteRange(grainWriteRange, outputRange, 0, 128, 128);
		CHECK(grainWriteRange.getStartIndex() == outputRange.getStartIndex());
		CHECK(grainWriteRange.getLengthInSamples() == 128);
	}

	SECTION("Read/Write middle grain - no shifting")
	{
		float grainNum = 2.f; 
		float detectedPeriod = 128.f;
		float shiftedRatio = 1.f;

		// READ 
		RD::BufferRange grainReadRange;
		granulatorTester.updateGrainReadRange(grainReadRange, sourceRangeNeededForShifting, grainNum, detectedPeriod);
		juce::int64 expectedReadStart = sourceRangeNeededForShifting.getStartIndex() + (juce::int64)(grainNum * detectedPeriod);
		juce::int64 expectedReadLength = (juce::int64)detectedPeriod;

		CHECK(grainReadRange.getStartIndex() == expectedReadStart);
		CHECK(grainReadRange.getLengthInSamples() == expectedReadLength);

		//--------------------------------------------

		// WRITE
		float shiftedPeriod = detectedPeriod * ( 1.f / shiftedRatio);
		RD::BufferRange grainWriteRange;
		granulatorTester.updateGrainWriteRange(grainWriteRange, outputRange, grainNum, detectedPeriod, shiftedPeriod);
		
		juce::int64 expectedWriteStart = (juce::int64)(grainNum * shiftedPeriod);
		juce::int64 expectedLength = (juce::int64)detectedPeriod;
		CHECK(grainWriteRange.getStartIndex() == expectedWriteStart);
		CHECK(grainWriteRange.getLengthInSamples() == expectedLength);
	}


	SECTION("Read/Write middle grain - no shifting")
	{
		float grainNum = 3.5f; 
		float detectedPeriod = 128.f;
		float shiftedRatio = 1.f;

		// READ 
		RD::BufferRange grainReadRange;
		granulatorTester.updateGrainReadRange(grainReadRange, sourceRangeNeededForShifting, grainNum, detectedPeriod);
		juce::int64 expectedReadStart = sourceRangeNeededForShifting.getStartIndex() + (juce::int64)(grainNum * detectedPeriod);
		juce::int64 expectedReadLength = (juce::int64)(detectedPeriod / 2.f);

		CHECK(grainReadRange.getStartIndex() == expectedReadStart);
		CHECK(grainReadRange.getLengthInSamples() == expectedReadLength);

		//--------------------------------------------

		// WRITE
		float shiftedPeriod = detectedPeriod * ( 1.f / shiftedRatio);
		RD::BufferRange grainWriteRange;
		granulatorTester.updateGrainWriteRange(grainWriteRange, outputRange, grainNum, detectedPeriod, shiftedPeriod);
		
		juce::int64 expectedWriteStart = (juce::int64)(grainNum * shiftedPeriod);
		juce::int64 expectedLength = (juce::int64)(detectedPeriod / 2.f);
		CHECK(grainWriteRange.getStartIndex() == expectedWriteStart);
		CHECK(grainWriteRange.getLengthInSamples() == expectedLength);
	}

}


//=======================================
//
TEST_CASE("Can calculate read/write ranges with shifting up")
{
	Granulator granulator;
	GranulatorTester granulatorTester(granulator);

	juce::int64 startInSourceRange = 256;
	RD::BufferRange sourceRangeNeededForShifting(startInSourceRange, 1023);
	RD::BufferRange outputRange(0, 511);


	SECTION("Read/Write first grain - shifting up")
	{
		float grainNum = 0.f; 
		float detectedPeriod = 128.f;
		float shiftedRatio = 1.5f;

		RD::BufferRange grainReadRange;
		granulatorTester.updateGrainReadRange(grainReadRange, sourceRangeNeededForShifting, grainNum, detectedPeriod);
		CHECK(grainReadRange.getStartIndex() == startInSourceRange);
		CHECK(grainReadRange.getLengthInSamples() == (juce::int64)detectedPeriod);

		RD::BufferRange grainWriteRange;
		juce::int64 shiftedLength = detectedPeriod * (1.f / shiftedRatio);
		juce::int64 expectedWriteStart = shiftedLength * grainNum;
		granulatorTester.updateGrainWriteRange(grainWriteRange, outputRange, grainNum, detectedPeriod, shiftedLength);
		CHECK(grainWriteRange.getStartIndex() == outputRange.getStartIndex());
		CHECK(grainWriteRange.getLengthInSamples() == (juce::int64)detectedPeriod);
	}

	SECTION("Read/Write middle grain - shifting up")
	{
		float grainNum = 1.f; 
		float detectedPeriod = 128.f;
		float shiftedRatio = 1.5f;

		RD::BufferRange grainReadRange;
		granulatorTester.updateGrainReadRange(grainReadRange, sourceRangeNeededForShifting, grainNum, detectedPeriod);
		juce::int64 expectedReadStart = sourceRangeNeededForShifting.getStartIndex() + (juce::int64)(grainNum * detectedPeriod);
		juce::int64 expectedReadLength = (juce::int64)detectedPeriod;
		CHECK(grainReadRange.getStartIndex() == expectedReadStart);
		CHECK(grainReadRange.getLengthInSamples() == (juce::int64)detectedPeriod);

		RD::BufferRange grainWriteRange;
		juce::int64 shiftedLength = detectedPeriod * (1.f / shiftedRatio);
		juce::int64 expectedWriteStart = shiftedLength * grainNum;
		granulatorTester.updateGrainWriteRange(grainWriteRange, outputRange, grainNum, detectedPeriod, shiftedLength);
		CHECK(grainWriteRange.getStartIndex() == expectedWriteStart);
		CHECK(grainWriteRange.getLengthInSamples() == (juce::int64)detectedPeriod);
	}

	SECTION("Read/Write third grain - shifting up")
	{
		float grainNum = 2.f; 
		float detectedPeriod = 128.f;
		float shiftedRatio = 1.5f;

		RD::BufferRange grainReadRange;
		granulatorTester.updateGrainReadRange(grainReadRange, sourceRangeNeededForShifting, grainNum, detectedPeriod);
		juce::int64 expectedReadStart = sourceRangeNeededForShifting.getStartIndex() + (juce::int64)(grainNum * detectedPeriod);
		juce::int64 expectedReadLength = (juce::int64)detectedPeriod;
		CHECK(grainReadRange.getStartIndex() == expectedReadStart);
		CHECK(grainReadRange.getLengthInSamples() == expectedReadLength);

		RD::BufferRange grainWriteRange;
		juce::int64 shiftedLength = detectedPeriod * (1.f / shiftedRatio);
		juce::int64 expectedWriteStart = shiftedLength * grainNum;
		granulatorTester.updateGrainWriteRange(grainWriteRange, outputRange, grainNum, detectedPeriod, shiftedLength);
		CHECK(grainWriteRange.getStartIndex() == expectedWriteStart);
		CHECK(grainWriteRange.getLengthInSamples() == (juce::int64)detectedPeriod);
	}


	SECTION("Read/Write partial grain - shifting up")
	{
		float grainNum = 3.5f; 
		float detectedPeriod = 128.f;
		float shiftedRatio = 1.5f;

		RD::BufferRange grainReadRange;
		granulatorTester.updateGrainReadRange(grainReadRange, sourceRangeNeededForShifting, grainNum, detectedPeriod);
		juce::int64 expectedReadStart = sourceRangeNeededForShifting.getStartIndex() + (juce::int64)(grainNum * detectedPeriod);
		juce::int64 expectedReadLength = (juce::int64)detectedPeriod;		
		CHECK(grainReadRange.getStartIndex() == expectedReadStart);
		CHECK(grainReadRange.getLengthInSamples() == (juce::int64)detectedPeriod);

		RD::BufferRange grainWriteRange;
		juce::int64 shiftedLength = detectedPeriod * (1.f / shiftedRatio);
		juce::int64 expectedWriteStart = shiftedLength * grainNum;
		granulatorTester.updateGrainWriteRange(grainWriteRange, outputRange, grainNum, detectedPeriod, shiftedLength);
		CHECK(grainWriteRange.getStartIndex() == expectedWriteStart);
		CHECK(grainWriteRange.getLengthInSamples() == (juce::int64)detectedPeriod);
	}
}


//=======================================
//
TEST_CASE("Can calculate read/write ranges with shifting down")
{
	Granulator granulator;
	GranulatorTester granulatorTester(granulator);

	juce::int64 startInSourceRange = 256;
	RD::BufferRange sourceRangeNeededForShifting(startInSourceRange, 1023);
	RD::BufferRange outputRange(0, 511);


	SECTION("Read/Write first grain - shifting down")
	{
		float grainNum = 0.f; 
		float detectedPeriod = 128.f;
		float shiftedRatio = .75f;

		RD::BufferRange grainReadRange;
		granulatorTester.updateGrainReadRange(grainReadRange, sourceRangeNeededForShifting, grainNum, detectedPeriod);
		CHECK(grainReadRange.getStartIndex() == startInSourceRange);
		CHECK(grainReadRange.getLengthInSamples() == (juce::int64)detectedPeriod);

		RD::BufferRange grainWriteRange;
		juce::int64 shiftedLength = detectedPeriod * (1.f / shiftedRatio);
		juce::int64 expectedWriteStart = shiftedLength * grainNum;
		granulatorTester.updateGrainWriteRange(grainWriteRange, outputRange, grainNum, detectedPeriod, shiftedLength);
		CHECK(grainWriteRange.getStartIndex() == outputRange.getStartIndex());
		CHECK(grainWriteRange.getLengthInSamples() == (juce::int64)detectedPeriod);
	}

	SECTION("Read/Write middle grain - shifting down")
	{
		float grainNum = 1.f; 
		float detectedPeriod = 128.f;
		float shiftedRatio = .75f;

		RD::BufferRange grainReadRange;
		granulatorTester.updateGrainReadRange(grainReadRange, sourceRangeNeededForShifting, grainNum, detectedPeriod);
		juce::int64 expectedReadStart = sourceRangeNeededForShifting.getStartIndex() + (juce::int64)(grainNum * detectedPeriod);
		juce::int64 expectedReadLength = (juce::int64)detectedPeriod;
		CHECK(grainReadRange.getStartIndex() == expectedReadStart);
		CHECK(grainReadRange.getLengthInSamples() == (juce::int64)detectedPeriod);

		RD::BufferRange grainWriteRange;
		juce::int64 shiftedLength = detectedPeriod * (1.f / shiftedRatio);
		juce::int64 expectedWriteStart = shiftedLength * grainNum;
		granulatorTester.updateGrainWriteRange(grainWriteRange, outputRange, grainNum, detectedPeriod, shiftedLength);
		CHECK(grainWriteRange.getStartIndex() == expectedWriteStart);
		CHECK(grainWriteRange.getLengthInSamples() == (juce::int64)detectedPeriod);
	}

	SECTION("Read/Write third grain - shifting down")
	{
		float grainNum = 2.f; 
		float detectedPeriod = 128.f;
		float shiftedRatio = .75f;

		RD::BufferRange grainReadRange;
		granulatorTester.updateGrainReadRange(grainReadRange, sourceRangeNeededForShifting, grainNum, detectedPeriod);
		juce::int64 expectedReadStart = sourceRangeNeededForShifting.getStartIndex() + (juce::int64)(grainNum * detectedPeriod);
		juce::int64 expectedReadLength = (juce::int64)detectedPeriod; // -1 because juce range handles 0 index weird, range 0,127 is length 127
		CHECK(grainReadRange.getStartIndex() == expectedReadStart);
		CHECK(grainReadRange.getLengthInSamples() == expectedReadLength);

		RD::BufferRange grainWriteRange;
		juce::int64 shiftedLength = detectedPeriod * (1.f / shiftedRatio);
		juce::int64 expectedWriteStart = shiftedLength * grainNum;
		granulatorTester.updateGrainWriteRange(grainWriteRange, outputRange, grainNum, detectedPeriod, shiftedLength);
		CHECK(grainWriteRange.getStartIndex() == expectedWriteStart);
		CHECK(grainWriteRange.getLengthInSamples() == (juce::int64)detectedPeriod);
	}


	SECTION("Read/Write partial grain - shifting up")
	{
		float grainNum = 3.5f; 
		float detectedPeriod = 128.f;
		float shiftedRatio = 1.5f;

		RD::BufferRange grainReadRange;
		granulatorTester.updateGrainReadRange(grainReadRange, sourceRangeNeededForShifting, grainNum, detectedPeriod);
		juce::int64 expectedReadStart = sourceRangeNeededForShifting.getStartIndex() + (juce::int64)(grainNum * detectedPeriod);
		juce::int64 expectedReadLength = (juce::int64)detectedPeriod;		
		CHECK(grainReadRange.getStartIndex() == expectedReadStart);
		CHECK(grainReadRange.getLengthInSamples() == (juce::int64)detectedPeriod);

		RD::BufferRange grainWriteRange;
		juce::int64 shiftedLength = detectedPeriod * (1.f / shiftedRatio);
		juce::int64 expectedWriteStart = shiftedLength * grainNum;
		granulatorTester.updateGrainWriteRange(grainWriteRange, outputRange, grainNum, detectedPeriod, shiftedLength);
		CHECK(grainWriteRange.getStartIndex() == expectedWriteStart);
		CHECK(grainWriteRange.getLengthInSamples() == (juce::int64)detectedPeriod);
	}
}











//=======================================
//
// TEST_CASE("Granulator::mCurrentGrain::mGrainReadRange is updated correctly")
// {
// 	Granulator granulator;
// 	GranulatorTester granulatorTester(granulator);
// 	juce::AudioBuffer<float> buffer (1, 100000);

// 	// Step #1, this relies on source range being updated first
// 	granulatorTester.updateSourceRange(buffer);
// }



//====================== END RANGE TESTS ==========================
//=================================================================

//=================================================================
//===================== RANGE HELPERS =============================
//=================================================================


//===============
TEST_CASE("Can calculate correct offset due to pitch shifting.")
{
	Granulator granulator;
	GranulatorTester granulatorTester(granulator);

	SECTION("A shiftRatio of 1.f means no shifting, so we expect 0")
	{
		juce::int64 offset = granulatorTester.getPitchShiftOffset(100.f, 1.f);
		juce::int64 expectedOffset = 0;
		CHECK(offset == expectedOffset);
	}

	SECTION("A shiftRatio of 1.1f means shifting up, resulting in a positive value offset")
	{
		juce::int64 offset = granulatorTester.getPitchShiftOffset(100000.f, 1.1f);
		juce::int64 expectedOffset = 9090;
		CHECK(offset == expectedOffset);
	}

	SECTION("A shiftRatio of 0.9f means shifting down, resulting in a negative value offset")
	{
		juce::int64 offset = granulatorTester.getPitchShiftOffset(100000.f, 0.9f);
		juce::int64 expectedOffset = -11111;
		CHECK(offset == expectedOffset);
	}
}


//====================================
//===================================

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







//==============================
//==============================
//==============================
//==============================
//========== OLD and maybe de-commissioned
//==============================
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