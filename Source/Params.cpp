#include "Params.h"

namespace afoq
{
juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout L;
    constexpr int v = 1;

    L.add (std::make_unique<AudioParameterChoice> (ParameterID { "phase", v },    "Processing Mode", StringArray { "Zero Latency", "Natural Phase", "Linear Phase" }, 0));
    L.add (std::make_unique<AudioParameterChoice> (ParameterID { "lin_res", v },  "Linear Phase Resolution", StringArray { "Low", "Medium", "High", "Max" }, 1));
    L.add (std::make_unique<AudioParameterChoice> (ParameterID { "analyzer", v }, "Analyzer", StringArray { "Off", "Pre", "Post", "Pre+Post" }, 3));
    L.add (std::make_unique<AudioParameterFloat>  (ParameterID { "out_gain", v }, "Output Gain", NormalisableRange<float> (-24.f, 24.f, 0.01f), 0.f, AudioParameterFloatAttributes().withLabel ("dB")));

    L.add (std::make_unique<AudioParameterInt> (ParameterID { "solo", v }, "Solo Band", 0, kMaxBands, 0));

    NormalisableRange<float> freq (10.f, 30000.f, 0.01f);  freq.setSkewForCentre (1000.f);
    NormalisableRange<float> qr   (0.025f, 40.f, 0.001f);  qr.setSkewForCentre (1.f);
    NormalisableRange<float> atk  (0.1f, 500.f, 0.01f);    atk.setSkewForCentre (20.f);
    NormalisableRange<float> rel  (5.f, 5000.f, 0.1f);     rel.setSkewForCentre (200.f);

    for (int i = 0; i < kMaxBands; ++i)
    {
        const String n = "Band " + String (i + 1) + " ";
        auto id = [i] (const char* s) { return ParameterID { pid (i, s), v }; };
        L.add (std::make_unique<AudioParameterBool>   (id ("on"),    n + "Enabled", false));
        L.add (std::make_unique<AudioParameterChoice> (id ("type"),  n + "Shape", StringArray { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch", "Tilt Shelf" }, 0));
        L.add (std::make_unique<AudioParameterFloat>  (id ("freq"),  n + "Frequency", freq, 1000.f, AudioParameterFloatAttributes().withLabel ("Hz")));
        L.add (std::make_unique<AudioParameterFloat>  (id ("gain"),  n + "Gain", NormalisableRange<float> (-30.f, 30.f, 0.01f), 0.f, AudioParameterFloatAttributes().withLabel ("dB")));
        L.add (std::make_unique<AudioParameterFloat>  (id ("q"),     n + "Q", qr, 1.f));
        L.add (std::make_unique<AudioParameterChoice> (id ("slope"), n + "Slope", StringArray { "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct", "72 dB/oct", "96 dB/oct", "Brickwall" }, 1));
        L.add (std::make_unique<AudioParameterChoice> (id ("ch"),    n + "Channel", StringArray { "Stereo", "Left", "Right", "Mid", "Side" }, 0));
        L.add (std::make_unique<AudioParameterBool>   (id ("dyn"),   n + "Dynamic", false));
        L.add (std::make_unique<AudioParameterChoice> (id ("dir"),   n + "Dynamic Mode", StringArray { "Compress", "Expand" }, 0));
        L.add (std::make_unique<AudioParameterFloat>  (id ("thr"),   n + "Threshold", NormalisableRange<float> (-90.f, 0.f, 0.1f), -30.f, AudioParameterFloatAttributes().withLabel ("dB")));
        L.add (std::make_unique<AudioParameterFloat>  (id ("range"), n + "Range", NormalisableRange<float> (-30.f, 30.f, 0.1f), -6.f, AudioParameterFloatAttributes().withLabel ("dB")));
        L.add (std::make_unique<AudioParameterFloat>  (id ("atk"),   n + "Attack", atk, 15.f, AudioParameterFloatAttributes().withLabel ("ms")));
        L.add (std::make_unique<AudioParameterFloat>  (id ("rel"),   n + "Release", rel, 180.f, AudioParameterFloatAttributes().withLabel ("ms")));
        L.add (std::make_unique<AudioParameterBool>   (id ("wide"),  n + "Wide Detection", false));
    }
    return L;
}
}
