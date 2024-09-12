/**
 * @file Window.h
 * @author Ryan Devens
 * @brief 
 * @version 0.1
 * @date 2024-09-09
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#pragma once

#include "Util/Juce_Header.h"


static const double kDefaultSize = 44100;

/**
 * @brief This class provides a look-up table for different window functions, First just hanning.
 * 
 * The goal is that we look up a value (0 - 1.0) and use that as an amplitude coefficient to create 
 * a way to interpret a single stored buffer of amplitude for various window lengths.
 * 
 * Inspired by ADSR and LFO wavetables I have made in the past.  I hope this saves on processing.  
 * Re-generating the window with each incoming new length (i.e. pitch period) seems expensive.
 * Storing these windows ahead of time feels like it will cause large memory footprint bloat and reduce instance count
 * 
 * These theories are not tested!  But this method is most logical first step for me.
 * 
 * This window lookup table 
 * 
 */

class Window
{
public:
    enum class Shape
    {
        kNone,
        kHanning,
    };

    Window();
    ~Window();

    void setSampleRate(double sampleRate);

    void setShape(Window::Shape newShape);

private:
    friend class WindowTester;
    juce::AudioBuffer<float> mBuffer; // using juce buffer for some helpful functions, but could be a simple array

    Window::Shape mCurrentShape;

    void _update();
};