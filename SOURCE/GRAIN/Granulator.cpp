/**
 * Granulator.cpp
 * Created by Ryan Devens on 2025-10-13
 */

// Suppress MSVC warning C4244 from std::make_unique<Window> template instantiation in <memory>
#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable: 4244)
#endif

#include "Granulator.h"
#include "../SUBMODULES/RD/SOURCE/BufferRange.h"
#include "../SUBMODULES/RD/SOURCE/BufferHelper.h"

Granulator::Granulator()
	: mGrainBuffers(kNumGrainBuffers)
{
	mWindow = std::make_unique<Window>();
	//mWindow->setLooping(true);
}

Granulator::~Granulator()
{
	mWindow.reset();

}

#if defined(_MSC_VER)
    #pragma warning(pop)
#endif

//====================================
void Granulator::prepare(double sampleRate, int blockSize, int lookaheadSize)
{
	mProcessBlockSize = blockSize;

	for (auto& grainBuffer : mGrainBuffers)
	{
		grainBuffer.clear();
		grainBuffer.setSize(2, lookaheadSize);
	}

	// Initialize buffer state indices
	mReadingBufferIndex = 0;
	mWritingBufferIndex = 1;
	mSpilloverBufferIndex = 2;

	mNeedToFillReading = true;
	mGrainReadPos = 0;
	mWindow->setSizeShapePeriod(static_cast<int>(sampleRate), Window::Shape::kHanning, blockSize); // period updated after pitch detection
	mWindow->setLooping(true);
}


//=======================================
void Granulator::granulate(juce::AudioBuffer<float>& lookaheadBuffer, juce::AudioBuffer<float>& processBlock,
							float detectedPeriod, float shiftedPeriod)
{
	if(_shouldGranulateToReadingBuffer())
	{
		_granulateToReadingBuffer(lookaheadBuffer, detectedPeriod, shiftedPeriod);
		mNeedToFillReading = false;
	}
	else if(_shouldGranulateToWritingBuffer())
	{
		_granulateToWritingBuffer(lookaheadBuffer, detectedPeriod, shiftedPeriod);
	}

	// Rotation of buffer states happens when we've read past the reading buffer
	int grainBufferSize = _getReadingBuffer().getNumSamples();
	if(mGrainReadPos >= grainBufferSize)
	{
		mGrainReadPos = mGrainReadPos - grainBufferSize; // wrap read pos, all grain buffers are same size
		_rotateBufferStates();
	}

	_writeFromGrainBufferToProcessBlock(processBlock);
}

//=======================================
bool Granulator::_shouldGranulateToReadingBuffer()
{
	return mNeedToFillReading;
}

//=======================================
bool Granulator::_shouldGranulateToWritingBuffer()
{
	bool shouldGranulate = false;

	// this function determines if we will run out of grain data in the current block
	// block size of 256 samples will span from 0-255.
	// So if we are starting at index 768 in grainBuffer and block size is 256,
	// we won't granulate writing buffer until next block.
	// b/c we have all the grain data needed to finish out current process block with reading buffer
	int finalGrainReadPosThisBlock = mGrainReadPos + mProcessBlockSize - 1;

	// we are about to read beyond the end of the reading buffer
	// in this processBlock, so granulate the writing buffer and be ready to rotate.
	if(finalGrainReadPosThisBlock >= _getReadingBuffer().getNumSamples() || mGrainReadPos == 0)
	{
		shouldGranulate = true;
	}

	return shouldGranulate;
}


//=======================================
void Granulator::_granulateToReadingBuffer(juce::AudioBuffer<float>& bufferToGranulate, float detectedPeriod, float shiftedPeriod)
{
	_granulateToGrainBuffer(bufferToGranulate, _getReadingBuffer(), _getWritingBuffer(), detectedPeriod, shiftedPeriod);
}

//=======================================
void Granulator::_granulateToWritingBuffer(juce::AudioBuffer<float>& bufferToGranulate, float detectedPeriod, float shiftedPeriod)
{
	_granulateToGrainBuffer(bufferToGranulate, _getWritingBuffer(), _getSpilloverBuffer(), detectedPeriod, shiftedPeriod);
}

//=======================================
void Granulator::_granulateToGrainBuffer(juce::AudioBuffer<float>& bufferToGranulate, juce::AudioBuffer<float>& grainBuffer,
										 juce::AudioBuffer<float>& spilloverBuffer, float detectedPeriod, float shiftedPeriod)
{
	mWindow->setPeriod(static_cast<int>(detectedPeriod));



	// we are concerned with the grains we need to write to the grainBuffer/spillover
	// if shifting down we may not read all the lookahead.
	// shifting up we have more synth indices than analysis
	while (mCurrentSynthMarkIndex < grainBuffer.getNumSamples())
	{
		mCurrentAnalysisMarkIndex = _getNextAnalysisMarkIndex(bufferToGranulate, detectedPeriod, mCurrentAnalysisMarkIndex);
		mCurrentSynthMarkIndex = _getNextSynthMarkIndex(detectedPeriod, shiftedPeriod, mCurrentSynthMarkIndex, mCurrentAnalysisMarkIndex);

		auto [analStart, analCenter, analEnd] = _getAnalysisBounds(bufferToGranulate, mCurrentAnalysisMarkIndex, detectedPeriod);
		auto [synthStart, synthEnd] = _getSynthBounds(mCurrentSynthMarkIndex, detectedPeriod);

		int sourceBufferSize = bufferToGranulate.getNumSamples();
		int destBufferSize = grainBuffer.getNumSamples();

		// Rectify analysis (read) bounds to valid source buffer range
		int rectifiedAnalStart = analStart >= 0 ? analStart : 0;
		int rectifiedAnalEnd = analEnd < sourceBufferSize ? analEnd : sourceBufferSize - 1;
		int analStartClipOffset = rectifiedAnalStart - analStart;

		// Rectify synth (write) bounds to valid dest buffer range
		int rectifiedSynthStart = synthStart >= 0 ? synthStart : 0;
		int rectifiedSynthEnd = synthEnd < destBufferSize ? synthEnd : destBufferSize - 1;
		int synthStartClipOffset = rectifiedSynthStart - synthStart;

		RD::BufferRange readRange;
		readRange.setStartIndex(rectifiedAnalStart);
		readRange.setEndIndex(rectifiedAnalEnd);

		RD::BufferRange writeRange;
		writeRange.setStartIndex(rectifiedSynthStart);
		writeRange.setEndIndex(rectifiedSynthEnd);

		// Get the grain as an audio block from the source buffer (rectified range only)
		juce::dsp::AudioBlock<float> grainBlock = BufferHelper::getRangeAsBlock(bufferToGranulate, readRange);

		// Set window phase based on how much was clipped from analysis start
		double phaseIncrement = (double)mWindow->getSize() / (double)mWindow->getPeriod();
		double windowPhase = analStartClipOffset * phaseIncrement;
		mWindow->setReadPos(windowPhase);

		// Apply window starting at correct phase
		BufferHelper::applyWindowToBlock(grainBlock, *mWindow.get());

		// Handle misaligned clipping between read and write ranges
		// Sample at grainBlock[i] came from (analStart + analStartClipOffset + i)
		// and should write to (synthStart + analStartClipOffset + i)
		juce::dsp::AudioBlock<float> blockToWrite = grainBlock;
		int blockOffset = 0;
		int actualWriteStart = rectifiedSynthStart;

		if (synthStartClipOffset > analStartClipOffset)
		{
			// Synth clipped more at start - skip samples from grainBlock
			blockOffset = synthStartClipOffset - analStartClipOffset;
		}
		else if (analStartClipOffset > synthStartClipOffset)
		{
			// Analysis clipped more at start - shift write position forward
			actualWriteStart = rectifiedSynthStart + (analStartClipOffset - synthStartClipOffset);
		}

		// Calculate how many samples we can actually write
		int grainBlockSize = static_cast<int>(grainBlock.getNumSamples());
		int availableSamples = grainBlockSize - blockOffset;
		int writeSpaceAvailable = rectifiedSynthEnd - actualWriteStart + 1;
		int samplesToWrite = std::min(availableSamples, writeSpaceAvailable);

		if (samplesToWrite > 0 && blockOffset < grainBlockSize)
		{
			blockToWrite = grainBlock.getSubBlock(
				static_cast<size_t>(blockOffset),
				static_cast<size_t>(samplesToWrite)
			);

			writeRange.setStartIndex(actualWriteStart);
			writeRange.setEndIndex(actualWriteStart + samplesToWrite - 1);

			// Write the grain block to the output buffer
			BufferHelper::writeBlockToBuffer(grainBuffer, blockToWrite, writeRange);
		}
	}
}

//==========================================
int Granulator::_getNextAnalysisMarkIndex(juce::AudioBuffer<float>& lookaheadBuffer, float detectedPeriod, int currentIndex)
{
	int updatedIndex = currentIndex;

	if(updatedIndex < 0)
	{
		updatedIndex = BufferHelper::getPeakIndex(lookaheadBuffer, 0, (int)detectedPeriod);
	}
	else
	{
		updatedIndex = updatedIndex + (int)detectedPeriod;
	}

	// wrap around lookaheadBuffer
	if(updatedIndex >= lookaheadBuffer.getNumSamples())
		updatedIndex = updatedIndex - lookaheadBuffer.getNumSamples();

	return updatedIndex;

}

//==========================================
std::tuple<int, int, int> Granulator::_getAnalysisBounds(juce::AudioBuffer<float>& buffer, int analysisMarkIndex, float detectedPeriod)
{
	int center = _getWindowCenterIndex(buffer, analysisMarkIndex, detectedPeriod);
	int start = center - detectedPeriod;
	int end = center + detectedPeriod - 1;
	return {start, center, end};
}

//==========================================
std::tuple<int, int> Granulator::_getSynthBounds(int synthMarkIndex, float detectedPeriod)
{
	int center = synthMarkIndex;
	int start = center - detectedPeriod;
	int end = center + detectedPeriod - 1;
	return {start, end};
}

//==========================================
int Granulator::_getWindowCenterIndex(juce::AudioBuffer<float>& buffer, int analysisMarkIndex, float detectedPeriod)
{
	int centerIndex = analysisMarkIndex;

	int radius = (int)(detectedPeriod / 4.f);
	centerIndex = BufferHelper::getPeakIndex(buffer, centerIndex - radius, centerIndex + radius, centerIndex);

	return centerIndex;
}



//==========================================
int Granulator::_getNextSynthMarkIndex(float detectedPeriod, float shiftedPeriod,
									int currentSynthMarkIndex, int currentAnalysisMarkIndex)
{
	int nextSynthMarkIndex = 0;
	if(currentSynthMarkIndex < 0)
	{
		nextSynthMarkIndex = currentAnalysisMarkIndex; // first one lines up
	}
	else
	{
		nextSynthMarkIndex = currentSynthMarkIndex + (int)shiftedPeriod;
	}

	return nextSynthMarkIndex;
}

//==========================================
int Granulator::_getWrappedSynthMarkIndex(juce::AudioBuffer<float>& buffer, int synthMarkIndex)
{
	int bufferSize = buffer.getNumSamples();
	int wrappedIndex = synthMarkIndex;

	if (wrappedIndex >= bufferSize)
		wrappedIndex = wrappedIndex - bufferSize;

	return wrappedIndex;
}

//=======================================
void Granulator::_writeFromGrainBufferToProcessBlock(juce::AudioBuffer<float>& processBlock)
{
	for(int sampleIndex = 0; sampleIndex < processBlock.getNumSamples(); sampleIndex++)
	{
		for(int ch = 0; ch < processBlock.getNumChannels(); ch++)
		{
			float grainSample = _getReadingBuffer().getSample(ch, mGrainReadPos);
			processBlock.setSample(ch, sampleIndex, grainSample);
		}
		mGrainReadPos++;
	}
}

//=======================================
void Granulator::_rotateBufferStates()
{
	// Save current indices before rotation
	int oldReadingIndex = mReadingBufferIndex;
	int oldWritingIndex = mWritingBufferIndex;
	int oldSpilloverIndex = mSpilloverBufferIndex;

	// Clear the buffer that is becoming Spillover (the old Reading buffer)
	mGrainBuffers[oldReadingIndex].clear();

	// Rotate states:
	// - Old Reading -> new Spillover (just cleared)
	// - Old Writing -> new Reading (keeps data, will be read from)
	// - Old Spillover -> new Writing (keeps spillover data, new writes add to it)
	mReadingBufferIndex = oldWritingIndex;
	mWritingBufferIndex = oldSpilloverIndex;
	mSpilloverBufferIndex = oldReadingIndex;
}

//=======================================
juce::AudioBuffer<float>& Granulator::getReadingBuffer()
{
	return _getReadingBuffer();
}

//=======================================
juce::AudioBuffer<float>& Granulator::_getReadingBuffer()
{
	return mGrainBuffers[mReadingBufferIndex];
}

//=======================================
juce::AudioBuffer<float>& Granulator::_getWritingBuffer()
{
	return mGrainBuffers[mWritingBufferIndex];
}

//=======================================
juce::AudioBuffer<float>& Granulator::_getSpilloverBuffer()
{
	return mGrainBuffers[mSpilloverBufferIndex];
}

































