//
//  Smoother.h
//  Sandbox
//
//  Created by Andrew on 10/13/17.
//  Copyright © 2017 Antares. All rights reserved.
//

#pragma once
#include <cmath>

#if DEBUG_SMOOTHER
#include <iostream>
#endif

class OnePoleLPF {
public:
    OnePoleLPF()
    {
        // init to bypass
        a0_ = 1.0;
        b1_ = 0.;
        z1_ = 0;
    };

    OnePoleLPF(float Fc, float Fs) { design(Fc, Fs); };

    void design(float Fc, float Fs)
    {
        b1_ = exp(-2.f * kPi * Fc / Fs);
        a0_ = (1. - b1_);
        z1_ = 0.;
#if DEBUG_SMOOTHER
        std::cout << "[Smoother] a0 = " << a0_ << "\t\tb1 = " << b1_ << "\n";
#endif
    }

    inline float run(float x_in)
    {
        return z1_ = x_in * a0_ + z1_ * b1_;
    }

private:
    static constexpr float kPi = 3.14159265426f;
    float a0_, b1_, z1_;
};
