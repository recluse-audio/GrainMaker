/**
 * Grain.h
 * Created by Ryan Devens
 *
 * Simple data struct representing a single grain for TD-PSOLA processing
 */

#pragma once
#include "../Util/Juce_Header.h"

struct Grain
{
	bool isActive = false;
	juce::int64 mAbsAnalysisMarkIndex = -1;
	juce::int64 mAbsSynthesisMarkIndex = -1;
	float detectedPeriod = 0.0f;

	void reset()
	{
		isActive = false;
		mAbsAnalysisMarkIndex = -1;
		mAbsSynthesisMarkIndex = -1;
		detectedPeriod = 0.0f;
	}
};
