#include "PluginProcessor.h"
#include "PluginEditor.h"

//==================================
PluginEditor::PluginEditor (PluginProcessor& p)
: AudioProcessorEditor (&p)
, mProcessor (p)
{
	// INIT LABELS
	mPitchDisplayLabel = std::make_unique<juce::Label>();
	mPitchDisplayLabel->setText("Detected Pitch", juce::NotificationType::dontSendNotification);
	addAndMakeVisible(mPitchDisplayLabel.get());

	mShiftRatioLabel = std::make_unique<juce::Label>();
	mShiftRatioLabel->setText("Shift Ratio", juce::NotificationType::dontSendNotification);
	addAndMakeVisible(mShiftRatioLabel.get());

	mEmissionRateLabel = std::make_unique<juce::Label>();
	mEmissionRateLabel->setText("Emission Rate", juce::NotificationType::dontSendNotification);
	addAndMakeVisible(mEmissionRateLabel.get());
	
	// INIT DISPLAYS
	mPitchDisplay = std::make_unique<juce::Label>();
	addAndMakeVisible(mPitchDisplay.get());
	
	// ADD SLIDERS
	mPitchShiftSlider = std::make_unique<juce::Slider>(juce::Slider::SliderStyle::Rotary, juce::Slider::TextEntryBoxPosition::TextBoxBelow);
	addAndMakeVisible(mPitchShiftSlider.get());
	mEmissionRateSlider = std::make_unique<juce::Slider>(juce::Slider::SliderStyle::Rotary, juce::Slider::TextEntryBoxPosition::TextBoxBelow);
	addAndMakeVisible(mEmissionRateSlider.get());

	// INIT ATTACHMENTS
	mPitchShiftAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        mProcessor.getAPVTS(), "shift ratio", *mPitchShiftSlider.get());
	mEmissionRateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
		mProcessor.getAPVTS(), "emission rate", *mEmissionRateSlider.get());

	// BOUNDS
	this->setSize(400, 400);
	mPitchDisplayLabel->setBounds(100, 50, 100, 30);
	mPitchDisplay->setBounds(100, 100, 100, 30);

	mShiftRatioLabel->setBounds(200, 50, 100, 30);
	mPitchShiftSlider->setBounds(200, 100, 100, 100);

	mEmissionRateLabel->setBounds(300, 50, 100, 30);
	mEmissionRateSlider->setBounds(300, 100, 100, 100);


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
	mEmissionRateAttachment.reset();

	// COMPONENTS
	mShiftRatioLabel.reset();
	mPitchDisplayLabel.reset();
	mEmissionRateLabel.reset();

	mPitchShiftSlider.reset();
	mEmissionRateSlider.reset();
	mPitchDisplay.reset();
}

//==============================================================================
void PluginEditor::paint (juce::Graphics& g)
{
	g.fillAll(juce::Colours::darkred);
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