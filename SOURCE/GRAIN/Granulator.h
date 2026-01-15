/**
 * Made by Ryan Devens 2025-10-13
 * 
 */

 #pragma once
#include "Util/Juce_Header.h"
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
	~Granulator();

	void prepare(double sampleRate, int blockSize, int lookaheadSize);

	juce::AudioBuffer<float>& getActiveGrainBuffer();

	void granulate(juce::AudioBuffer<float>& lookaheadBuffer, juce::AudioBuffer<float>& processBlock,
				float detectedPeriod, float shiftedPeriod);

	Window& getWindow() { return *mWindow.get(); }
private:
	friend class GranulatorTester;
	int mProcessBlockSize = 0;
	juce::AudioBuffer<float> mGrainBuffer1;
	juce::AudioBuffer<float> mGrainBuffer2;
	int mActiveGrainBuffer = 0;
	int mGrainWritePos = 0;
	int mGrainReadPos = 0;

	bool mNeedToFillActive = true;

	bool _shouldGranulateToActiveBuffer();
	// if mGrainReadPos will extend past active grain buffer's size then we need to granulate to inactive buffer
	bool _shouldGranulateToInactiveBuffer();

	// This is expected after a prepareToPlay is called or a reset occurs
	// normal operation we would write to inactive though.
	void _granulateToActiveGrainBuffer(juce::AudioBuffer<float>& bufferToGranulate, float detectedPeriod, float shiftedPeriod);

	void _granulateToInactiveGrainBuffer(juce::AudioBuffer<float>& bufferToGranulate, float detectedPeriod, float shiftedPeriod);

	void _granulateToGrainBuffer(juce::AudioBuffer<float>& bufferToGranulate, juce::AudioBuffer<float>& grainBuffer, float detectedPeriod, float shiftedPeriod);

	//
	void _writeFromGrainBufferToProcessBlock(juce::AudioBuffer<float>& processBlock);

	// This happens when mGrainReadPos surpasses mGrainBuffer size.
	// need to check per-sample so we know when to switch from one to the next. T
	void _switchActiveBuffer();

	juce::AudioBuffer<float>& _getActiveGrainBuffer();
	juce::AudioBuffer<float>& _getInactiveGrainBuffer();
	
	std::unique_ptr<Window> mWindow;


 };