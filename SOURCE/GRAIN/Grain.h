/**
 * Grain.h
 * Created by Ryan Devens
 *
 * Represents a single grain for TD-PSOLA processing.
 * Each grain owns its own Window for independent windowing.
 */

#pragma once
#include "../Util/Juce_Header.h"
#include "../SUBMODULES/RD/SOURCE/Window.h"

class Grain
{
public:
	Grain();
	~Grain();

	bool isActive = false;
	std::tuple<juce::int64, juce::int64> mAnalysisRange { -1, -1 };
	std::tuple<juce::int64, juce::int64> mSynthRange { -1, -1 };

	// Prepare the grain's window - call once during setup
	void prepare(int windowSize);

	// Configure window for this grain's period and reset read position
	void setWindowPeriod(int grainSize);

	// Set window read position based on phase offset (for grains that started before current block)
	void setWindowPhaseOffset(int sampleOffset);

	// Get next window sample and advance read position
	float getNextWindowSample();

	void reset();

private:
	Window mWindow;
};
