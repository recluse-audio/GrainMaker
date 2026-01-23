#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PITCH/PitchDetector.h"
#include "GRAIN/Granulator.h"
#include "GRAIN/AnalysisMarker.h"
#include "../SUBMODULES/RD/SOURCE/CircularBuffer.h"
#include "../SUBMODULES/RD/SOURCE/BufferHelper.h"


//==============================================================================
PluginProcessor::PluginProcessor()
: AudioProcessor (_getBusesProperties())
, apvts(*this, nullptr, "Parameters", _createParameterLayout())
{
    mPitchDetector = std::make_unique<PitchDetector>();
    mCircularBuffer = std::make_unique<CircularBuffer>();
	mGranulator = std::make_unique<Granulator>();

	mShiftRatio = 1.f;

    _initParameterListeners();

}

//=================================
//
PluginProcessor::~PluginProcessor()
{
	mCircularBuffer.reset();
    mPitchDetector.reset();
    mGranulator.reset();
	mAnalysisMarker.reset();
}

//==============================================================================
const juce::String PluginProcessor::getName() const
{
    return "GrainMaker";
}

bool PluginProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PluginProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int PluginProcessor::getCurrentProgram()
{
    return 0;
}

void PluginProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String PluginProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
	// be atleast minLookaheadSize, if at or above, use 2x block size
	int pitchDetectBufferNumSamples = samplesPerBlock >= MagicNumbers::minDetectionSize ? (samplesPerBlock * 2) : MagicNumbers::minLookaheadSize;
	// scale for sample rates, we deal with the same size for 44100 and 48000 for now (same for 88200 and 96000)
	if(sampleRate > 48000.0 && sampleRate <= 96000.0)
		pitchDetectBufferNumSamples = pitchDetectBufferNumSamples * 2;
	else if(sampleRate > 96000.0)
		pitchDetectBufferNumSamples = pitchDetectBufferNumSamples * 4;

	mDetectionBuffer.clear();
	mDetectionBuffer.setSize(getTotalNumOutputChannels(), pitchDetectBufferNumSamples);

    mPitchDetector->prepareToPlay(sampleRate, pitchDetectBufferNumSamples);

    mCircularBuffer->setSize(getTotalNumOutputChannels(), static_cast<int>(pitchDetectBufferNumSamples) * 2); // by default 2 seconds
    //mCircularBuffer->setDelay(MagicNumbers::minLookaheadSize);  // delay is factored in as part of getAnalysisReadRange

    mGranulator->prepare(sampleRate, samplesPerBlock, pitchDetectBufferNumSamples);

	mSamplesProcessed = 0;
	mBlockSize = samplesPerBlock;
    mPredictedNextAnalysisMark = -1;
}

void PluginProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;
    [[maybe_unused]] auto totalNumInputChannels  = getTotalNumInputChannels();
    [[maybe_unused]] auto totalNumOutputChannels = getTotalNumOutputChannels();

    // // write audio to circular buffer
    // bool writeSuccess = mCircularBuffer->pushBuffer(buffer);
	// if(!writeSuccess)
	// 	return;

    // // invalid values, bail out

    // // clean up buffers, about to fill
	// buffer.clear();
    // mDetectionBuffer.clear();

    // ProcessState prevProcessState = mProcessState;

    // // range we will detect on
    // auto [detectStart, detectEnd] = getDetectionRange();
    // mCircularBuffer->readRange(mDetectionBuffer, detectStart);

    // // Try and detect pitch, update state accordingly in temp variable for now
    // float detected_period = mPitchDetector->process(mDetectionBuffer);
    // if(detected_period > 2)
    //     mProcessState = ProcessState::kTracking;
    // else
    //     mProcessState = ProcessState::kDetecting;

    // auto processCounterRange = getProcessCounterRange();

    // if(mProcessState == ProcessState::kDetecting)
    // {
    //     if(prevProcessState == ProcessState::kTracking)
    //         mPredictedNextAnalysisMark = -1;

    //     auto dryBufferRange = getDryBlockRange();
    //     mGranulator->processDetecting(buffer, *mCircularBuffer.get(), dryBufferRange, processCounterRange);
    // }
    // else if (mProcessState == ProcessState::kTracking)
    // {
    //     float shiftedPeriod = detected_period / mShiftRatio;
    //     std::tuple<juce::int64, juce::int64> peakRange{0, 0};

    //     // if we just started tracking, we need to find the first peak range 
    //     if(prevProcessState == ProcessState::kDetecting)
    //         peakRange = getFirstPeakRange(detected_period);
    //     else
    //         peakRange = getPrecisePeakRange(mPredictedNextAnalysisMark, detected_period);

    //     juce::Range<juce::int64> peakRangeInCircularBuffer;
    //     peakRangeInCircularBuffer.setStart(std::get<0>(peakRange));
    //     peakRangeInCircularBuffer.setEnd(std::get<1>(peakRange));
    //     juce::int64 markedIndex = mCircularBuffer->findPeakInRange(peakRangeInCircularBuffer);
    //     mPredictedNextAnalysisMark = markedIndex + shiftedPeriod;

    //     auto analysisReadRange = getAnalysisReadRange(markedIndex, detected_period);
    //     auto analysisWriteRange = getAnalysisWriteRange(analysisReadRange);

    //     mGranulator->processTracking(buffer, *mCircularBuffer.get(), analysisReadRange, analysisWriteRange, getProcessCounterRange(), detected_period, shiftedPeriod);
    // }


	mSamplesProcessed += buffer.getNumSamples();
}

//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

//==============================================================================
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::ignoreUnused (destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    juce::ignoreUnused (data, sizeInBytes);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}



//===============================================================================
//
const float PluginProcessor::getLastDetectedPitch()
{
    float pitch = static_cast<float>(this->getSampleRate() / mPitchDetector->getCurrentPeriod());
    return pitch;
}

//===============================================================================
//
juce::AudioProcessorValueTreeState& PluginProcessor::getAPVTS()
{
    return apvts;
}

//===============================================================================
//
void PluginProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if(parameterID == "shift ratio")
    {
        float clampedValue = juce::jlimit(0.5f, 1.5f, newValue);
		mShiftRatio = clampedValue;
    }
    else if(parameterID == "emission rate")
    {
    }
}

//==================================
// PRIVATE
//==================================

//===================
//
juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::_createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Add a gain parameter as an example
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "shift ratio",         // Parameter ID
        "Shift Ratio",         // Parameter name
        0.5,           // Min value
        1.5f,           // Max value
        1.f));         // Default value

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "emission rate",         // Parameter ID
        "Emission Rate",         // Parameter name
        1.f,           // Min value
        400.f,           // Max value
        1.f));         // Default value
        
    return { params.begin(), params.end() };
}


//====================
//
juce::AudioProcessor::BusesProperties PluginProcessor::_getBusesProperties()
{
    return BusesProperties()
                .withInput("Input", juce::AudioChannelSet::stereo(), true)
                .withOutput("Output", juce::AudioChannelSet::stereo(), true);
}

//===================
void PluginProcessor::_initParameterListeners()
{
    apvts.addParameterListener("shift ratio", this);
    apvts.addParameterListener("emission rate", this);
}

//-------------------------------------------
std::tuple<juce::int64, juce::int64> PluginProcessor::getProcessCounterRange()
{
    // where we will be at the end of this block
    juce::int64 startProcessSample = mSamplesProcessed;
    juce::int64 endProcessSample = startProcessSample + mBlockSize - 1; 

    return std::make_tuple(startProcessSample, endProcessSample);
}

//-------------------------------------------
std::tuple<juce::int64, juce::int64> PluginProcessor::getDetectionRange()
{
    // where we will be at the end of this block
    juce::int64 endProcessSample = mSamplesProcessed + mBlockSize - 1; 
    // The end sample index of windowed audio data, but adjusted by lookahead
    juce::int64 endDetectionSample = endProcessSample - MagicNumbers::minLookaheadSize;
    juce::int64 startDetectionSample = endDetectionSample - MagicNumbers::minDetectionSize;
    return std::make_tuple(startDetectionSample, endDetectionSample);
}


//-------------------------------------------
std::tuple<juce::int64, juce::int64> PluginProcessor::getFirstPeakRange(float detectedPeriod)
{
    // where we will be at the end of this block
    juce::int64 endProcessSample = mSamplesProcessed + mBlockSize - 1; 
    juce::int64 endDetectionSample = endProcessSample - MagicNumbers::minLookaheadSize;
    // The end sample index of windowed audio data, but adjusted by lookahead
    juce::int64 endFirstPeakRange = endDetectionSample; // 
    juce::int64 startFirstPeakRange = endFirstPeakRange - (juce::int64)detectedPeriod;
    return std::make_tuple(startFirstPeakRange, endFirstPeakRange);
}

//-------------------------------------------
std::tuple<juce::int64, juce::int64> PluginProcessor::getPrecisePeakRange(juce::int64 predictedAnalysisMarker, float detectionPeriod)
{
    // where we will be at the end of this block
    juce::int64 radius = (juce::int64)(detectionPeriod * 0.25f);
    juce::int64 predictedRangeStart = predictedAnalysisMarker - radius;
    juce::int64 predictedRangeEnd = predictedAnalysisMarker + radius;
    return std::make_tuple(predictedRangeStart, predictedRangeEnd);
}

//-------------------------------------------
std::tuple<juce::int64, juce::int64, juce::int64> PluginProcessor::getAnalysisReadRange(juce::int64 analysisMark, float detectedPeriod)
{
    juce::int64 analysisRangeStart = analysisMark - (juce::int64) detectedPeriod;
    juce::int64 analysisRangeEnd = analysisMark + (juce::int64) detectedPeriod;
    return std::make_tuple(analysisRangeStart, analysisMark, analysisRangeEnd);
}

//-------------------------------------------
std::tuple<juce::int64, juce::int64, juce::int64> PluginProcessor::getAnalysisWriteRange(std::tuple<juce::int64, juce::int64, juce::int64> analysisReadRange)
{
    juce::int64 writeStart = std::get<0>(analysisReadRange) + MagicNumbers::minLookaheadSize;
    juce::int64 writeMark = std::get<1>(analysisReadRange) + MagicNumbers::minLookaheadSize;
    juce::int64 writeEnd = std::get<2>(analysisReadRange) + MagicNumbers::minLookaheadSize;
    return std::make_tuple(writeStart, writeMark, writeEnd);
}

//-------------------------------------------
std::tuple<juce::int64, juce::int64> PluginProcessor::getDryBlockRange()
{
    juce::int64 blockRangeStart = mSamplesProcessed - MagicNumbers::minLookaheadSize;
    juce::int64 blockRangeEnd = blockRangeStart + mBlockSize;
    return std::make_tuple(blockRangeStart, blockRangeEnd);
}


