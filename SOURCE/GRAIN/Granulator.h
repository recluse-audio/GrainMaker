/**
 * Made by Ryan Devens 2025-10-13
 * 
 */

 #pragma once
#include "Util/Juce_Header.h"
#include "GrainBuffer.h"
#include "../SUBMODULES/RD/SOURCE/Window.h"
#include "../SUBMODULES/RD/SOURCE/BufferRange.h"
 /**
  * @brief This class is intended to perform the read/write operations 
  * of granulating a buffer and writing it to another, getting a windowed portion of it, etc.
  * 
  */
 class Granulator
 {
public:
	Granulator();
	~Granulator() = default;

	void granulateBuffer(juce::AudioBuffer<float>& bufferToGranulate, juce::AudioBuffer<float>& bufferToWriteTo,
		float grainPeriod, float emissionPeriod, Window::Shape windowShape);

private:
	Window mWindow;

 };