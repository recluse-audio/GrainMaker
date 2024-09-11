#include "Window.h"
#include "DSP_TOOLS/BufferFiller.h"


//===================
//
Window::Window()
{
    mBuffer.clear();
    mBuffer.setSize(1, (int)kDefaultSize); // sizing these according to 
    
}

//====================
//
Window::~Window()
{
    mBuffer.clear();
}

//====================
//
void Window::setSampleRate(double sampleRate)
{
    mBuffer.setSize(1, (int) sampleRate); // 1 second worth of samples by default
}


//==================
//
void Window::setShape(Window::Shape newShape)
{
    if(mCurrentShape != newShape)
    {
        mCurrentShape = newShape;
        _update();
    }
}

//==================
//
void Window::_update()
{
    if(mCurrentShape == Window::kHanning)
        BufferFiller::generateHanning(mBuffer);
    else if(mCurrentShape == Window::kNone)
        BufferFiller::fillWithAllOnes(mBuffer);
}