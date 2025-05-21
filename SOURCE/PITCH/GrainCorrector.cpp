#include "GrainCorrector.h"
#include "../SUBMODULES/RD/SOURCE/BufferMath.h"

//=================
GrainCorrector::GrainCorrector(PitchMarkedCircularBuffer& pitchMarkedBuffer)
: mPitchMarkedBuffer(pitchMarkedBuffer)
{
    mWindow.setShape(Window::Shape::kHanning);
	mWindow.setLooping(true);
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
    mAnalysisAudioBuffer.clear();
    mAnalysisPitchMarksBuffer.clear();

    mPitchMarkedBuffer.popBufferAndPitch(mAnalysisAudioBuffer, mAnalysisPitchMarksBuffer);

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
            int readStartIndex = analysisIndex;
            int readEndIndex = analysisIndex + periodMarkerValue;

            // keep things in range
            if( readStartIndex < 0 )
                readStartIndex = 0;
            if( readEndIndex > maxAnalysisIndex )
                readEndIndex = maxAnalysisIndex;

            int analysisSubBlockLength = readEndIndex - readStartIndex;
            
            int transposeOffset = (mTransposeRatio - 1.f) * (float)periodMarkerValue * -1;

            if(transposeOffset == 0)
                DBG("zero offset");
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