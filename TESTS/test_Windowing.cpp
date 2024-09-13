#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "TEST_UTILS/TestUtils.h"
#include "TEST_UTILS/BufferGenerator.h"

#include "DSP_TOOLS/Window.h"

/**
 * @brief Helps check stuff under the hood, especially really fundamental stuff like buffer resizing with sample rate
 * 
 * 
 */

class WindowTester
{
public:
    WindowTester(Window& windower) : mWindow(windower){}
    ~WindowTester(){}

    int getWindowSize() { return mWindow.mBuffer.getNumSamples(); }
    // int getWindowShape() { return mWindow.mCurrentShape; } // hack testing I know

private:
    Window& mWindow;
};


//=========================
//
TEST_CASE("Window can handle sample rate change")
{
    Window w;
    WindowTester wt(w);

    double sampleRate = 48000;
    w.setSize(sampleRate);
    CHECK( wt.getWindowSize() == sampleRate );

    sampleRate = 88200;
    w.setSize(sampleRate);
    CHECK( wt.getWindowSize() == sampleRate );

    sampleRate = 96000;
    w.setSize(sampleRate);
    CHECK( wt.getWindowSize() == sampleRate );

    sampleRate = 176400;
    w.setSize(sampleRate);
    CHECK( wt.getWindowSize() == sampleRate );

    sampleRate = 192000;
    w.setSize(sampleRate);
    CHECK( wt.getWindowSize() == sampleRate );
}


//=====================
//
TEST_CASE("Can get sample from window")
{

}

//=====================
//
TEST_CASE("Can set and store shape of window ")
{
    // Window w;
    // WindowTester wt(w);

    // w.setSize(1024);
    // w.setShape(Window::Shape::kHanning);

    // // checking values are normalized
    // for(int sampleIndex = 0; sampleIndex <= numSamples; sampleIndex++)
    // {
    //     float sample = buffer.getSample(0, sampleIndex);
    //     CHECK(sample >= 0.f);
    //     CHECK(sample <= 1.f);
    // }

    // // checking symmetry
    // for(int sampleIndex = 0; sampleIndex <= numSamples / 2.f; sampleIndex++)
    // {
    //     float sample1 = buffer.getSample(0, sampleIndex);
    //     float sample2 = buffer.getSample(0, numSamples - sampleIndex);

    //     CHECK(sample1 == Catch::Approx(sample2).epsilon(0.0001f));
    // }

}