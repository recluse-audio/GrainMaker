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

    auto analysisReadPtr = mAnalysisAudioBuffer.getArrayOfReadPointers();
    auto pitchMarkReadPtr = mAnalysisPitchMarksBuffer.getArrayOfReadPointers();
    auto processBufferWritePtr = processBuffer.getArrayOfWritePointers();

    for(int sampleIndex = 0; sampleIndex < processBuffer.getNumSamples(); sampleIndex++)
    {
        // TODO: What if circle buffer is not same num channels as processBuffer
        for(int ch = 0; ch < processBuffer.getNumChannels(); ch++)
        {
            auto analysisSample = analysisReadPtr[ch][sampleIndex + mOutputDelay];
            processBufferWritePtr[ch][sampleIndex] += analysisSample;
        }
    }
}


//==================
void GrainCorrector::_updateAnalysisSize()
{
    int numAnalysisSamples = mProcessBlockSize + mOutputDelay;

    // going to have to make this maxBlockSize + mOutputDelay
    mAnalysisAudioBuffer.setSize(2, numAnalysisSamples);
    mAnalysisAudioBuffer.clear();
    mAnalysisPitchMarksBuffer.setSize(1, numAnalysisSamples);
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