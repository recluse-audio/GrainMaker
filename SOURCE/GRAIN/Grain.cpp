/**
 * Grain.cpp
 * Created by Ryan Devens
 */

#include "Grain.h"

Grain::Grain()
{
}

Grain::~Grain()
{
}

//=======================================
void Grain::prepare(int windowSize)
{
	mWindow.setSizeShapePeriod(windowSize, Window::Shape::kHanning, windowSize);
	mWindow.setLooping(true);
}

//=======================================
void Grain::setWindowPeriod(int grainSize)
{
	mWindow.setPeriod(grainSize);
	mWindow.resetReadPos();
}

//=======================================
void Grain::setWindowPhaseOffset(int sampleOffset)
{
	double phaseIncrement = static_cast<double>(mWindow.getSize()) / static_cast<double>(mWindow.getPeriod());
	mWindow.setReadPos(sampleOffset * phaseIncrement);
}

//=======================================
float Grain::getNextWindowSample()
{
	return mWindow.getNextSample();
}

//=======================================
void Grain::reset()
{
	isActive = false;
	mAnalysisRange = { -1, -1 };
	mSynthRange = { -1, -1 };
	mWindow.resetReadPos();
}
