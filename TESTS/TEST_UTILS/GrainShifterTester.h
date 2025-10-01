#include "../../SOURCE/PITCH/GrainShifter.h"

/** Friend class used to test private functions of GrainShifter */
class GrainShifterTester
{
public:

    static juce::int64 callCalculateFirstGrainStartPos(GrainShifter& g, juce::int64 prevShiftedPeriod, const RD::BufferRange& prevOutputRange, const RD::BufferRange& prevWriteRange)
    {
        return g._calculateFirstGrainStartingPos(prevShiftedPeriod, prevOutputRange, prevWriteRange);
    }

    static int callCalculateNumGrainsToOutput(GrainShifter& g, float detectedPeriod, float shiftRatio, const RD::BufferRange& outputRange, const juce::int64 firstGrainStartPos)
    {
        return g._calculateNumGrainsToOutput( detectedPeriod, shiftRatio, outputRange, firstGrainStartPos);
    }

    static void callGetSourceRangeNeededForNumGrains(GrainShifter& g, int numGrains, float detectedPeriod, const RD::BufferRange& sourceRange, RD::BufferRange& needed)
    {
        g._updateSourceRangeNeededForNumGrains(numGrains, detectedPeriod, sourceRange, needed);
    }


    static void callUpdateGrainReadRange(GrainShifter& g, RD::BufferRange& readRange, const RD::BufferRange& sourceRangeNeeded, float grainNumber, float detectedPeriod)
    {
        g._updateGrainReadRange(readRange, sourceRangeNeeded, grainNumber, detectedPeriod);
    }

    static void callUpdateGrainWriteRange(GrainShifter& g, RD::BufferRange& writeRange, const RD::BufferRange& outputRange, float grainNumber, float detectedPeriod, float periodAfterShifting)
    {
        g._updateGrainWriteRange(writeRange, outputRange, grainNumber, detectedPeriod, periodAfterShifting);
    }

    static void callApplyWindowToFullGrain(GrainShifter& g, juce::dsp::AudioBlock<float>& block)
    {
        g._applyWindowToFullGrain(block);
    }

	static float getWindowSampleAtIndexInPeriod(GrainShifter& g, juce::int64 indexInPeriod, float period)
    {
        g._getWindowSampleAtIndexInPeriod(indexInPeriod, period);
    }

	static double getSampleRate(GrainShifter& g)
	{
		return g.mSampleRate;
	}

};
