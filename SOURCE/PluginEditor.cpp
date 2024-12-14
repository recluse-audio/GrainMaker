#include "PluginProcessor.h"
#include "PluginEditor.h"

//==================================
PluginEditor::PluginEditor (PluginProcessor& p)
: AudioProcessorEditor (&p)
, mProcessor (p)
{
	// INIT COMPONENTS
	mPitchDisplay = std::make_unique<juce::Label>();
	addAndMakeVisible(mPitchDisplay.get());
	
	mPitchShiftSlider = std::make_unique<juce::Slider>(juce::Slider::SliderStyle::Rotary, juce::Slider::TextEntryBoxPosition::TextBoxBelow);
	addAndMakeVisible(mPitchShiftSlider.get());

	// INIT ATTACHMENTS
	mPitchShiftAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        mProcessor.getAPVTS(), "shift ratio", *mPitchShiftSlider.get());

	// BOUNDS
	this->setSize(400, 400);
	mPitchDisplay->setBounds(200, 300, 50, 30);
	mPitchShiftSlider->setBounds(100, 200, 50, 50);

	// TIMER
	startTimerHz(30);

}

//=================================
PluginEditor::~PluginEditor()
{
	//TIMERS
	stopTimer();

	// ATTACHMENTS
	mPitchShiftAttachment.reset();

	// COMPONENTS
	mPitchShiftSlider.reset();
	mPitchDisplay.reset();
}

//==============================================================================
void PluginEditor::paint (juce::Graphics& g)
{
	g.fillAll(juce::Colours::red);
}


//=================================
void PluginEditor::resized()
{

}


//==================================
void PluginEditor::timerCallback()
{
	float currentPitch = mProcessor.getLastDetectedPitch();
	auto pitchString = juce::String(currentPitch);
	mPitchDisplay->setText(pitchString, juce::NotificationType::dontSendNotification);
}