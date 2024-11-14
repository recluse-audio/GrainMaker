#include "GrainCorrector.h"

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
void GrainCorrector::process(juce::AudioBuffer<float>& processBuffer)
{

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

    juce::dsp::AudioBlock<float> analysisBlock(mAnalysisAudioBuffer);
    juce::dsp::AudioBlock<float> processBlock(processBuffer);
    int halfProcessBlock = processBlock.getNumSamples()/2;

    auto analysisSubBlock1 = analysisBlock.getSubBlock((size_t)mOutputDelay, (size_t)halfProcessBlock);
    auto analysisSubBlock2 = analysisBlock.getSubBlock((size_t)(mOutputDelay + halfProcessBlock), (size_t)halfProcessBlock);


    auto processSubBlock1 = processBlock.getSubBlock((size_t)0, (size_t)halfProcessBlock);
    auto processSubBlock2 = processBlock.getSubBlock((size_t)halfProcessBlock, (size_t)halfProcessBlock );

    processSubBlock1.add(analysisSubBlock1);
    processSubBlock2.add(analysisSubBlock2);



    // REFACTOR: Make this write to the processBuffer by grains taken at pitchMarks
    // and still pass the tests!!!  Then delete this.

    // now loop through mAnalysisAudioBuffer, take chunks of it
    // find the grains at pitch marks
    // make the grains one at a time using juce::AudioBlock and RD_DSP::Window
    // write the 
    // for(int sampleIndex = 0; sampleIndex < mAnalysisPitchMarksBuffer.getNumSamples(); sampleIndex++)
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
}

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