#include "PluginProcessor.h"
#include "PluginEditor.h"

//==================================
PluginEditor::PluginEditor (PluginProcessor& p)
: AudioProcessorEditor (&p)
, mProcessor (p)
{
	

	mPitchDisplay = std::make_unique<juce::Label>();
	addAndMakeVisible(mPitchDisplay.get());
	
	// BOUNDS
	this->setSize(400, 400);

	mPitchDisplay->setBounds(100, 300, 50, 30);


	// TIMER
	startTimerHz(30);

}

//=================================
PluginEditor::~PluginEditor()
{
	stopTimer();

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