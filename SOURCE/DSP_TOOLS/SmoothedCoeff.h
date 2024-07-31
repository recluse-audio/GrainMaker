//
#pragma once
#include <cmath>

class SmoothedCoeff {
public:
    enum { kInit = 1 };

    SmoothedCoeff()
    {
    }
    ~SmoothedCoeff() {}

    inline void set(float new_value, bool init = false)
    {
        value[0] = new_value;
        if (init)
            value[1] = value[0];
    }

    inline float get(void)
    {
        // run smoothing
        value[1] = tc * value[1] + (1. - tc) * value[0];
        return float(value[1]);
    }

    void init(float fs, float ms = 10.)
    {
        // calculate time constant
        tc = (ms == 0) ? 0. : exp(-1. / (ms * .001 * fs));
    }

private:
    double tc;
    double value[2] = { 0, 0 };
};
