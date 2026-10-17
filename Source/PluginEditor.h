#pragma once
#include "PluginProcessor.h"
#include "UI/Theme.h"
#include "UI/ParamBank.h"
#include "UI/EqDisplay.h"
#include "UI/BandPanel.h"

/** Drag-to-edit readout bound to one parameter (used for the output gain). */
class ValueLabel : public juce::Component
{
public:
    ValueLabel (juce::RangedAudioParameter& p, std::function<juce::String (float)> f) : param (p), fmt (std::move (f)),
        att (p, [this] (float) { repaint(); }) { att.sendInitialUpdate(); }
    void paint (juce::Graphics& g) override
    {
        g.setColour (hov ? juce::Colours::white : juce::Colour (0xffa2a2b0)); g.setFont (afoq::ui::font (11.5f));
        g.drawText (fmt (param.convertFrom0to1 (param.getValue())), getLocalBounds(), juce::Justification::centred);
    }
    void mouseDown (const juce::MouseEvent& e) override { att.beginGesture(); y0 = e.y; n0 = param.getValue(); }
    void mouseDrag (const juce::MouseEvent& e) override
    { att.setValueAsPartOfGesture (param.convertFrom0to1 (juce::jlimit (0.f, 1.f, n0 + (float) (y0 - e.y) / (e.mods.isShiftDown() ? 800.f : 200.f)))); }
    void mouseUp (const juce::MouseEvent&) override { att.endGesture(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { att.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue())); }
    void mouseEnter (const juce::MouseEvent&) override { hov = true; repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hov = false; repaint(); }
private:
    juce::RangedAudioParameter& param; std::function<juce::String (float)> fmt; juce::ParameterAttachment att;
    bool hov = false; int y0 = 0; float n0 = 0;
};

class AfoQ4Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AfoQ4Editor (AfoQ4Processor&);
    ~AfoQ4Editor() override { stopTimer(); setLookAndFeel (nullptr); }
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void selectBand (int i);
    void fixSelection();
    void commit();
    void restore (int idx);
    void loadPreset (int i);
    void menuFor (juce::Component& c, const juce::StringArray& items, juce::RangedAudioParameter& p);

    AfoQ4Processor& proc;
    afoq::ui::AfoLookAndFeel lnf;
    afoq::ui::Bank bank;
    afoq::ui::EqDisplay display;
    afoq::ui::BandPanel panel;
    juce::TextButton undoB { juce::String (juce::CharPointer_UTF8 ("\xe2\x86\xb6")) }, redoB { juce::String (juce::CharPointer_UTF8 ("\xe2\x86\xb7")) },
                     abB { "A/B" }, copyB { "Copy" }, prevB { "<" }, presetB, nextB { ">" },
                     rangeB { "12 dB" }, phaseB, resB, analyzerB;
    ValueLabel outLabel;

    std::vector<juce::ValueTree> hist; int hi = -1;
    juce::ValueTree slotA, slotB; bool onB = false;
    int sel = -1, presetIdx = 0, range = 12;
};
