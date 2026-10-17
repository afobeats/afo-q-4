#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Types.h"

namespace afoq
{
inline juce::String pid (int band, const char* name) { return "b" + juce::String (band + 1) + "_" + name; }
juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}
