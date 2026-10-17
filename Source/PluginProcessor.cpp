#include "PluginProcessor.h"
#include "PluginEditor.h"
using namespace afoq;

AfoQ4Processor::AfoQ4Processor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AFOQ4", createLayout())
{
    auto raw = [this] (const juce::String& id) { return apvts.getRawParameterValue (id); };
    for (int i = 0; i < kMaxBands; ++i)
        ptrs[(size_t) i] = { raw (pid (i, "on")), raw (pid (i, "type")), raw (pid (i, "freq")), raw (pid (i, "gain")), raw (pid (i, "q")),
                             raw (pid (i, "slope")), raw (pid (i, "ch")), raw (pid (i, "dyn")), raw (pid (i, "dir")), raw (pid (i, "thr")),
                             raw (pid (i, "range")), raw (pid (i, "atk")), raw (pid (i, "rel")), raw (pid (i, "wide")) };
    phasePtr = raw ("phase"); resPtr = raw ("lin_res"); outPtr = raw ("out_gain"); soloPtr = raw ("solo");
}

bool AfoQ4Processor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto o = l.getMainOutputChannelSet();
    return (o == juce::AudioChannelSet::stereo() || o == juce::AudioChannelSet::mono()) && o == l.getMainInputChannelSet();
}

void AfoQ4Processor::prepareToPlay (double sampleRate, int blockSize)
{
    sr = sampleRate;
    scratch.assign ((size_t) blockSize, 0.f); dryL = dryR = scratch;
    engine.prepare (sr);
    linear.prepare (sr, blockSize);
    outGain.reset (sr, 0.02);  outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outPtr->load()));
    modeFade.reset (sr, 0.012); modeFade.setCurrentAndTargetValue (1.f);
    soloMix.reset (sr, 0.010);  soloMix.setCurrentAndTargetValue (0.f);
    activeMode = (int) phasePtr->load(); fadingOut = false; lastRes = -1; lastSolo = -1;
    linear.setActive (activeMode == (int) PhaseMode::Linear);
    linear.setOrder (12 + (int) resPtr->load());
    setLatencySamples (activeMode == (int) PhaseMode::Linear ? linear.latencySamples() : 0);
}

void AfoQ4Processor::readParams()
{
    for (size_t i = 0; i < (size_t) kMaxBands; ++i)
    {
        const auto& q = ptrs[i]; auto& p = params[i];
        p.on = q.on->load() > 0.5f;
        p.type = (FilterType) (int) q.type->load();
        p.freq = q.freq->load(); p.gain = q.gain->load(); p.q = q.q->load();
        p.slope = kSlopes[juce::jlimit (0, 8, (int) q.slope->load())];
        p.ch = (ChannelMode) (int) q.ch->load();
        p.dynOn = q.dyn->load() > 0.5f; p.expand = q.dir->load() > 0.5f; p.wide = q.wide->load() > 0.5f;
        p.thr = q.thr->load(); p.range = q.range->load(); p.atk = q.atk->load(); p.rel = q.rel->load();
    }
}

void AfoQ4Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n > (int) scratch.size()) { scratch.resize ((size_t) n); dryL.resize ((size_t) n); dryR.resize ((size_t) n); }
    float* l = buffer.getWritePointer (0);
    float* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : scratch.data();
    if (buffer.getNumChannels() == 1) std::copy (l, l + n, r);

    readParams();
    const int reqMode = (int) phasePtr->load(), res = (int) resPtr->load();
    const int linMode = (int) PhaseMode::Linear;

    // --- resolution change ---
    if (res != lastRes)
    {
        lastRes = res; linear.setOrder (12 + res);
        if (activeMode == linMode) { pendingLatency.store (linear.latencySamples()); triggerAsyncUpdate(); }
    }

    // --- click-free mode switching: fade out, swap, fade in. Linear Phase is entered only once its IR is ready. ---
    if (reqMode != activeMode)
    {
        linear.setActive (true);
        if (reqMode != linMode || linear.isReady())
        {
            if (! fadingOut) { modeFade.setTargetValue (0.f); fadingOut = true; }
            if (fadingOut && ! modeFade.isSmoothing() && modeFade.getCurrentValue() < 1e-4f)
            {
                activeMode = reqMode; fadingOut = false; linear.reset();
                pendingLatency.store (activeMode == linMode ? linear.latencySamples() : 0); triggerAsyncUpdate();
                modeFade.setTargetValue (1.f);
            }
        }
    }
    else linear.setActive (activeMode == linMode);

    const int solo = (int) soloPtr->load() - 1;
    const bool soloing = solo >= 0 || soloMix.isSmoothing() || soloMix.getCurrentValue() > 0.f;
    if (soloing) { std::copy (l, l + n, dryL.begin()); std::copy (r, r + n, dryR.begin()); }

    preAnalyzer.push (l, r, n);
    engine.setParams (params);

    if (activeMode == linMode)
    {
        engine.process (l, r, n, false);                                // detectors only: drive the dynamic IR gains
        for (int i = 0; i < kMaxBands; ++i) snapshot.k[(size_t) i] = engine.dynamicAmount (i);
        snapshot.p = params; linear.setSnapshot (snapshot);
        linear.process (l, r, n);
    }
    else
    {
        engine.setNatural (activeMode == (int) PhaseMode::Natural);
        engine.process (l, r, n, true);
        if (reqMode == linMode)                                         // keep the IR designer fed while we wait to switch
        { for (int i = 0; i < kMaxBands; ++i) snapshot.k[(size_t) i] = engine.dynamicAmount (i); snapshot.p = params; linear.setSnapshot (snapshot); }
    }

    // --- solo audition: band-pass (or the cut-away region) of the dry input ---
    if (soloing)
    {
        soloMix.setTargetValue (solo >= 0 ? 1.f : 0.f);
        if (solo != lastSolo && solo >= 0) { soloZ1[0] = soloZ1[1] = soloZ2[0] = soloZ2[1] = 0; }
        if (solo >= 0)
        {
            lastSolo = solo; const auto& p = params[(size_t) solo];
            soloC = (p.type == FilterType::LowShelf || p.type == FilterType::LowCut)   ? biquadPass (false, sr, p.freq, 0.707)
                  : (p.type == FilterType::HighShelf || p.type == FilterType::HighCut) ? biquadPass (true,  sr, p.freq, 0.707)
                                                                                      : bandPass (sr, p.freq, p.q);
            soloCh = p.ch;
        }
        for (int i = 0; i < n; ++i)
        {
            float a0, a1; getLanes (soloCh, dryL[(size_t) i], dryR[(size_t) i], a0, a1);
            double y[2], x[2] = { a0, a1 };
            for (int c = 0; c < 2; ++c)
            {
                y[c] = soloC.b0 * x[c] + soloZ1[c];
                soloZ1[c] = soloC.b1 * x[c] - soloC.a1 * y[c] + soloZ2[c];
                soloZ2[c] = soloC.b2 * x[c] - soloC.a2 * y[c];
            }
            float sl = 0.f, sr2 = 0.f; putLanes (soloCh, sl, sr2, (float) y[0], (float) y[1]);
            const float m = soloMix.getNextValue();
            l[i] += m * (sl - l[i]); r[i] += m * (sr2 - r[i]);
        }
    }

    // --- mode fade + output gain + meters ---
    outGain.setTargetValue (juce::Decibels::decibelsToGain (outPtr->load()));
    float pl = 0.f, pr = 0.f;
    for (int i = 0; i < n; ++i)
    {
        const float g = outGain.getNextValue() * modeFade.getNextValue();
        l[i] *= g; r[i] *= g; pl = std::max (pl, std::abs (l[i])); pr = std::max (pr, std::abs (r[i]));
    }
    if (pl > meterL.load (std::memory_order_relaxed)) meterL.store (pl, std::memory_order_relaxed);
    if (pr > meterR.load (std::memory_order_relaxed)) meterR.store (pr, std::memory_order_relaxed);
    postAnalyzer.push (l, r, n);
}

void AfoQ4Processor::handleAsyncUpdate() { setLatencySamples (pendingLatency.load()); }

void AfoQ4Processor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}
void AfoQ4Processor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* AfoQ4Processor::createEditor() { return new AfoQ4Editor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AfoQ4Processor(); }
