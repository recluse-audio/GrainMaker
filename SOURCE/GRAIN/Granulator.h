/**
 * Made by Ryan Devens 2025-10-13
 * 
 */

 #pragma once
#include "Util/Juce_Header.h"
#include <vector>

// Suppress MSVC warning C4244 from Window.h and std::unique_ptr<Window> usage
#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable: 4244)
#endif

#include "../SUBMODULES/RD/SOURCE/Window.h"

#if defined(_MSC_VER)
    #pragma warning(pop)
#endif

#include "../SUBMODULES/RD/SOURCE/BufferRange.h"

// Number of grain buffers for double/multi-buffering
static constexpr int kDefaultNumGrainBuffers = 2;
 /**
  * @brief This class is intended to perform the read/write operations 
  * of granulating a buffer and writing it to another, getting a windowed portion of it, etc.
  * 
  */
 class Granulator
 {
public:
	Granulator(int numGrainBuffers = kDefaultNumGrainBuffers);
	~Granulator();

	void prepare(double sampleRate, int blockSize, int lookaheadSize);

	int getNumGrainBuffers() const { return static_cast<int>(mGrainBuffers.size()); }

	juce::AudioBuffer<float>& getActiveGrainBuffer();

	void granulate(juce::AudioBuffer<float>& lookaheadBuffer, juce::AudioBuffer<float>& processBlock,
				float detectedPeriod, float shiftedPeriod);

private:
	friend class GranulatorTester;
	int mProcessBlockSize = 0;
	std::vector<juce::AudioBuffer<float>> mGrainBuffers;
	int mActiveGrainBufferIndex = 0;
	// pos to read from grainBuffer and to write in processBlock
	int mGrainReadPos = 0;
	int mProcessBlockWritePos = 0;

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

	// find first peak within period of lookaheadBuffer, this becomes mCurrentAnalysisMarkIndex
	// make grain 2 periods in length by windowing centered around mCurrentAnalysisMarkIndex +/- detectedPeriod (0's if negative sample indices)
	// mCurrentSynthesisMarkIndex will be equal to mCurrentAnalysisMarkIndex
	// write grain to mGrainBuffer centered around mCurrentSynthesisMarkIndex
	// mPrevAnalysisMark = mCurrentAnalysisMark; mPrevSynthesisMarkIndex = mCurrentSynthesisMarkIndex;

	// predictedAnalysisMarkIndex = mPrevAnalysisMarkIndex + detectedPEriod
	// mCurrentAna
	
	int _getNextAnalysisMarkIndex(juce::AudioBuffer<float>& buffer, float detectedPeriod, int currentIndex);
	int _getWindowCenterIndex(juce::AudioBuffer<float>& buffer, int analysisMarkIndex, float detectedPeriod);
	int _updateCurrentSynthMarkIndex(juce::AudioBuffer<float>& buffer, float detectedPeriod, float shiftedPeriod);
	
	int mCurrentAnalysisMarkIndex = -1;
	int mCurrentWindowCenterIndex = -1;
	int mCurrentSynthMarkIndex = -1;

 };