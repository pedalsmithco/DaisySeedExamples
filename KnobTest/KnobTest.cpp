#include "daisy_seed.h"
#include "daisysp.h"
using namespace daisy;
using namespace daisysp;

DaisySeed hw;
Oscillator osc;
float knob_smooth = 0.f;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    // Read the pot (0.0 - 1.0) and smooth out jitter
    float knob = hw.adc.GetFloat(0);
    fonepole(knob_smooth, knob, 0.01f);

    // Map knob to 4 octaves: MIDI note 36 (C2) up to 84 (C6)
    osc.SetFreq(mtof(36.f + knob_smooth * 48.f));

    for(size_t i = 0; i < size; i++)
    {
        float s = osc.Process();
        out[0][i] = s;
        out[1][i] = s;
    }
}

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(4);

    // Set up one ADC channel on A0 (pin 22)
    AdcChannelConfig adc;
    adc.InitSingle(seed::A0);
    hw.adc.Init(&adc, 1);
    hw.adc.Start();

    osc.Init(hw.AudioSampleRate());
    osc.SetWaveform(Oscillator::WAVE_SIN);
    osc.SetAmp(0.1f);
    hw.StartAudio(AudioCallback);

    while(1)
    {
        hw.SetLed(true);  System::Delay(250);
        hw.SetLed(false); System::Delay(250);
    }
}