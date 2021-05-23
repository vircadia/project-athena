//
//  AudioEffectOptions.cpp
//  libraries/audio/src
//
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "AudioEffectOptions.h"

#include <ScriptContext.h>
#include <ScriptEngine.h>
#include <ScriptValue.h>

static const QString BANDWIDTH_HANDLE = "bandwidth";
static const QString PRE_DELAY_HANDLE = "preDelay";
static const QString LATE_DELAY_HANDLE = "lateDelay";
static const QString REVERB_TIME_HANDLE = "reverbTime";
static const QString EARLY_DIFFUSION_HANDLE = "earlyDiffusion";
static const QString LATE_DIFFUSION_HANDLE = "lateDiffusion";
static const QString ROOM_SIZE_HANDLE = "roomSize";
static const QString DENSITY_HANDLE = "density";
static const QString BASS_MULT_HANDLE = "bassMult";
static const QString BASS_FREQ_HANDLE = "bassFreq";
static const QString HIGH_GAIN_HANDLE = "highGain";
static const QString HIGH_FREQ_HANDLE = "highFreq";
static const QString MOD_RATE_HANDLE = "modRate";
static const QString MOD_DEPTH_HANDLE = "modDepth";
static const QString EARLY_GAIN_HANDLE = "earlyGain";
static const QString LATE_GAIN_HANDLE = "lateGain";
static const QString EARLY_MIX_LEFT_HANDLE = "earlyMixLeft";
static const QString EARLY_MIX_RIGHT_HANDLE = "earlyMixRight";
static const QString LATE_MIX_LEFT_HANDLE = "lateMixLeft";
static const QString LATE_MIX_RIGHT_HANDLE = "lateMixRight";
static const QString WET_DRY_MIX_HANDLE = "wetDryMix";

static const float BANDWIDTH_DEFAULT = 10000.0f;
static const float PRE_DELAY_DEFAULT = 20.0f;
static const float LATE_DELAY_DEFAULT = 0.0f;
static const float REVERB_TIME_DEFAULT = 2.0f;
static const float EARLY_DIFFUSION_DEFAULT = 100.0f;
static const float LATE_DIFFUSION_DEFAULT = 100.0f;
static const float ROOM_SIZE_DEFAULT = 50.0f;
static const float DENSITY_DEFAULT = 100.0f;
static const float BASS_MULT_DEFAULT = 1.5f;
static const float BASS_FREQ_DEFAULT = 250.0f;
static const float HIGH_GAIN_DEFAULT = -6.0f;
static const float HIGH_FREQ_DEFAULT = 3000.0f;
static const float MOD_RATE_DEFAULT = 2.3f;
static const float MOD_DEPTH_DEFAULT = 50.0f;
static const float EARLY_GAIN_DEFAULT = 0.0f;
static const float LATE_GAIN_DEFAULT = 0.0f;
static const float EARLY_MIX_LEFT_DEFAULT = 20.0f;
static const float EARLY_MIX_RIGHT_DEFAULT = 20.0f;
static const float LATE_MIX_LEFT_DEFAULT = 90.0f;
static const float LATE_MIX_RIGHT_DEFAULT = 90.0f;
static const float WET_DRY_MIX_DEFAULT = 50.0f;

static void setOption(ScriptValuePointer arguments, const QString name, float defaultValue, float& variable) {
    variable = arguments->property(name)->isNumber() ? (float)arguments->property(name)->toNumber() : defaultValue;
}

/*@jsdoc
 * Reverberation options that can be used to initialize an {@link AudioEffectOptions} object when created.
 * @typedef {object} AudioEffectOptions.ReverbOptions
 * @property {number} bandwidth=10000 - The corner frequency (Hz) of the low-pass filter at reverb input.
 * @property {number} preDelay=20 - The delay (milliseconds) between dry signal and the onset of early reflections.
 * @property {number} lateDelay=0 - The delay (milliseconds) between early reflections and the onset of reverb tail.
 * @property {number} reverbTime=2 - The time (seconds) for the reverb tail to decay by 60dB, also known as RT60.
 * @property {number} earlyDiffusion=100 - Adjusts the buildup of echo density in the early reflections, normally 100%.
 * @property {number} lateDiffusion=100 - Adjusts the buildup of echo density in the reverb tail, normally 100%.
 * @property {number} roomSize=50 - The apparent room size, from small (0%) to large (100%).
 * @property {number} density=100 - Adjusts the echo density in the reverb tail, normally 100%.
 * @property {number} bassMult=1.5 - Adjusts the bass-frequency reverb time, as multiple of reverbTime.
 * @property {number} bassFreq=250 - The crossover frequency (Hz) for the onset of bassMult.
 * @property {number} highGain=-6 - Reduces the high-frequency reverb time, as attenuation (dB).
 * @property {number} highFreq=3000 - The crossover frequency (Hz) for the onset of highGain.
 * @property {number} modRate=2.3 - The rate of modulation (Hz) of the LFO-modulated delay lines.
 * @property {number} modDepth=50 - The depth of modulation (percent) of the LFO-modulated delay lines.
 * @property {number} earlyGain=0 - Adjusts the relative level (dB) of the early reflections.
 * @property {number} lateGain=0 - Adjusts the relative level (dB) of the reverb tail.
 * @property {number} earlyMixLeft=20 - The apparent distance of the source (percent) in the early reflections.
 * @property {number} earlyMixRight=20 - The apparent distance of the source (percent) in the early reflections.
 * @property {number} lateMixLeft=90 - The apparent distance of the source (percent) in the reverb tail.
 * @property {number} lateMixRight=90 - The apparent distance of the source (percent) in the reverb tail.
 * @property {number} wetDryMix=50 - Adjusts the wet/dry ratio, from completely dry (0%) to completely wet (100%).
 */
AudioEffectOptions::AudioEffectOptions(ScriptValuePointer arguments) {
    setOption(arguments, BANDWIDTH_HANDLE, BANDWIDTH_DEFAULT, _bandwidth);
    setOption(arguments, PRE_DELAY_HANDLE, PRE_DELAY_DEFAULT, _preDelay);
    setOption(arguments, LATE_DELAY_HANDLE, LATE_DELAY_DEFAULT, _lateDelay);
    setOption(arguments, REVERB_TIME_HANDLE, REVERB_TIME_DEFAULT, _reverbTime);
    setOption(arguments, EARLY_DIFFUSION_HANDLE, EARLY_DIFFUSION_DEFAULT, _earlyDiffusion);
    setOption(arguments, LATE_DIFFUSION_HANDLE, LATE_DIFFUSION_DEFAULT, _lateDiffusion);
    setOption(arguments, ROOM_SIZE_HANDLE, ROOM_SIZE_DEFAULT, _roomSize);
    setOption(arguments, DENSITY_HANDLE, DENSITY_DEFAULT, _density);
    setOption(arguments, BASS_MULT_HANDLE, BASS_MULT_DEFAULT, _bassMult);
    setOption(arguments, BASS_FREQ_HANDLE, BASS_FREQ_DEFAULT, _bassFreq);
    setOption(arguments, HIGH_GAIN_HANDLE, HIGH_GAIN_DEFAULT, _highGain);
    setOption(arguments, HIGH_FREQ_HANDLE, HIGH_FREQ_DEFAULT, _highFreq);
    setOption(arguments, MOD_RATE_HANDLE, MOD_RATE_DEFAULT, _modRate);
    setOption(arguments, MOD_DEPTH_HANDLE, MOD_DEPTH_DEFAULT, _modDepth);
    setOption(arguments, EARLY_GAIN_HANDLE, EARLY_GAIN_DEFAULT, _earlyGain);
    setOption(arguments, LATE_GAIN_HANDLE, LATE_GAIN_DEFAULT, _lateGain);
    setOption(arguments, EARLY_MIX_LEFT_HANDLE, EARLY_MIX_LEFT_DEFAULT, _earlyMixLeft);
    setOption(arguments, EARLY_MIX_RIGHT_HANDLE, EARLY_MIX_RIGHT_DEFAULT, _earlyMixRight);
    setOption(arguments, LATE_MIX_LEFT_HANDLE, LATE_MIX_LEFT_DEFAULT, _lateMixLeft);
    setOption(arguments, LATE_MIX_RIGHT_HANDLE, LATE_MIX_RIGHT_DEFAULT, _lateMixRight);
    setOption(arguments, WET_DRY_MIX_HANDLE, WET_DRY_MIX_DEFAULT, _wetDryMix);
}

AudioEffectOptions::AudioEffectOptions(const AudioEffectOptions &other) : QObject() {
    *this = other;
}

AudioEffectOptions& AudioEffectOptions::operator=(const AudioEffectOptions &other) {
    _bandwidth = other._bandwidth;
    _preDelay = other._preDelay;
    _lateDelay = other._lateDelay;
    _reverbTime = other._reverbTime;
    _earlyDiffusion = other._earlyDiffusion;
    _lateDiffusion = other._lateDiffusion;
    _roomSize = other._roomSize;
    _density = other._density;
    _bassMult = other._bassMult;
    _bassFreq = other._bassFreq;
    _highGain = other._highGain;
    _highFreq = other._highFreq;
    _modRate = other._modRate;
    _modDepth = other._modDepth;
    _earlyGain = other._earlyGain;
    _lateGain = other._lateGain;
    _earlyMixLeft = other._earlyMixLeft;
    _earlyMixRight = other._earlyMixRight;
    _lateMixLeft = other._lateMixLeft;
    _lateMixRight = other._lateMixRight;
    _wetDryMix = other._wetDryMix;
     
    return *this;
}

ScriptValuePointer AudioEffectOptions::constructor(ScriptContext* context, ScriptEngine* engine) {
    return engine->newQObject(new AudioEffectOptions(context->argument(0)));
}
