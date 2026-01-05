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


	/**
	 * @brief Granulates an input buffer and writes to output buffer with specified grain and emission periods
	 * As of 2025-10-21 I am deciding to make this use a "varispeed/tape speedup" technique as opposed to a "repeat grain" type granulating
	 * This means when shifting up, if the buffer to granulate is not big enough you will end up with zeroes at the end of bufferToWriteTo
	 * @param bufferToGranulate Source buffer to read grains from
	 * @param bufferToWriteTo Output buffer to write granulated audio to
	 * @param grainPeriod Size of each grain in samples
	 * @param emissionPeriod Distance between grain start positions in output
	 * @param window Window function to apply to grains
	 * @return Normalized phase position (0.0-1.0) representing completion percentage of final grain written
	 */
	static float granulateBuffer(juce::AudioBuffer<float>& bufferToGranulate, juce::AudioBuffer<float>& bufferToWriteTo,
		float grainPeriod, float emissionPeriod, Window& window, bool timePreserving = false);




private:
	float mFinalGrainNormalizedPhase = 1.f;
	// Window is no longer stored internally - passed by reference to granulateBuffer

 };