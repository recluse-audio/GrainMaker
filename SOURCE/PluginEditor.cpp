#include "PluginProcessor.h"
#include "PluginEditor.h"

//==================================
PluginEditor::PluginEditor (PluginProcessor& p)
: AudioProcessorEditor (&p)
, mProcessor (p)
{
	setSize(400, 400);

}

//=================================
PluginEditor::~PluginEditor()
{

}

//==============================================================================
void PluginEditor::paint (juce::Graphics& g)
{
	g.fillAll(juce::Colours::whitesmoke);
}


//=================================
void PluginEditor::resized()
{

}
