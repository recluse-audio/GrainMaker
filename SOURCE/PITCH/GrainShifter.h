/**
 * @file GrainShifter.h
 * @author Ryan Devens
 * @brief 
 * @version 0.1
 * @date 2025-04-15
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#pragma once
#include "Util/Juce_Header.h"
#include "../SUBMODULES/RD/SOURCE/Window.h"

 /**
  * @brief 
  * 
  */
 class GrainShifter
 {
public:
	GrainShifter();
	~GrainShifter();

	// won't die if buffers are stupidly sized, but will simply write 0's
	void process(juce::AudioBuffer<float>& inputBuffer, juce::AudioBuffer<float>& outputBuffer, float detectedPeriod, float shiftRatio);
private:

	Window mWindow;
 };