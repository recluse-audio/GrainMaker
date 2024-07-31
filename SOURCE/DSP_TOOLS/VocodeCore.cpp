/*
  ==============================================================================

    Vocoder.cpp
    Created: 22 Jun 2018 3:49:06pm
    Author:  Andrew

  ==============================================================================
*/

#include "VocodeCore.h"

/**
    Constructor
 */
template <typename dspfloat_t>
VocodeCore<dspfloat_t>::VocodeCore()
{
    // reset band arrays
    for (int i=0; i < kMaxBands; i++) {
        bg_[i] = 1.;
        bf_[i] = 1.;
        vf_[i] = 0.;
        sf_[i] = 0;
        io_[i] = i; // 1:1 channel routing
    }
    
    this->setSampleRate(fs_);
}

//
//  intitialize vocoder for new sample rate
//
template <typename dspfloat_t>
void VocodeCore<dspfloat_t>::setSampleRate(float fs)
{
    fs_ = fs;

    // set decimation clock
    int scale;
    if (fs_ > 100000)
        scale = 4;
    else
    if (fs_ > 50000)
        scale = 2;
    else
        scale = 1;

    slowCount_ = kSlowClockPeriod * scale;
    slowClock_ = slowCount_ - 1;

    // set coefficient de-zippering time constant
    smoothAlpha_ = expf(-2.f * 3.1415926f * 100.f / (fs_ / slowCount_)); // 100 Hz = 10 msec time constant
    morphAlpha_ = 0.;

    // design filter banks
    
    designVintage(model_);

    // set envelope followers

    dspfloat_t tc = TimeMath::onePoleCoeff<dspfloat_t>(envAttTimeMs_, fs_, TimeMath::kDecayAnalog);
    for (int i=0; i < bands_; i++) {
        voiceEnvDet_[i].setSampleRate(fs_);
        voiceEnvDet_[i].setAttack(tc);
    }

    setEnvReleaseTimes(); // updates all release times
    
    inputFollow_.setSampleRate(fs_);
    inputFollow_.setAttackMs(inpAttTimeMs_);
    inputFollow_.setReleaseMs(inpRelTimeMs_);
    inputEnv_ = 0.;

    voiceFollow_.setSampleRate(fs_);
    voiceFollow_.setAttackMs(envAttTimeMs_);
    voiceFollow_.setReleaseMs(envRelTimeMs_);
    voiceEnv_ = 0.;

    synthFollow_.setSampleRate(fs_);
    synthFollow_.setAttackMs(extAttTimeMs_);
    synthFollow_.setReleaseMs(extRelTimeMs_);
    synthEnv_ = 0.;
    
    // set mic proximity effect lowcut HPF

    locutHpf_.design(fs_, kLowcutFilterFc, 0.7071f, 0., BiquadFilter<dspfloat_t>::kHighpass);

    // set tube eq (approximate flat response)

    tubeToneEq_.setSampleRate(fs_);
    tubeToneEq_.setControl(TubeTone<dspfloat_t>::kLow, kTubeEqLowDefault);
    tubeToneEq_.setControl(TubeTone<dspfloat_t>::kMid, kTubeEqMidDefault);
    tubeToneEq_.setControl(TubeTone<dspfloat_t>::kTop, emphasis_);

    // set high consonant filter

    hiconHpf_.design(fs_, hiconHpfFc_, 0.7071f, MultiStageIIR<dspfloat_t>::kTypeHighpass, kHiConHpfStages);

    // set post processors
    tubeProc_[0].setSampleRate(fs_);
    tubeProc_[1].setSampleRate(fs_);
    
    chorusFx_.setSampleRate(fs_);
    uvDetector_.setSampleRate(fs_);
    
    // set level meter decay times
    for (int i=0; i < 4; i++) {
        levelMeter[i].setDecayMs(200., fs_);
    }
}

//
// design EVOC20 style vocoder
//
template <typename dspfloat_t>
void VocodeCore<dspfloat_t>::designCustom(void)
{
    if (!canModifyFilters(model_))
        return;

    // generate voice filter bank
    LogMath::logspace(fc_, fmin_, fmax_, bands_);
    for (int i=0; i < bands_; i++) {
        voiceFilter_[i].design(fs_, fc_[i], Qv_, MultiStageIIR<dspfloat_t>::kTypeBandpass, stages_);
    }
    
    // apply stretch and slide
    float fmin = powf(10.f, log10f(fmin_) + slide_) / stretch_;
    float fmax = powf(10.f, log10f(fmax_) + slide_) * stretch_;
    
    // limit frequency bounds
    fmin = fmin < 50 ? 50 : fmin;
    fmax = fmax > 15000 ? 15000 : fmax;
    
    // generate synth filter bank
    LogMath::logspace(fx_, fmin, fmax, bands_);
    for (int i=0; i < bands_; i++) {
        synthFilter_[i].design(fs_, fx_[i], Qs_, MultiStageIIR<dspfloat_t>::kTypeBandpass, stages_);
    }
    
    setLPF(useLpf_, Qv_, Qs_);
    setHPF(useHpf_, Qv_, Qs_);
    
    setBandShift(shift_);
}

//
// design vintage vocoder from raw data
//
template <typename dspfloat_t>
void VocodeCore<dspfloat_t>::designVintage(int model)
{
    // uppdate state
    bands_ = vintage_[model].numBands;
    stages_ = vintage_[model].numPoles / 2;
    fmin_ = vintage_[model].bandFreq[0];
    fmax_ = vintage_[model].bandFreq[bands_ - 1];
    useLpf_ = vintage_[model].lpfOn;
    useHpf_ = vintage_[model].hpfOn;
#if 0
    stretch_ = 1.0;
    slide_ = 0.0;
#endif
    float Q = vintage_[model].filterQ;
    Qv_ = Qs_ = Q;

    // AK: below are hacks to implement specific variations of vintage vocoder filters
    // --> this needs to be refactored into something more elegant

    switch (model) {
        default:
            for (int i=0; i < bands_; i++) {
                fc_[i] = fx_[i] = vintage_[model].bandFreq[i];
                voiceFilter_[i].design(fs_, fc_[i], Q, MultiStageIIR<dspfloat_t>::kTypeBandpass, stages_);
                synthFilter_[i].design(fs_, fc_[i], Q, MultiStageIIR<dspfloat_t>::kTypeBandpass, stages_);
            }
            setLPF(vintage_[model].lpfOn, Q, Q);
            setHPF(vintage_[model].hpfOn, Q, Q);
            break;
        case kVocSennhVSM201:
            for (int i=0; i < bands_; i++) {
                Q = vsm201Q_[i];
                fc_[i] = fx_[i] = vintage_[model].bandFreq[i];
                voiceFilter_[i].design(fs_, fc_[i], Q, MultiStageIIR<dspfloat_t>::kTypeBandpass, stages_);
                synthFilter_[i].design(fs_, fc_[i], Q, MultiStageIIR<dspfloat_t>::kTypeBandpass, stages_);
            }
            break;
        case kVocBarkhausen:
            for (int i=0; i < bands_; i++) {
                Q = barkScaleQ_[i];
                fc_[i] = fx_[i] = vintage_[model].bandFreq[i];
                voiceFilter_[i].design(fs_, fc_[i], Q, MultiStageIIR<dspfloat_t>::kTypeBandpass, stages_);
                synthFilter_[i].design(fs_, fc_[i], Q, MultiStageIIR<dspfloat_t>::kTypeBandpass, stages_);
            }
            setLPF(true, .7f, .7f); // hack to set low-band LPF in lieu of unstable BPF
            break;
        case kVocMAMVF11:
            for (int i=0; i < bands_; i++) {
                Q = mamVF11Q_[i];
                fc_[i] = fx_[i] = vintage_[model].bandFreq[i];
                voiceFilter_[i].design(fs_, fc_[i], Q, MultiStageIIR<dspfloat_t>::kTypeBandpass, stages_);
                synthFilter_[i].design(fs_, fc_[i], Q, MultiStageIIR<dspfloat_t>::kTypeBandpass, stages_);
            }
            break;
        case kVocRolandSVC350:
            for (int i=0; i < bands_; i++) {
                fc_[i] = fx_[i] = vintage_[model].bandFreq[i];
                voiceFilter_[i].design(fs_, fc_[i], Q, MultiStageIIR<dspfloat_t>::kTypeBandpass, stages_);
                fc_[i] = vintage_[model].bandFreq[i + bands_];
                synthFilter_[i].design(fs_, fc_[i], Q, MultiStageIIR<dspfloat_t>::kTypeBandpass, stages_);
            }
            break;
    }

    setBandShift(shift_); // update band shift
}

//
//
//
template <typename dspfloat_t>
int VocodeCore<dspfloat_t>::getVocoderModelParams(int& poles, bool& lpfOn, bool& hpfOn, float& fMin, float& fMax, float& fBw)
{
    // current values
    poles = stages_ * 2;
    lpfOn = useLpf_;
    hpfOn = useHpf_;
    fMin = fmin_;
    fMax = fmax_;
    fBw = Qv_ = Qs_;
    return bands_;
}

//
// run vocoder (sample processing)
//
template <typename dspfloat_t>
void VocodeCore<dspfloat_t>::run(dspfloat_t ( &xi )[2], dspfloat_t ( &xo )[2], dspfloat_t noise, dspfloat_t voice, bool synthIsExternal, int audition)
{
    // get voice input sample (CH1)
    dspfloat_t xv = xi[0];
    
    // apply voice lowcut (HPF)
    if (lowcut_) {
        xv = locutHpf_.run(xv);
    }
    
    // run unvoiced detector
    unvoiced_ = uvDetector_.run(xv, freeze_);
    
    // run input envelope follower
    inputEnv_ = (float)inputFollow_.run(xv, Follower<dspfloat_t>::kSmoothBranching);
    
    // run voice compressor: if compThreshLvl_ = 0 then compGain_ = 1 (no compression)
    voiceCompGain_ = (inputEnv_ > voiceCompThresh_) ? 1.f / inputEnv_ : 1.f / voiceCompThresh_;
    xv *= voiceCompGain_;

    // run pre-emphasis filter
    if (emphasis_ > 0) {
        xv = tubeToneEq_.runBuffered(xv);
    }

    // run voice envelope follower (before analysis fitler bank)
    voiceEnv_ = (float)voiceFollow_.run(xv, rmsdet_, freeze_);
    
    levelMeter[kVoiceLevel].run(xv);
    
    // get synth input sample (CH2)
    dspfloat_t xs = xi[1];
    
    // run synth envelope follower
    synthEnv_ = (float)synthFollow_.run(xs, Follower<dspfloat_t>::kSmoothBranching);
    
    // run sidechain compressor if external carrier
    if (synthIsExternal)
    {
#if 0
        synthEnv_ = fmax(synthEnv_, kSynthEnvGateThr); // apply downward gate
#endif
        synthCompGain_ = (synthEnv_ > synthCompThresh_) ? 1.f / synthEnv_ : 1.f / synthCompThresh_;
        xs *= (synthCompGain_ * synthExtraGain_);
    }

    // apply envelope to noise source
    dspfloat_t xn = noise * synthEnv_;
    
    // mix unvoiced noise w/ synth
    if (unvoiced_) {
        xs = unvoicedMix_[kActive] * xn + (1.f - unvoicedMix_[kActive]) * xs;
    }
    
    levelMeter[kSynthLevel].run(xs);
    
    // -vocoder nucleus-

    dspfloat_t y[2] = {0., 0.};
    
    if (audition > 0)
    {
        dspfloat_t xin = (audition == 2) ? noise : xi[1] * 0.5f; // -6dB cut on synth audition
        
        // run analysis filter bank + followers, all bands
        for (int i=0; i < bands_; i++) {
            vf_[i] = voiceEnvDet_[i].run(voiceFilter_[i].run(xin), rmsdet_, freeze_);
        }
        
        // run synthesis filter bank, all bands
        for (int i=0; i < bands_; i++) {
            sf_[i] = synthFilter_[i].run(xin);
        }
        
        // sum even synthesis filter bank outputs
        for (int i=0; i < bands_; i+=2) {
            y[0] += sf_[i] * bf_[i];
        }

        // sum odd synthesis filter bank outputs
        for (int i=1; i < bands_; i+=2) {
            y[1] += sf_[i] * bf_[i];
        }
    }
    else
    {
        // run analysis filter bank + followers, all bands
        for (int i=0; i < bands_; i++) {
            vf_[i] = voiceEnvDet_[i].run(voiceFilter_[i].run(xv), rmsdet_, freeze_);
        }

        // run synthesis filter bank, all bands
        for (int i=0; i < bands_; i++) {
            sf_[i] = synthFilter_[i].run(xs);
        }
        
        // apply analysis spectral envelope onto synthesis filter bank,
        // sum even bands to left output
        for (int i=0; i < bands_; i+=2) {
            if (io_[i] != -1) {
                y[0] += sf_[io_[i]] * vf_[i] * bf_[i] * boost_;
            }
        }
        
        // apply analysis spectral envelope onto synthesis filter bank,
        // sum odd bands to right output
        for (int i=1; i < bands_; i+=2) {
            if (io_[i] != -1) {
                y[1] += sf_[io_[i]] * vf_[i] * bf_[i] * boost_;
            }
        }
    }
    
    // apply stereo spread
    dspfloat_t yl = spreadAdj_[kActive] * y[0] + (1 - spreadAdj_[kActive]) * y[1];
    dspfloat_t yr = spreadAdj_[kActive] * y[1] + (1 - spreadAdj_[kActive]) * y[0];
    
    y[0] = yl;
    y[1] = yr;
    
    levelMeter[kVocodeLevel].run(y, LevelMeter<dspfloat_t>::kPeak); // kRMS

    // apply voice amplitude modulation (voicing)
    vlsGain_ = ((inputEnv_ - 1.f) * vlsAmount_[kActive] + 1.f) * vlsCoeff_[kActive];

    y[0] *= vlsGain_;
    y[1] *= vlsGain_;

    // apply output gain scaling
    y[0] *= makeupGain_[kActive];
    y[1] *= makeupGain_[kActive];

    // run warmth processing
    if (warmth_ > 0) {
        y[0] = tubeProc_[0].run(y[0]);
        y[1] = tubeProc_[1].run(y[1]);
    }
    
    // run voice high consonant filter (HPF) on dry vocal
    dspfloat_t xhp = hiconHpf_.run(voice); // * inputEnv_ * hiconLevel_;
    
    if (chorusOn_ && !vocToChorus_) {
        // run ensemble processing
        chorusFx_.run(y, y);
    }
    
    // mix dry vocal to output (w/ 3dB cut for stereo split)
    y[0] += dryVocMix_[kActive] * xhp * 0.7071f;
    y[1] += dryVocMix_[kActive] * xhp * 0.7071f;

    if (chorusOn_ && vocToChorus_) {
        // run ensemble processing
        chorusFx_.run(y, y);
    }
    
    // run level meters
    levelMeter[kOutputLevel].run(y);
    
    // clamp outputs
    y[0] = fmin(y[0],  k0dBFS);
    y[0] = fmax(y[0], -k0dBFS);
    y[1] = fmin(y[1],  k0dBFS);
    y[1] = fmax(y[1], -k0dBFS);

    // return the output samples
    xo[0] = y[0];
    xo[1] = y[1];
    
    // run slow clock processing (coefficient smoothing)
    if (++slowClock_ == slowCount_)    {
        slowClock_ = 0;

        // band gain smoothing
        float morphBeta = 1.0f - morphAlpha_;
        for (int i=0; i < bands_; i++) {
            if (morphEnable_)
                bf_[i] = morphAlpha_ * bf_[i] + morphBeta * bm_[i];
            else
                bf_[i] = morphAlpha_ * bf_[i] + morphBeta * bg_[i];
        }
        
        // other coefficient smoothing
        float smoothBeta = 1.0f - smoothAlpha_;
        vlsCoeff_[kActive] = smoothAlpha_ * vlsCoeff_[kActive] + smoothBeta * vlsCoeff_[kTarget];
        vlsAmount_[kActive] = smoothAlpha_ * vlsAmount_[kActive] + smoothBeta * vlsAmount_[kTarget];
        spreadAdj_[kActive] = smoothAlpha_ * spreadAdj_[kActive] + smoothBeta * spreadAdj_[kTarget];
        dryVocMix_[kActive] = smoothAlpha_ * dryVocMix_[kActive] + smoothBeta * dryVocMix_[kTarget];
        makeupGain_[kActive] = smoothAlpha_ * makeupGain_[kActive] + smoothBeta * makeupGain_[kTarget];
        unvoicedMix_[kActive] = smoothAlpha_ * unvoicedMix_[kActive] + smoothBeta * unvoicedMix_[kTarget];
    }
}


//
// API / user interface
//
template <typename dspfloat_t>
void VocodeCore<dspfloat_t>::setControl(int paramId, float param, float param2, int& guiUpdateFlag)
{
    dspfloat_t temp;
    int guiUpdateType = kNoGuiUpdate; // no update

    switch (paramId)
    {
        // Filterbank design
        case kVocFbEmulation:
            if (param < kVocNumModels) {
                model_ = (int)param;
                designVintage(model_);
            }
            guiUpdateType = kUpdateFilterGraphAndMultiSlider;
            break;
        case kVocFbNumBands:
            if (param <= kMaxBands && canModifyFilters(model_)) {
                bands_ = (int)param;
                designCustom();
            };
            guiUpdateType = kUpdateFilterGraphAndMultiSlider;
            break;
        case kVocFbNumPoles:
            stages_ = (int)param + 1; // param index starts at 0
            designCustom();
            guiUpdateType = kUpdateFilterGraph;
            break;
        case kVocFbFilterQ:
            Qs_ = param;
            Qv_ = param;
            designCustom();
            guiUpdateType = kUpdateFilterGraph;
            break;
        case kVocFbFreqMin:
            fmin_ = param;
            designCustom();
            guiUpdateType = kUpdateFilterGraph;
            break;
        case kVocFbFreqMax:
            fmax_ = param;
            designCustom();
            guiUpdateType = kUpdateFilterGraph;
            break;
        case kVocFbSetLPF:
            useLpf_ = (bool)param;
            setLPF(useLpf_, Qv_, Qs_, true);
            guiUpdateType = kUpdateFilterGraph;
            break;
        case kVocFbSetHPF:
            useHpf_ = (bool)param;
            setHPF(useHpf_, Qv_, Qs_, true);
            guiUpdateType = kUpdateFilterGraph;
            break;
        case kVocFbStretch:
            stretch_ = param;
            designCustom();
            guiUpdateType = kUpdateFilterGraph;
            break;
        case kVocFbSlide:
            slide_ = param;
            designCustom();
            guiUpdateType = kUpdateFilterGraph;
            break;

        // Envelope followers
        case kVocEnvBoost:
            boost_ = (float)LogMath::dbToLin(param);
            break;
        case kVocEnvFlavor:
            rmsdet_ = (bool)param;
            break;
        case kVocEnvFreeze:
            freeze_ = (bool)param;
            break;
        case kVocEnvAttack:
            envAttTimeMs_ = param;
            temp = TimeMath::onePoleCoeff<dspfloat_t>(param, fs_, TimeMath::kDecayAnalog);
            for (int i=0; i < bands_; i++) {
                voiceEnvDet_[i].setAttack(temp);
            }
            voiceFollow_.setAttack(temp);
            break;
        case kVocEnvRelease:
            envRelTimeMs_ = param;
            setEnvReleaseTimes();
            voiceFollow_.setReleaseMs(envRelTimeMs_);
            break;
        case kVocEnvRelRatio:
            envRelTimeDt_ = param;
            setEnvReleaseTimes();
            break;
        case kVocEnvBandGain:
            bg_[(int)param2] = (float)LogMath::dbToLin(param); // dBToGain(-60. * (1. - param));
            break;
        case kVocEnvBandShift:
            shift_ = (int)param;
            setBandShift(shift_);
            break;
        case kVocEnvBandGlide:
            morphAlpha_ = (param == 0) ? 0.f : exp(-1.f / (param * .001f * fs_ / slowCount_));
            break;

        // Voice compressor
        case kVocCompThresh:
            voiceCompThresh_ = (float)LogMath::dbToLin(-param); // invert! slider range is 0 to 36 (dB)
            break;
        case kVocCompAttack:
            inpAttTimeMs_ = param;
            inputFollow_.setAttackMs(inpAttTimeMs_);
            break;
        case kVocCompRelease:
            inpRelTimeMs_ = param;
            inputFollow_.setReleaseMs(inpRelTimeMs_);
            break;

        // Pre-filtering
        case kVocLowCutEnable:
            lowcut_ = (bool)param;
            break;
        case kVocTubeEqTreble:
            emphasis_ = param * .01f;
            tubeToneEq_.setControl(TubeTone<dspfloat_t>::kTop, emphasis_);
            break;
            
        // High consonant filter
        case kVocHiConHpfFc:
            hiconHpfFc_ = param;
            hiconHpf_.design(fs_, hiconHpfFc_, 1., MultiStageIIR<dspfloat_t>::kTypeHighpass, kHiConHpfStages);
            break;
        case kVocHiConLevel:
            hiconLevel_ = param * .01f;
            break;

        // Global
        case kVocSynthComp:
            synthCompThresh_ = (float)LogMath::dbToLin(-param); // invert! slider range is 0 to 36 (dB)
            break;
        case kVocSynthGain:
            synthExtraGain_ = (float)LogMath::dbToLin(param); // slider range is -12 to 12 (dB)
            break;
        case kVocDriveGain:
            warmth_ = param;
            tubeProc_[0].setControl(TubeProc<dspfloat_t>::kDriveGain, param);
            tubeProc_[1].setControl(TubeProc<dspfloat_t>::kDriveGain, param);
            break;
        case kVocChorusType:
            chorusOn_ = (param > 0);
            if (chorusOn_)
                chorusFx_.loadPreset((int)param);
            break;
        case kVocMakeupGain:
            makeupGain_[kTarget] = (float)LogMath::dbToLin(param);
            break;
        case kVocUvMixBalance:
            unvoicedMix_[kTarget] = param * .01f;
            break;
        case kVocUvSensitivity:
            uvDetector_.setParam(UnvoicedDetector<dspfloat_t>::kAlpha, param);
            break;
        case kVocAmplitudeMod:
            vlsAmount_[kTarget] = param * .01f;
            vlsCoeff_[kTarget] = (float)LogMath::dbToLin(kVlsGainRefDb * (1.0f - vlsAmount_[kTarget]));
            break;
        case kVocStereoSpread:
            spreadAdj_[kTarget] = (1.f - (param / 100.f)) * 0.5f; // 0 to 100 -> -1 to +1
            break;
        case kVocVoiceToOutMix:
            dryVocMix_[kTarget] = param / 100.f; //  * 0.707; // 0 to 1, -3dB cut
            break;
        case kVocVoiceToChorus:
            vocToChorus_ = (bool)param;
            break;
        default:
            // TODO - error
            break;
    }
    guiUpdateFlag = guiUpdateType;
}

//
//
//
template <typename dspfloat_t>
int VocodeCore<dspfloat_t>::getGraphData(float h[][kGraphPoints], float f[], float fc[], float fx[], int bins)
{
    // get filter frequency responses
    
    for (int k=0; k < bins; k++)
    {
        for (int n=0; n < bands_; n++) {
            h[n][k] = (float)voiceFilter_[n].getMagnitudeResponse(f[k], k);
        }
    }
    
    // get filter center frequencies

    for (int n=0; n < bands_; n++) {
        fc[n] = LogMath::log2lin(0, bins, f[0], f[bins-1], fc_[n]);
        fx[n] = LogMath::log2lin(0, bins, f[0], f[bins-1], fx_[n]);
    }
    
    return bands_; // number of bands
}


//
//
//
template <typename dspfloat_t>
void VocodeCore<dspfloat_t>::getLevels(float ( &pLevel )[kNumLevelParams])
{
    // voice + synth input envelope detectors
    pLevel[kVoiceEnvelope] = inputEnv_;
    pLevel[kSynthEnvelope] = synthEnv_; // vlsGain_
    
    // voice compressor gain reduction
    pLevel[kVoiceCompGain] = voiceCompGain_;
    pLevel[kSynthCompGain] = synthCompGain_;

    // input level meters
    pLevel[kVoiceLevel] = float(levelMeter[kVoiceLevel].get());
    pLevel[kSynthLevel] = float(levelMeter[kSynthLevel].get());
    
    // output level meters
    pLevel[kVocodeLevel] = float(levelMeter[kVocodeLevel].get());
    pLevel[kOutputLevel] = float(levelMeter[kOutputLevel].get());
}

//
//
//
template <typename dspfloat_t>
void VocodeCore<dspfloat_t>::getVocoderModelDefaultParams(int& poles, bool& lpfOn, bool& hpfOn, float& fMin, float& fMax, float& fBw, int& bands)
{
    bands = vintage_[model_].numBands;
    poles = vintage_[model_].numPoles;
    fMin = vintage_[model_].bandFreq[0];
    fMax = vintage_[model_].bandFreq[bands - 1];
    lpfOn = vintage_[model_].lpfOn;
    hpfOn = vintage_[model_].hpfOn;
    fBw = vintage_[model_].filterQ;
}

//
//
//
template <typename dspfloat_t>
void VocodeCore<dspfloat_t>::getVocoderModelDefaultParams(VintageModel& model)
{
    int bands = vintage_[model_].numBands;
    
    model.numBands = vintage_[model_].numBands;
    model.numPoles = vintage_[model_].numPoles;
    model.bandFreq[0] = vintage_[model_].bandFreq[0];
    model.bandFreq[bands - 1] = vintage_[model_].bandFreq[bands - 1];
    model.lpfOn = vintage_[model_].lpfOn;
    model.hpfOn = vintage_[model_].hpfOn;
    model.filterQ = vintage_[model_].filterQ;
}

//
//
//
template <typename dspfloat_t>
bool VocodeCore<dspfloat_t>::canModifyFilters(void) const
{
    return vintage_[model_].canModify;
}

//
//
//
template <typename dspfloat_t>
bool VocodeCore<dspfloat_t>::canModifyFilters(int model) const
{
    return vintage_[model].canModify;
}



/* PRIVATE METHODS */

//
//
//
template <typename dspfloat_t>
void VocodeCore<dspfloat_t>::setLPF(bool useLpf, float Qv, float Qs, bool updatedFromGui)
{
    if (updatedFromGui && !canModifyFilters(model_))
        return;
    
    int n = 0;
    if (useLpf) {
        voiceFilter_[n].design(fs_, fc_[n], Qv, MultiStageIIR<dspfloat_t>::kTypeLowpass, stages_);
        synthFilter_[n].design(fs_, fx_[n], Qs, MultiStageIIR<dspfloat_t>::kTypeLowpass, stages_);
    } else {
        voiceFilter_[n].design(fs_, fc_[n], Qv, MultiStageIIR<dspfloat_t>::kTypeBandpass, stages_);
        synthFilter_[n].design(fs_, fx_[n], Qs, MultiStageIIR<dspfloat_t>::kTypeBandpass, stages_);
    }
}

//
//
//
template <typename dspfloat_t>
void VocodeCore<dspfloat_t>::setHPF(bool useHpf, float Qv, float Qs, bool updatedFromGui)
{
    if (updatedFromGui && !canModifyFilters(model_))
        return;
    
    int n = bands_-1;
    if (useHpf) {
        voiceFilter_[n].design(fs_, fc_[n], Qv, MultiStageIIR<dspfloat_t>::kTypeHighpass, stages_);
        synthFilter_[n].design(fs_, fx_[n], Qs, MultiStageIIR<dspfloat_t>::kTypeHighpass, stages_);
    } else {
        voiceFilter_[n].design(fs_, fc_[n], Qv, MultiStageIIR<dspfloat_t>::kTypeBandpass, stages_);
        synthFilter_[n].design(fs_, fx_[n], Qs, MultiStageIIR<dspfloat_t>::kTypeBandpass, stages_);
    }
}

//
//
//
template <typename dspfloat_t>
void VocodeCore<dspfloat_t>::setEnvReleaseTimes(void)
{
    if (envRelTimeDt_ == 0) {
        dspfloat_t decay = TimeMath::onePoleCoeff<dspfloat_t>(envRelTimeMs_, fs_, TimeMath::kDecayAnalog);
        for (int i = 0; i < bands_; i++)
            voiceEnvDet_[i].setRelease(decay);
        return;
    }

    float rt_[kMaxBands]; // follower release times
    LogMath::logspace(rt_, envRelTimeMs_, envRelTimeMs_ * fabs(envRelTimeDt_), bands_);
    
    if (envRelTimeDt_ > 0) {
        for (int i = 0; i < bands_; i++)
            voiceEnvDet_[i].setRelease(TimeMath::onePoleCoeff<dspfloat_t>(rt_[i], fs_, TimeMath::kDecayAnalog));
    } else {
        for (int i = 0; i < bands_; i++)
            voiceEnvDet_[bands_ - i - 1].setRelease(TimeMath::onePoleCoeff<dspfloat_t>(rt_[i], fs_, TimeMath::kDecayAnalog));
    }
}

//
//
//
template <typename dspfloat_t>
void VocodeCore<dspfloat_t>::setBandShift(int shift)
{
    int i, j, k;
    
    switch (shift) {
        default:
            // shift left / right
            for (i = 0; i < bands_; i++) {
                j = i + shift;
                if (j < 0 || j > bands_ - 1)
                    io_[i] = -1; // disable
                else
                    io_[i] = j;
            }
            break;
        case 6:
            // reverse channel routing
            for (i = 0; i < bands_; i++) {
                io_[i] = bands_ - i - 1;
            }
            break;
        case 7:
            // criss-cross channel routing
            j = bands_ / 2;
            for (i = 0; i < j; i++) {
                io_[i] = i + j;
                io_[i + j] = i;
            }
            break;
        case 8:
            // split criss-cross channel routing
            j = bands_ / 4;
            for (i = 0; i < j; i++) {
                io_[i] = i + j;
                io_[i + j] = i;
            }
            k = bands_ / 2;
            for (i = k; i < j + k; i++) {
                io_[i] = i + j;
                io_[i + j] = i;
            }
            break;
    }
#ifdef VOCODER_DEBUG
    for (i = 0; i < bands_; i++) {
        std::cout << "io[" << i << "] = " << io_[i] << "\n";
    }
#endif
}
    

/*
 enum VocoderModel {
 kVocAntaresVoc1 = 0,	// 24 band (default)
 kVocBarkhausen,	    // 24 band, not importable
 kVocRolandVP330,	 	// 10 band
 kVocRolandSVC350, 		// 10 band
 kVocEMS2000,        	// 16 band, not importable
 kVocEMS5000,			// 22 band
 kVocSyntovox221,		// 20 band
 kVocSennhVSM201,    	// 20 band, not importable
 kVocBode7702,			// 16 band
 kVocMoog907,			// 10 band
 kVocMoog914,			// 14 band
 kVocKorgVC10,			// 20 band
 kVocElecHarm0300, 		// 14 band
 kVocDoepferA128,		// 15 band
 kVocDoepferA129,    	// 15 band, not importable
 kVocHoeroldVoIS,		// 18 band
 kVocGRPV22,			// 22 band
 kVocMAMVF11,        	// 11 band, not importable
 kVocMFOS12,			// 12 band
 kVodDudley,			// 10 band
 kVocNumModels       	// 18
 };
 */

//
// Vintage vocoder design data
//
template <typename dspfloat_t>
const typename VocodeCore<dspfloat_t>::VintageModel VocodeCore<dspfloat_t>::vintage_[] = {
	// 0: Antares VOC-1 (was Emagic EVOC20)
	{ {100, 122, 149, 182, 223, 272, 332, 406, 496, 606, 741, 905, 1105, 1350,
		1650, 2015, 2462, 3008, 3675, 4489, 5484, 6700, 8188, 10000}, 5.0, 24, 8, 1, 1, 1,
		{24, 5.0, 100, 10000, 1, 1, 8}},
	// 1: Bark scale vocoder
	{ {100, 150, 250, 350, 450, 570, 700, 840, 1000, 1170, 1370, 1600, 1850, 2150,
		2500, 2900, 3400, 4000, 4800, 5800, 7000, 8500, 10500, 13500}, 7.0, 24, 8, 0, 0, 0},
    // 2: Roland VP-330
    { {196, 281, 407, 610, 915, 1338, 1960, 2814, 4065, 6098}, 4.8f, 10, 4, 0, 0, 1,
        {10, 4.8f, 196, 6098, 0, 0, 4}},
    // 3: Roland SVC-350 (accounts for formant shift)
    { {161, 234, 333, 499, 732, 1098, 1614, 2335, 3326, 4989,
        195, 279, 404, 605, 908, 1329, 1946, 2794, 4036, 6053}, 4.8f, 10, 4, 0, 0, 1,
        {10, 4.8f, 161, 4989, 0, 0, 4}},
	// 4: EMS Vocoder 2000 / 3000
	{ {125, 185, 270, 350, 430, 530, 630, 780, 950, 1150, 1380, 1680, 2070,
		2780, 3800, 6400}, 5.0, 16, 6, 1, 1, 0},
	// 5: EMS Vocoder 5000 (22 band, LPF/HPF)
	{ {169, 205, 249, 303, 367, 444, 539, 653, 791, 958, 1161, 1406,
		1703, 2064, 2500, 3030, 3670, 4447, 5388, 6519, 7888, 9544}, 5.0, 22, 6, 1, 1, 1,
		{22, 5, 169, 9544, 1, 1, 6}},
	// 6: Synton Syntovox-221 (LPF/HPF)
	{ {190, 230, 280, 340, 410, 480, 590, 710, 880, 1100, 1300, 1600, 1900,
		2300, 2800, 3400, 4100, 4900, 5900, 7100}, 5.0, 20, 8, 1, 1, 1,
		{20, 5, 190, 7100, 1, 1, 8}},
	// 7: Sennheiser VSM-201 (estimated) (LPF/HPF)
	{ {140, 240, 340, 455, 560, 690, 820, 980, 1160, 1370, 1600, 1865, 2175,
		2500, 2900, 3425, 4025, 4825, 5850, 7300}, 5.0, 20, 6, 0, 0, 0},
	// 8: Moog / Bode 7702
	{ {141, 179, 228, 288, 358, 455, 575, 717, 910, 1151,
		1433, 1821, 2302, 2852, 3620, 4561}, 5.5, 16, 4, 1, 0, 1,
		{16, 5.5, 141, 4561, 1, 0, 4}},
	// 9: Moog 907
	{ {175, 250, 350, 500, 700, 1000, 1400, 2000, 2800, 4000}, 4.0, 10, 4, 1, 1, 1,
		{10, 4.5, 175, 4000, 1, 1, 4}},
	// 10: Moog 914
	{ {88, 125, 175, 250, 350, 500, 700, 1000, 1400, 2000,
		2800, 4000, 5600, 8000}, 4.0, 14, 4, 1, 1, 1,
		{14, 5, 88, 8000, 1, 1, 4}},
	// 11: Korg VC-10 (calculated from spreadsheet)
	{ {219, 264, 312, 381, 468, 572, 687, 858, 1030, 1256, 1514, 1839, 2191,
		2640, 3121, 3814, 4681, 5721, 6865, 8581}, 5.0, 20, 4, 0, 0, 1,
		{20, 5, 219, 8581, 0, 0, 4}},
    // 12: Electro Harmonix EH-0300
    { {246.1f, 298.2f, 361.2f, 437.7f, 530.2f, 642.4f, 778.3f, 942.9f, 1142.3f,
        1384.0, 1676.7f, 2031.4f, 2461.1f, 2981.7f}, 5.0, 14, 4, 0, 0, 1,
        {14, 5, 246, 2982, 0, 0, 4}},
	// 13: Doepfer A-128
	{ {50, 75, 110, 150, 220, 350, 500, 750, 1100, 1600, 2200, 3600,
		5200, 7500, 11000}, 5.0, 15, 4, 0, 0, 1,
		{15, 5, 50, 11000, 0, 0, 4}},
    // 14: Doepfer A-129 (LPF/HPF)
    { {100, 120, 160, 230, 330, 500, 750, 1100, 1300, 1600, 2300, 3300,
        5000, 7500, 10000}, 5.0, 15, 4, 1, 1, 0},
	// 15: Hoerold VoIS
	{ {119, 151, 192, 243, 309, 393, 500, 636, 808, 1027, 1305, 1659,
		2108, 2680, 3406, 4330, 5503, 6995}, 5.0, 18, 8, 0, 0, 1,
		{18, 5, 119, 6995, 0, 0, 8}},
	// 16: Groppioni (GRP) V22 (22 band, LPF/HPF)
	{ {185, 220, 262, 311, 370, 440, 523, 622, 740, 880, 1047, 1245,
		1480, 1760, 2093, 2489, 2960, 3520, 4186, 4978, 5920, 7040}, 5.0, 22, 8, 1, 1, 1,
		{22, 5, 185, 7040, 1, 1, 8}},
    // 17: MAM VF-11
    { {100, 225, 330, 470, 700, 1030, 1500, 2280, 3300, 4700, 9000}, 5.0, 11, 4, 0, 0, 0},
    // 18: MFOS 12
    { {100, 154, 208, 285, 395, 542, 720, 1013, 1495, 2000, 2546, 3330}, 4.0, 12, 2, 1, 1, 1,
        {12, 4, 100, 3330, 1, 1, 2}},
	// 19: Homer Dudley
	{ {250, 329, 434, 572, 754, 994, 1310, 1727, 2276, 3000}, 3.5, 10, 4, 0, 0, 1,
		{10, 3.5, 250, 3000, 0, 0, 4}},
};


// variable Qs for Bark scale filterbank
template <typename dspfloat_t>
const float VocodeCore<dspfloat_t>::barkScaleQ_[] = {0.51f, 1.5f, 2.5f, 3.5f, 4.09f, 4.75f, 5.0f, 5.6f, 6.25f, 6.16f, 6.52f,
	6.67f, 6.61f, 6.72f, 6.32f, 6.44f, 6.18f, 5.71f, 5.33f, 5.27f, 5.38f, 4.72f, 4.2f, 3.86f};

// variable Qs for Sennheiser VSM201 filterbank
template <typename dspfloat_t>
const float VocodeCore<dspfloat_t>::vsm201Q_[] = {1.65f, 2.92f, 4.14f, 4.55f, 5.6f, 5.75f, 6.3f, 7.0f, 6.55f, 7.6f,
    8.f, 8.1f, 8.7f, 8.33f, 9.66f, 9.78f, 11.5f, 13.78f, 8.35f, 9.125f};

// variable Qs for MAM VF11 filterbank
template <typename dspfloat_t>
const float VocodeCore<dspfloat_t>::mamVF11Q_[] = {0.7f, 5, 5, 5, 5, 5, 5, 5, 5, 5, 1.5756f};

template class VocodeCore<float>;
template class VocodeCore<double>;
