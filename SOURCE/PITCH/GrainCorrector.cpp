#include "GrainCorrector.h"
#include "../SUBMODULES/RD/SOURCE/BufferMath.h"

//=================
GrainCorrector::GrainCorrector(PitchMarkedCircularBuffer& pitchMarkedBuffer)
: mPitchMarkedBuffer(pitchMarkedBuffer)
{
    mWindow.setShape(Window::Shape::kHanning);
}

//=================
GrainCorrector::~GrainCorrector()
{

}

//=================
void GrainCorrector::prepare(double sampleRate, int maxBlockSize)
{
    mWindow.setSize(maxBlockSize);
    mWindow.setPeriod(128); 

    mProcessBlockSize = maxBlockSize;
    _updateAnalysisSize();

}

//=================
void GrainCorrector::setOutputDelay(int outputDelay)
{
    mOutputDelay = outputDelay;
    _updateAnalysisSize();
}

//=================
void GrainCorrector::setTransposeRatio(float newRatio)
{
    mTransposeRatio = newRatio;
}

//=================
void GrainCorrector::process(juce::AudioBuffer<float>& processBuffer)
{

    ////////////////////////////////
    // ATTEMPT ONE 
    ////////////////////////////////
    // Get audio block from 
    mPitchMarkedBuffer.popBufferAndPitch(mAnalysisAudioBuffer, mAnalysisPitchMarksBuffer);

    // copy section of audio data buffer to processBuffer 
    // grab juce::AudioBlock's from mAnalysisAudioBuffer at pitch marks (grains)
    // add these grains to processBuffer at shifted positions

    // auto analysisReadPtr = mAnalysisAudioBuffer.getArrayOfReadPointers();
    // auto pitchMarkReadPtr = mAnalysisPitchMarksBuffer.getArrayOfReadPointers();
    // auto processBufferWritePtr = processBuffer.getArrayOfWritePointers();

    // // baseline copying from analysis buffer to processBuffer
    // // we will overlay grains taken from mAnalysisAudioBuffer later
    // for(int sampleIndex = 0; sampleIndex < processBuffer.getNumSamples(); sampleIndex++)
    // {
    //     // TODO: What if circle buffer is not same num channels as processBuffer
    //     for(int ch = 0; ch < processBuffer.getNumChannels(); ch++)
    //     {
    //         auto analysisSample = analysisReadPtr[ch][sampleIndex + mOutputDelay];
    //         processBufferWritePtr[ch][sampleIndex] += analysisSample;
    //     }

    // }


    ////////////////////////////////
    // ATTEMPT TWO - Use writePtrs to write sample by sample 
    ////////////////////////////////
    // REFACTOR: Make this write to the processBuffer by grains taken at pitchMarks
    // and still pass the tests!!!  Then delete this.

    // now loop through analysisBlock, take chunks of it
    // find the grains at pitch marks
    // make the grains one at a time using juce::AudioBlock and RD_DSP::Window
    // write the 
    // for(int sampleIndex = 0; sampleIndex < analysisBlock.getNumSamples(); sampleIndex++)
    // {
    //     auto storedPeriodLength = pitchMarkReadPtr[0][sampleIndex];
    //     auto storedIndexInPeriod = pitchMarkReadPtr[1][sampleIndex];

    //     // pitches are marked by storing detected period in samples at the peak magnitude of that presumed waveform
    //     if(storedPeriodLength > 0.f)
    //     {
    //         int analysisStartIndex = sampleIndex - (storedPeriodLength / 2); // TODO: division at high rate of occurrence, eek!
    //         int analysisEndIndex = analysisStartIndex + storedPeriodLength;

    //         // juce::AudioBlock portion of the mAnalysisAudioBuffer that is one period of detected pitch in length.
    //         // making a grain with this
    //         auto pitchGrainBlock = analysisBlock.getSubBlock((size_t)analysisStartIndex, (size_t)analysisEndIndex);

    //     }


    // }


    ////////////////////////////////
    // ATTEMPT THREE - Use AudioBlocks, use subBlock for two pseudo grains
    ////////////////////////////////
    // juce::dsp::AudioBlock<float> analysisBlock(mAnalysisAudioBuffer);
    // juce::dsp::AudioBlock<float> processBlock(processBuffer);

    // int halfProcessBlock = processBlock.getNumSamples()/2;

    // auto analysisSubBlock1 = analysisBlock.getSubBlock((size_t)mOutputDelay, (size_t)halfProcessBlock);
    // auto analysisSubBlock2 = analysisBlock.getSubBlock((size_t)(mOutputDelay + halfProcessBlock), (size_t)halfProcessBlock);


    // auto processSubBlock1 = processBlock.getSubBlock((size_t)0, (size_t)halfProcessBlock);
    // auto processSubBlock2 = processBlock.getSubBlock((size_t)halfProcessBlock, (size_t)halfProcessBlock );

    // processSubBlock1.add(analysisSubBlock1);
    // processSubBlock2.add(analysisSubBlock2);




    //////////////////////////////
    // ATTEMPT FOUR - Make a subBlock at each pitch mark.  Make it the length of th evalue stored in the pitch buffer
    //////////////////////////////
    juce::dsp::AudioBlock<float> analysisBlock(mAnalysisAudioBuffer);
    juce::dsp::AudioBlock<float> markedBlock(mAnalysisPitchMarksBuffer);
    juce::dsp::AudioBlock<float> processBlock(processBuffer);

    // helps keep code clean, account for -1 and starting at index [0]
    int maxAnalysisIndex = analysisBlock.getNumSamples() - 1;
    int maxProcessBlockIndex = processBlock.getNumSamples() - 1;

    // loop analysisBlock 
    for(int analysisIndex = 0; analysisIndex <= maxAnalysisIndex; analysisIndex++)
    {
        // this will be equal to detected period length at the max sample index of a periodic waveform
        auto periodMarkerValue = markedBlock.getSample(0, analysisIndex);

        if(periodMarkerValue > 0.f)
        {
            // window for grain starts 1/2 period length before marked index, and ends 1/2 period after
            int halfPeriodLength = periodMarkerValue / 2;
            int readStartIndex = analysisIndex - halfPeriodLength;
            int readEndIndex = analysisIndex + halfPeriodLength;

            // keep things in range
            if( readStartIndex < 0 )
                readStartIndex = 0;
            if( readEndIndex > maxAnalysisIndex )
                readEndIndex = maxAnalysisIndex;

            int analysisSubBlockLength = readEndIndex - readStartIndex;
            
            int transposeOffset = (mTransposeRatio - 1.f) * (float)periodMarkerValue;

            // where the analysis block should go after transposition
            int transposedStartIndex = readStartIndex + transposeOffset;
            int transposedEndIndex = readEndIndex + transposeOffset;






            // nothing will be written to processBlock b/c it is entirely in the look-a-head
            if(transposedEndIndex < mOutputDelay)
                continue;

            // keep things in range

            // Normalizes mAnalysisAudioBuffer with the processBuffer so that mAnalysisAudioBuffer[mOutputDelay] lines up with processBuffer[0]
            int writeStartIndex = transposedStartIndex - mOutputDelay;
            int writeEndIndex = transposedEndIndex - mOutputDelay;
            
            
            /// DELETE TEST TEST TEST
            maxProcessBlockIndex = 32;

            if(writeStartIndex < 0)
            {
                writeStartIndex = 0;
            }
            if( writeEndIndex > maxProcessBlockIndex )
                writeEndIndex = maxProcessBlockIndex;

            // might be less than zero if a pitch period is not at all going to the processBlock()
            // "inside" the output delay
            int writeLength = (writeEndIndex - writeStartIndex) + 1; // need a length of 1 for single index

            // There is something to write to the processBlock
            if(writeLength >= 1)
            {
                // only want to read range that will end up being written to processBlock
                int trimmedReadSamples = (transposedStartIndex - mOutputDelay) * (-1.f);

                if (trimmedReadSamples > 0)
                    readStartIndex += trimmedReadSamples; //




                // bool fullReadBlock = transposedStartIndex - mOutputDelay >= 0;

                
                if(readStartIndex != 224)
                {
                    DBG("TEST TEST TEST PitchMarkedCircularBuffer::process().");
                    DBG(readStartIndex);
                }

                // readStartIndex needs to be mapped to processBlock's context i.e. factor in the mOutputDelay
                auto analysisSubBlock = analysisBlock.getSubBlock(readStartIndex, writeLength);
                auto processSubBlock = processBlock.getSubBlock(writeStartIndex, writeLength);
                //processSubBlock.copyFrom(analysisSubBlock);
                // auto analysisSubBlock = analysisBlock.getSubBlock(224, 32);
                // auto processSubBlock = processBlock.getSubBlock(0, 32);
                processSubBlock.add(analysisSubBlock);


            }
        }
    }
    

}

// //==================
// juce::dsp::AudioBlock<float> _getAnalysisSubBlock(juce::dsp::AudioBlock<float> analysisBlock, int analysisIndex, int period)
// {
//     int maxAnalysisIndex = analysisBlock.getNumSamples() - 1;
//     // window for grain starts 1/2 period length before marked index, and ends 1/2 period after
//     int halfPeriodLength = period / 2;
//     int readStartIndex = analysisIndex - halfPeriodLength;
//     int readEndIndex = analysisIndex + halfPeriodLength;

//     // keep things in range
//     if( readStartIndex < 0 )
//         readStartIndex = 0;
//     if( readEndIndex > maxAnalysisIndex )
//         readEndIndex = maxAnalysisIndex;

//     int readLength = readEndIndex - readStartIndex;

//     return analysisBlock.getSubBlock(readStartIndex, readLength)
// }

// //==================
// juce::dsp::AudioBlock<float> GrainCorrector::_writeGrainToProcessBlock(juce::dsp::AudioBlock<float> processBlock, 
//                                                                         juce::dsp::AudioBlock<float> analysisSubBlock )
// {
//     // make a subBlock of the range of the processBlock that is to be written to
//     int delayedStart = analysisSubBlock.get
// }

//==================
// void GrainCorrector::_makeGrain()
// {

// }


//==================
void GrainCorrector::_updateAnalysisSize()
{
    int numAnalysisSamples = mProcessBlockSize + mOutputDelay;

    // going to have to make this maxBlockSize + mOutputDelay
    mAnalysisAudioBuffer.setSize(2, numAnalysisSamples);
    mAnalysisAudioBuffer.clear();
    mAnalysisPitchMarksBuffer.setSize(2, numAnalysisSamples);
    mAnalysisPitchMarksBuffer.clear();
}




























//=================
void GrainCorrector::correct(juce::AudioBuffer<float>& audioBuffer, juce::AudioBuffer<float>& pitchBuffer)
{
    auto buffRead = audioBuffer.getArrayOfReadPointers();
    auto buffWrite = audioBuffer.getArrayOfWritePointers();
    mWindow.reset();

    float transposeRatio = 1.f;

    for(int sampleIndex = 0; sampleIndex < audioBuffer.getNumSamples(); sampleIndex++)
    {
        auto windowSample = mWindow.getNextSample();

        
        for(int ch = 0; ch < audioBuffer.getNumChannels(); ch++)
        {
            auto audioSample = buffRead[ch][sampleIndex];
            
            // index that we will write grain to
            int grainIndex = 0;

            // this will be from current segment if going down, next segment if going up
            auto grainSample = 0.f;

            // Transposing up so take from next segment
            if(transposeRatio > 1.f)
            {
                // transposeRatio of 1.1 means we want grain index 10% from end of current segment
                grainIndex = (mWindow.getSize() / transposeRatio) + sampleIndex;

                // TODO: Handle buffer overrun

                grainSample = buffRead[ch][grainIndex] * mWindow.getNextSample();

            }
            else if(transposeRatio < 1.f)
            {

            }
        }

    }

}