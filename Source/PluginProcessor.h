#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "DSP/EqEngine.h"
#include "DSP/LinearPhase.h"
#include "DSP/Analyzer.h"
#include "DSP/Lanes.h"

class AfoQ4Processor : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    AfoQ4Processor();
    ~AfoQ4Processor() override { cancelPendingUpdate(); }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Afo-Q 4"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    afoq::Analyzer preAnalyzer, postAnalyzer;
    float dynamicAmount (int band) const { return engine.dynamicAmount (band); }   // live 0..1 for the UI
    double getSampleRateHz() const { return sr; }
    std::atomic<float> meterL { 0.f }, meterR { 0.f };      // block peaks (linear); UI resets with exchange(0)

private:
    void handleAsyncUpdate() override;
    void readParams();

    struct Ptrs { std::atomic<float> *on, *type, *freq, *gain, *q, *slope, *ch, *dyn, *dir, *thr, *range, *atk, *rel, *wide; };
    std::array<Ptrs, afoq::kMaxBands> ptrs {};
    std::atomic<float> *phasePtr = nullptr, *resPtr = nullptr, *outPtr = nullptr, *soloPtr = nullptr;

    std::array<afoq::BandParams, afoq::kMaxBands> params {};
    afoq::LinearPhase::Snapshot snapshot;
    afoq::EqEngine engine;
    afoq::LinearPhase linear;
    juce::SmoothedValue<float> outGain, modeFade, soloMix;
    std::vector<float> scratch, dryL, dryR;
    double sr = 48000.0;
    int activeMode = 0, lastRes = -1, lastSolo = -1;
    bool fadingOut = false;
    double soloZ1[2] {}, soloZ2[2] {};
    afoq::Coeffs soloC; afoq::ChannelMode soloCh = afoq::ChannelMode::Stereo;
    std::atomic<int> pendingLatency { 0 };
};
