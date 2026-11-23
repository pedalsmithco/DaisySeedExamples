// Distortion.cpp  -  one-knob overdrive -> distortion for the Daisy Seed3
//
// Wiring (same as the earlier tests):
//   Guitar in   -> pin 16 (Audio In L),  sleeve -> pin 20 (AGND)
//   Amp out     -> pin 18 (Audio Out L), pin 19 (Out R) optional, sleeve -> pin 20
//   Gain pot    -> outer legs to pin 21 (3V3 analog) and pin 20 (AGND), wiper to pin 22 (A0)
//
// Knob low  : smooth, round overdrive (soft clipping, full low end)
// Knob high : crunchy distortion (much more gain, harder clipping, tighter lows)
// Volume is auto-levelled so the output stays roughly even across the sweep.

#include "daisy_seed.h"
#include "daisysp.h"
#include <math.h>
using namespace daisy;
using namespace daisysp;

// ---------- tweak these to taste ----------
const float kInputTrim   = 3.0f;   // guitar is weaker than line level; raise if it feels weak
const float kMinGain     = 2.0f;   // gain at knob = 0 (~6 dB, light breakup)
const float kMaxGain     = 120.0f; // gain at knob = 1 (~42 dB, full crunch)
const float kOutputLevel = 1.0f;   // overall output volume (0.5 - 2.0)
// ------------------------------------------

// Simple one-pole filter. Named DistFilter so it can't clash with DaisySP's OnePole.
struct DistFilter
{
    float a = 0.f, z = 0.f, sr = 48000.f;
    void  Init(float s) { sr = s; z = 0.f; }
    void  SetFreq(float f) { a = 1.f - expf(-6.2831853f * f / sr); }
    float Low(float x) { z += a * (x - z); return z; }
    float High(float x) { return x - Low(x); }
};

DaisySeed  hw;
DistFilter lowCut;   // trims the lows going into the clipper (tighter as gain rises)
DistFilter midPush;  // isolates mids/highs for a gentle push into the clipper
DistFilter fizzCut;  // rolls off the harsh top end after clipping
DcBlock    dc1, dc2;

float knob = 0.3f;

// Auto-level: makeup gain measured at 11 knob positions so the output stays
// about the same volume from clean-ish overdrive to full crunch.
const float kLevelTable[11] = {2.60f, 1.72f, 0.90f, 0.58f, 0.46f, 0.40f, 0.375f, 0.36f, 0.353f, 0.349f, 0.348f};
inline float AutoLevel(float k)
{
    float pos = k * 10.f;
    int   i   = (int)pos;
    if(i >= 10) return kLevelTable[10];
    float f = pos - (float)i;
    return kLevelTable[i] + f * (kLevelTable[i + 1] - kLevelTable[i]);
}

// Stage 1: soft, slightly asymmetric clip (like mismatched diodes). Adds warmth.
inline float WarmClip(float x)
{
    const float bias = 0.12f;
    return tanhf(x + bias) - tanhf(bias);
}

// Stage 2: variable-hardness clip. k = 2 is round and soft, k = 8 is close to a hard clip.
inline float ShapeClip(float x, float k)
{
    float ax = fabsf(x);
    return x / powf(1.f + powf(ax, k), 1.f / k);
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    // Smooth the knob so it never zips or crackles
    fonepole(knob, hw.adc.GetFloat(0), 0.004f);
    float g = knob * knob;  // squared taper: more control in the overdrive range

    // Everything that changes with the knob
    float drive    = kMinGain + g * (kMaxGain - kMinGain);
    float stage    = sqrtf(drive);              // split the gain across two stages
    float hardness = 2.f + 6.f * knob;          // soft -> hard clipping
    lowCut.SetFreq(70.f + 110.f * knob);        // 70 Hz -> 180 Hz: tighter lows when crunchy
    fizzCut.SetFreq(7500.f - 2500.f * knob);    // 7.5 kHz -> 5 kHz: tame fizz at high gain
    float level    = AutoLevel(knob);           // keeps the volume even across the sweep

    for(size_t i = 0; i < size; i++)
    {
        float x = in[0][i] * kInputTrim;

        x = lowCut.High(x);
        x = x + 0.5f * midPush.High(x);   // gentle mid push

        x = WarmClip(x * stage);          // stage 1
        x = dc1.Process(x);
        x = ShapeClip(x * stage, hardness); // stage 2
        x = dc2.Process(x);

        x = fizzCut.Low(x);

        float y = tanhf(x * level) * kOutputLevel;  // soft limiter: never harsh
        out[0][i] = y;
        out[1][i] = y;
    }
}

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(4);
    float sr = hw.AudioSampleRate();

    lowCut.Init(sr);  lowCut.SetFreq(70.f);
    midPush.Init(sr); midPush.SetFreq(700.f);
    fizzCut.Init(sr); fizzCut.SetFreq(7500.f);
    dc1.Init(sr);
    dc2.Init(sr);

    AdcChannelConfig adc;
    adc.InitSingle(seed::A0);   // pin 22
    hw.adc.Init(&adc, 1);
    hw.adc.Start();

    hw.StartAudio(AudioCallback);

    while(1)
    {
        hw.SetLed(true);  System::Delay(1000);
        hw.SetLed(false); System::Delay(1000);
    }
}
