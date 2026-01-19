/**
 * Made by Ryan Devens 2025-10-13
 * 
 */

 #pragma once
#include "Util/Juce_Header.h"
#include <vector>
#include <tuple>

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

// Number of grain buffers for triple-buffering (Reading, Writing, Spillover)
static constexpr int kNumGrainBuffers = 3;

/**
 * @brief States for grain buffer management
 * - Reading: Buffer being read from for output (not written to)
 * - Writing: Buffer being filled with new grain data
 * - Spillover: Receives overflow from Writing buffer, cleared when entering this state
 */
enum class GrainBufferState
{
	Reading,
	Writing,
	Spillover
};

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

	int getNumGrainBuffers() const { return static_cast<int>(mGrainBuffers.size()); }

	juce::AudioBuffer<float>& getReadingBuffer();

	void granulate(juce::AudioBuffer<float>& lookaheadBuffer, juce::AudioBuffer<float>& processBlock,
				float detectedPeriod, float shiftedPeriod);

private:
	friend class GranulatorTester;
	int mProcessBlockSize = 0;
	std::vector<juce::AudioBuffer<float>> mGrainBuffers;

	// Buffer state indices - each points to a different buffer
	int mReadingBufferIndex = 0;   // Buffer being read from for output
	int mWritingBufferIndex = 1;   // Buffer being filled with grain data
	int mSpilloverBufferIndex = 2; // Receives overflow, cleared on transition to this state

	// pos to read from grainBuffer and to write in processBlock
	int mGrainReadPos = 0;
	int mProcessBlockWritePos = 0;

	bool mNeedToFillReading = true;

	bool _shouldGranulateToReadingBuffer();
	// if mGrainReadPos will extend past reading buffer's size then we need to granulate to writing buffer
	bool _shouldGranulateToWritingBuffer();

	// This is expected after a prepareToPlay is called or a reset occurs
	void _granulateToReadingBuffer(juce::AudioBuffer<float>& bufferToGranulate, float detectedPeriod, float shiftedPeriod);

	void _granulateToWritingBuffer(juce::AudioBuffer<float>& bufferToGranulate, float detectedPeriod, float shiftedPeriod);

	void _granulateToGrainBuffer(juce::AudioBuffer<float>& bufferToGranulate, juce::AudioBuffer<float>& grainBuffer,
								juce::AudioBuffer<float>& spilloverBuffer, float detectedPeriod, float shiftedPeriod);


	//
	void _writeFromGrainBufferToProcessBlock(juce::AudioBuffer<float>& processBlock);

	// Rotates buffer states: Reading->Spillover (cleared), Writing->Reading, Spillover->Writing (keeps data)
	void _rotateBufferStates();

	juce::AudioBuffer<float>& _getReadingBuffer();
	juce::AudioBuffer<float>& _getWritingBuffer();
	juce::AudioBuffer<float>& _getSpilloverBuffer();
	
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
	std::tuple<int, int, int> _getWindowBounds(juce::AudioBuffer<float>& buffer, int analysisMarkIndex, float detectedPeriod);
	int _getNextSynthMarkIndex(float detectedPeriod, float shiftedPeriod, int currentSynthMarkIndex, int currentAnalysisMarkIndex);
	
	int mCurrentAnalysisMarkIndex = -1;
	int mCurrentWindowCenterIndex = -1;
	int mCurrentSynthMarkIndex = -1;

 };