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
    int getWindowShape() { return mWindow.mCurrentShape; } // hack testing I know

private:
    Window& mWindow;
};


TEST_CASE("Window can handle sample rate change")
{
    Window w;
    WindowTester wt(w);

    double sampleRate = 48000;
    w.setSampleRate(sampleRate);
    CHECK( wt.getWindowSize() == sampleRate );

    sampleRate = 88200;
    w.setSampleRate(sampleRate);
    CHECK( wt.getWindowSize() == sampleRate );

    sampleRate = 96000;
    w.setSampleRate(sampleRate);
    CHECK( wt.getWindowSize() == sampleRate );

    sampleRate = 176400;
    w.setSampleRate(sampleRate);
    CHECK( wt.getWindowSize() == sampleRate );

    sampleRate = 192000;
    w.setSampleRate(sampleRate);
    CHECK( wt.getWindowSize() == sampleRate );
}


//=====================
//
TEST_CASE("Can set and store shape of window ")
{
    Window w;
    WindowTester wt(w);

    w.setShape(Window::Shape::kHanning);


}