#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PITCH/PitchDetector.h"
#include "PITCH/PitchMarkedCircularBuffer.h"
#include "PITCH/GrainCorrector.h"
#include "Granulator.h"

//==============================================================================
PluginProcessor::PluginProcessor()
: AudioProcessor (_getBusesProperties())
, apvts(*this, nullptr, "Parameters", _createParameterLayout())
{
    mPitchDetector = std::make_unique<PitchDetector>();
    mPMCBuffer = std::make_unique<PitchMarkedCircularBuffer>();
    mGrainCorrector = std::make_unique<GrainCorrector>(*mPMCBuffer.get());
    mCircularBuffer = std::make_unique<CircularBuffer>();
    mGranulator = std::make_unique<Granulator>();

    mGranulator->setEmissionPeriodInSamples(1024);
    mGranulator->setGrainLengthInSamples(1024);
    mGranulator->setGrainShape(Window::Shape::kHanning);

	mShiftRatio = 1.f;
	
    _initParameterListeners();

}

//=================================
//
PluginProcessor::~PluginProcessor()
{
    mPitchDetector.reset();
    mPMCBuffer.reset();
    mGrainCorrector.reset();
    mCircularBuffer.reset();
    mGranulator.reset();
}

//==============================================================================
const juce::String PluginProcessor::getName() const
{
    return "ArtieTune";
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
	int lookahead = samplesPerBlock;

	mLookaheadBuffer.clear();
	mLookaheadBuffer.setSize(this->getNumOutputChannels(), samplesPerBlock + lookahead);

    // mPitchDetector->prepareToPlay(sampleRate, samplesPerBlock + lookahead);
    mPitchDetector->prepareToPlay(sampleRate, samplesPerBlock);
	
    // mPMCBuffer->prepare(sampleRate, samplesPerBlock);
    // mPMCBuffer->setSize(this->getNumOutputChannels(), (int)sampleRate); // by default 1 second

    // mGrainCorrector->prepare(sampleRate, samplesPerBlock);
    // mGrainCorrector->setOutputDelay(samplesPerBlock);

    mCircularBuffer->setSize(this->getNumOutputChannels(), (int)sampleRate * 2); // by default 1 second
    mCircularBuffer->setDelay(lookahead);

    mGranulator->prepare(sampleRate, samplesPerBlock);

    // mGranulator->setEmissionPeriodInSamples(44100);
    // mGranulator->setGrainLengthInSamples(44100);
    // mGranulator->setGrainShape(Window::Shape::kHanning);

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
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    bool writeSuccess = mCircularBuffer->pushBuffer(buffer);
	if(!writeSuccess)
		return;

	buffer.clear();
	mLookaheadBuffer.clear();
    mCircularBuffer->popBufferWithLookahead(mLookaheadBuffer, buffer);

	// int sampleOffset = mLookaheadBuffer.getNumSamples() - buffer.getNumSamples();

	// for(int sampleIndex = 0; sampleIndex < buffer.getNumSamples(); sampleIndex++)
	// {
	// 	for(int ch = 0; ch < buffer.getNumChannels(); ch++)
	// 	{
	// 		float lookaheadSample = mLookaheadBuffer.getSample(ch, sampleIndex);
	// 		buffer.setSample(ch, sampleIndex, lookaheadSample);
	// 	}
	// }

    float detected_period = mPitchDetector->process(mLookaheadBuffer);
	// mGranulator->setEmissionPeriodInSamples(detected_period * mShiftRatio);
	mGranulator->processShifting(mLookaheadBuffer, buffer, detected_period, mShiftRatio);

    // mPMCBuffer->pushBufferAndPeriod(buffer, detected_period);
    // mGrainCorrector->process(buffer);
    // buffer.applyGain(0.f);
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
    float pitch = this->getSampleRate() / mPitchDetector->getCurrentPeriod();
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
		mShiftRatio = newValue;
        //mGrainCorrector->setTransposeRatio(newValue);
    }
    else if(parameterID == "emission rate")
    {
        //mGranulator->setEmissionRateInHz(newValue);
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