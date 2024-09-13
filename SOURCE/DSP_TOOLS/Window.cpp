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
void Window::setSize(double newSize)
{
    mBuffer.setSize(1, (int) newSize); // 1 second worth of samples by default
}

//=====================
//
const int Window::getSize()
{
    return mBuffer.getNumSamples();
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
    if(mCurrentShape == Window::Shape::kHanning)
        BufferFiller::generateHanning(mBuffer);
    else if(mCurrentShape == Window::Shape::kNone)
        BufferFiller::fillWithAllOnes(mBuffer);
}

