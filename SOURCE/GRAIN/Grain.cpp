/**
 * Grain.cpp
 * Created by Ryan Devens
 */

#include "Grain.h"

Grain::Grain()
{
	mBuffer.setSize(2, 1024);
	mBuffer.clear();
}

Grain::~Grain()
{
}

//=======================================
void Grain::prepare(int maxGrainSize, int numChannels)
{
	mBuffer.setSize(numChannels, maxGrainSize);
	mBuffer.clear();
}

//=======================================
void Grain::reset()
{
	isActive = false;
	mAnalysisRange = { -1, -1, -1 };
	mSynthRange = { -1, -1, -1 };
	mBuffer.clear();
}
