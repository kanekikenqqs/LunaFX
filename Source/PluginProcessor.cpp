#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>

using namespace juce;

LunaFXProcessor::LunaFXProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", params::createLayout())
{
#define X(n) raw.n = apvts.getRawParameterValue (#n);
    DJX_RAW_PARAMS(X)
#undef X
    for (int i = 0; i < djx::kNumFx; ++i)
    {
        raw.amt[i] = apvts.getRawParameterValue (params::amtId (i));
        uiAmt[i].store (0.f);
    }
    for (int s = 0; s < params::kNumSlots; ++s)
    {
        raw.slotChoice[s] = apvts.getRawParameterValue (params::slotChoiceId (s));
        raw.slotEnable[s] = apvts.getRawParameterValue (params::slotEnableId (s));
        raw.slotLevel[s]  = apvts.getRawParameterValue (params::slotLevelId (s));
        uiSlotWet[s].store (0.f);
    }
}

bool LunaFXProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo()) return false;
    return out == l.getMainInputChannelSet();
}

void LunaFXProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    if (! std::isfinite (sampleRate) || sampleRate < 8000.0 || sampleRate > 384000.0)
        sampleRate = 48000.0;
    samplesPerBlock = jmax (1, samplesPerBlock);
    currentSampleRate = sampleRate;
    engine.prepare (sampleRate);
    monoR.assign ((size_t) jmax (512, samplesPerBlock), 0.f);
    heldNotes = 0;
    prevEngaged = false;
    prevKill = false;
    lastKillRequest = killRequest.load (std::memory_order_relaxed);
    killFade = 1.f;
    for (auto& w : uiSlotWet) w.store (0.f);

    const int maxBlock = jmax (8192, samplesPerBlock);
    dryBuf.setSize (2, maxBlock);
    dryBuf.clear();
    bypassRamp.assign ((size_t) maxBlock, 0.f);
    bypassSm.reset (sampleRate, 0.015);
    wasBypassed = raw.bypass->load() > 0.5f;
    bypassSm.setCurrentAndTargetValue (wasBypassed ? 1.f : 0.f);
}

int LunaFXProcessor::slotFx (int slot) const noexcept
{
    if (slot < 0 || slot >= params::kNumSlots || raw.slotChoice[slot] == nullptr) return -1;
    const int choice = (int) (raw.slotChoice[slot]->load() + 0.5f);
    return params::fxFromChoice (choice);
}

float LunaFXProcessor::slotLiveMix (int slot) const noexcept
{
    if (slot < 0 || slot >= params::kNumSlots) return 0.f;
    return uiSlotWet[slot].load (std::memory_order_relaxed);
}

// ------------------------------------------------------------------------------------------
void LunaFXProcessor::buildSettings (djx::Settings& s, bool engagedNow)
{
    using namespace djx;
    auto V = [&] (std::atomic<float>* p) { return p != nullptr ? p->load() : 0.f; };
    auto CL = [&] (std::atomic<float>* p, float lo, float hi) { return jlimit (lo, hi, V (p)); };
    auto IDX = [&] (std::atomic<float>* p, int hi) { return jlimit (0, hi, (int) (V (p) + 0.5f)); };

    s.engaged = engagedNow;
    s.fadeMs  = CL (raw.fade, 1.f, 200.f);
    s.inGain  = Decibels::decibelsToGain (CL (raw.inGain, -60.f, 24.f), -60.f);
    s.outGain = Decibels::decibelsToGain (CL (raw.outGain, -60.f, 24.f), -60.f);

    // ------------------------------------------------------------------ slots
    // Exactly eight slots are mixed. Only an assigned, enabled slot with a
    // valid instrument can contribute; everything else is forced dry. This is
    // what prevents enabled-but-unassigned library instruments from leaking
    // audio (the old implementation left every a_* value active).
    s.amt.fill (0.f);
    s.outLevel.fill (1.f);
    for (auto& w : uiSlotWet) w.store (0.f);

    const float depth = CL (raw.padDepth, 0.f, 1.f);
    float prox[params::kNumSlots] {};
    params::slotWeights (CL (raw.padX, 0.f, 1.f) * 2.f - 1.f,
                         CL (raw.padY, 0.f, 1.f) * 2.f - 1.f, prox);

    bool used[kNumFx] {};
    for (int slot = 0; slot < params::kNumSlots; ++slot)
    {
        const int fx = slotFx (slot);
        if (! params::isVisibleFx (fx) || used[fx]) continue;
        if (V (raw.slotEnable[slot]) <= 0.5f) continue;
        if (raw.amt[fx] == nullptr) continue;

        used[fx] = true;
        const float maxMix = jlimit (0.f, 1.f, V (raw.amt[fx]));
        const float eff = jlimit (0.f, 1.f, maxMix * depth * prox[slot]);
        s.amt[(size_t) fx] = eff;
        s.outLevel[(size_t) fx] = Decibels::decibelsToGain (CL (raw.slotLevel[slot], -24.f, 12.f), -60.f);
        uiSlotWet[slot].store (eff, std::memory_order_relaxed);
    }

    // ------------------------------------------------------------------ pitch
    s.pitchWinMs = CL (raw.pitchWin, 20.f, 120.f);
    s.harmDownSemis = CL (raw.harmDown, -24.f, 0.f);
    s.harmUpSemis = CL (raw.harmUp, 0.f, 24.f);
    s.harmDownFine = CL (raw.harmDownFine, -50.f, 50.f);
    s.harmUpFine = CL (raw.harmUpFine, -50.f, 50.f);
    s.harmDownLvl = CL (raw.harmDownLvl, 0.f, 1.f);
    s.harmUpLvl = CL (raw.harmUpLvl, 0.f, 1.f);
    s.harmSpread = CL (raw.harmSpread, 0.f, 1.f);
    s.whamSemis = CL (raw.whamPos, -24.f, 24.f);
    s.diveSemis = CL (raw.diveDepth, 1.f, 36.f);
    s.diveTime = CL (raw.diveTime, 0.1f, 4.f);
    s.diveCurve = IDX (raw.diveCurve, 3);
    s.detCents = CL (raw.detCents, 1.f, 50.f);

    // --------------------------------------------------------------------- FX
    s.fuzzDrive = CL (raw.fuzzDrive, 0.f, 1.f);
    s.fuzzTone = CL (raw.fuzzTone, 0.f, 1.f);
    s.fuzzBias = CL (raw.fuzzBias, -1.f, 1.f);
    s.fuzzMode = IDX (raw.fuzzMode, 1);
    s.fuzzComp = CL (raw.fuzzComp, 0.f, 1.f);

    s.tapeDrive = CL (raw.tapeDrive, 0.f, 1.f);
    s.tapeTone = CL (raw.tapeTone, 0.f, 1.f);
    s.tapeWear = CL (raw.tapeWear, 0.f, 1.f);
    s.tapeBias = CL (raw.tapeBias, -1.f, 1.f);
    s.tapeComp = CL (raw.tapeComp, 0.f, 1.f);

    s.ringFreq = CL (raw.ringFreq, 20.f, 4000.f);
    s.ringShape = IDX (raw.ringShape, 3);
    s.ringSpread = CL (raw.ringSpread, 0.f, 1.f);

    s.filtMode = IDX (raw.filtMode, 2);
    s.filtShape = IDX (raw.filtShape, 3);
    s.filtFreq = CL (raw.filtFreq, 40.f, 12000.f);
    s.filtRes = CL (raw.filtRes, 0.f, 1.f);
    s.filtRate = CL (raw.filtRate, 0.1f, 20.f);
    s.filtDepth = CL (raw.filtDepth, 0.f, 1.f);

    s.radMode = IDX (raw.radMode, 5);
    s.radioLow = CL (raw.radioLow, 180.f, 1800.f);
    s.radioHigh = CL (raw.radioHigh, 1200.f, 16000.f);
    s.radioDrive = CL (raw.radioDrive, 2.f, 16.f);
    s.radRes = CL (raw.radRes, 0.f, 1.f);
    s.radNoise = CL (raw.radNoise, 0.f, 1.f);
    s.wahSens = CL (raw.wahSens, 0.f, 1.f);

    // ----------------------------------------------------------------- glitch
    s.glBeats = kDivBeats[IDX (raw.glDiv, kNumDivs - 1)];
    s.tsTime = CL (raw.tsTime, 0.05f, 3.f);
    s.frzMs = CL (raw.frzSize, 40.f, 400.f);
    s.grainSizeMs = CL (raw.grainSize, 20.f, 500.f);
    s.grainDensity = CL (raw.grainDensity, 1.f, 40.f);
    s.grainPitch = CL (raw.grainPitch, 0.25f, 4.f);
    s.grainJitter = CL (raw.grainJitter, 0.f, 1.f);
    s.grainPos = CL (raw.grainPos, 0.f, 1.f);
    s.grainReverse = CL (raw.grainReverse, 0.f, 1.f);
    s.grainSpread = CL (raw.grainSpread, 0.f, 1.f);
    s.grainFb = CL (raw.grainFb, 0.f, 0.6f);

    // -------------------------------------------------------------------- mod
    s.choRate = CL (raw.choRate, 0.1f, 5.f);
    s.choDepth = CL (raw.choDepth, 0.f, 1.f);
    s.flRate = CL (raw.flRate, 0.05f, 5.f);
    s.flFb = CL (raw.flFb, -0.9f, 0.9f);
    s.phRate = CL (raw.phRate, 0.05f, 8.f);
    s.phFb = CL (raw.phFb, 0.f, 0.9f);
    s.trBeats = kDivBeats[IDX (raw.trDiv, kNumDivs - 1)];
    s.trDepth = CL (raw.trDepth, 0.f, 1.f);
    s.vibRate = CL (raw.vibRate, 0.5f, 12.f);
    s.vibDepth = CL (raw.vibDepth, 0.f, 1.f);

    s.deathyRate = CL (raw.deathyRate, 0.02f, 8.f);
    s.deathyDepth = CL (raw.deathyDepth, 0.f, 1.f);
    s.deathyTone = CL (raw.deathyTone, 0.f, 1.f);
    s.deathyRes = CL (raw.deathyRes, 0.f, 1.f);
    s.hauntRate = CL (raw.hauntRate, 0.02f, 5.f);
    s.hauntDepth = CL (raw.hauntDepth, 0.f, 1.f);
    s.hauntTone = CL (raw.hauntTone, 0.f, 1.f);
    s.hauntRes = CL (raw.hauntRes, 0.f, 1.f);
    s.ghostRate = CL (raw.ghostRate, 0.02f, 5.f);
    s.ghostDepth = CL (raw.ghostDepth, 0.f, 1.f);
    s.ghostTone = CL (raw.ghostTone, 0.f, 1.f);
    s.ghostRes = CL (raw.ghostRes, 0.f, 1.f);
    s.tremorRate = CL (raw.tremorRate, 0.05f, 12.f);
    s.tremorDepth = CL (raw.tremorDepth, 0.f, 1.f);
    s.tremorShape = IDX (raw.tremorShape, 2);
    s.tremorRes = CL (raw.tremorRes, 0.f, 1.f);

    // ------------------------------------------------------------------- time
    s.dlyMode = IDX (raw.dlyMode, 6);
    const int dd = IDX (raw.dlyDiv, kNumDivs);
    s.dlyFree = dd >= kNumDivs;
    s.dlyBeats = kDivBeats[jmin (dd, kNumDivs - 1)];
    s.dlyMs = CL (raw.dlyMs, 20.f, 2000.f);
    s.dlyFb = CL (raw.dlyFb, 0.f, 0.95f);
    s.dlyTone = CL (raw.dlyTone, 500.f, 12000.f);
    s.dlyPing = CL (raw.dlyPing, 0.f, 1.f);
    s.dlyWidth = CL (raw.dlyWidth, 0.f, 1.f);
    s.dlySat = CL (raw.dlySat, 0.f, 1.f);
    s.dlyMod = CL (raw.dlyMod, 0.f, 1.f);
    s.dlyDuck = CL (raw.dlyDuck, 0.f, 1.f);
    s.dlyFreeze = V (raw.dlyFreeze) > 0.5f;

    s.revRoom = IDX (raw.revRoom, 9);
    s.revDecay = CL (raw.revDecay, 0.3f, 20.f);
    s.revDamp = CL (raw.revDamp, 0.f, 1.f);
    s.revPreMs = CL (raw.revPre, 0.f, 200.f);
    s.revSize = CL (raw.revSize, 0.5f, 1.8f);
    s.revDiff = CL (raw.revDiff, 0.f, 1.f);
    s.revEarly = CL (raw.revEarly, 0.f, 1.f);
    s.revMod = CL (raw.revMod, 0.f, 1.f);
    s.revWidth = CL (raw.revWidth, 0.f, 1.f);
    s.revFreeze = V (raw.revFreeze) > 0.5f;

    s.shDecay = CL (raw.shDecay, 1.f, 20.f);
    s.shFb = CL (raw.shFb, 0.f, 0.85f);
    s.shSemis = CL (raw.shInt, -12.f, 12.f);
    s.shLow = CL (raw.shLow, 0.f, 1.f);
    s.shTone = CL (raw.shTone, 1200.f, 16000.f);
    s.shWidth = CL (raw.shWidth, 0.f, 1.f);
    s.shMod = CL (raw.shMod, 0.f, 1.f);
}

void LunaFXProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    if (n <= 0 || numCh <= 0) return;
    for (int ch = getTotalNumInputChannels(); ch < numCh; ++ch) buffer.clear (ch, 0, n);

    // Emergency kill: clear tails immediately but keep parameters and fade the
    // output back in over 8 ms so the panic action never clicks.
    const bool killParam = raw.kill->load() > 0.5f;
    const unsigned int killSeq = killRequest.load (std::memory_order_relaxed);
    const bool killEdge = (killSeq != lastKillRequest) || (killParam && ! prevKill);
    if (killEdge)
    {
        engine.clearTails();
        killFade = 0.f;
        lastKillRequest = killSeq;
        for (auto& w : uiSlotWet) w.store (0.f, std::memory_order_relaxed);
    }
    prevKill = killParam;

    // ---- MIDI: any held note engages the accent (optional)
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn()) ++heldNotes;
        else if (m.isNoteOff()) heldNotes = jmax (0, heldNotes - 1);
        else if (m.isAllNotesOff() || m.isAllSoundOff()) heldNotes = 0;
    }
    const bool midiOn = raw.midi->load() > 0.5f;
    if (! midiOn) heldNotes = 0;

    // ---- bypass: fully bypassed => audio passes through untouched and the engine sleeps
    const bool byp = raw.bypass->load() > 0.5f;
    bypassSm.setTargetValue (byp ? 1.f : 0.f);
    const bool canFade = n <= dryBuf.getNumSamples() && numCh <= dryBuf.getNumChannels() && n <= (int) bypassRamp.size();
    if (byp && (! canFade || (! bypassSm.isSmoothing() && bypassSm.getCurrentValue() >= 1.f)))
    {
        if (! wasBypassed)
        {
            engine.clearTails();
            for (auto& w : uiSlotWet) w.store (0.f, std::memory_order_relaxed);
            for (auto& a : uiAmt) a.store (0.f);
            uiEngage.store (0.f);
        }
        wasBypassed = true;
        prevEngaged = false;
        bypassSm.setCurrentAndTargetValue (1.f);
        return;
    }
    wasBypassed = false;

    const bool crossfade = canFade && (byp || bypassSm.isSmoothing() || bypassSm.getCurrentValue() > 0.f);
    const bool canMixDryWet = canFade;
    if (canMixDryWet)
        for (int ch = 0; ch < numCh; ++ch) dryBuf.copyFrom (ch, 0, buffer, ch, 0, n);

    const bool engagedNow = raw.engage->load() > 0.5f || (midiOn && heldNotes > 0);

    // ---- auto randomize on engage / release
    const int rm = (int) (raw.autoRand->load() + 0.5f);
    if ((rm == 1 && engagedNow && ! prevEngaged) || (rm == 2 && ! engagedNow && prevEngaged))
        triggerAsyncUpdate();
    prevEngaged = engagedNow;

    buildSettings (settings, engagedNow);

    // ---- tempo
    djx::Tempo t;
    bool playing = false; double bpm = 120.0, ppq = 0.0; bool hasPpq = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            playing = pos->getIsPlaying();
            if (auto p = pos->getPpqPosition()) { ppq = *p; hasPpq = true; }
        }
    t.bpm = bpm;
    t.playing = playing && hasPpq;
    t.beat = t.playing ? ppq : freeBeat;
    const double beatPerSample = jlimit (20.0, 400.0, bpm) / (60.0 * currentSampleRate);
    freeBeat += (double) n * beatPerSample;

    if (numCh >= 2)
    {
        engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n, settings, t);
    }
    else
    {
        float* L = buffer.getWritePointer (0);
        const int chunk = (int) monoR.size();
        djx::Tempo tc = t;
        for (int pos = 0; pos < n; pos += chunk)
        {
            const int m = jmin (chunk, n - pos);
            std::copy (L + pos, L + pos + m, monoR.begin());
            engine.process (L + pos, monoR.data(), m, settings, tc);
            tc.beat += (double) m * beatPerSample;
        }
    }

    if (crossfade)
    {
        for (int i = 0; i < n; ++i) bypassRamp[(size_t) i] = bypassSm.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* out = buffer.getWritePointer (ch);
            const float* dry = dryBuf.getReadPointer (ch);
            for (int i = 0; i < n; ++i)
            {
                const float m = bypassRamp[(size_t) i];
                out[i] = out[i] * (1.f - m) + dry[i] * m;
            }
        }
    }

    // Master parallel mix, independent of the Accent footswitch and of the XY.
    if (canMixDryWet)
    {
        const float wet = jlimit (0.f, 1.f, raw.dryWet->load());
        if (wet < 0.9999f)
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* out = buffer.getWritePointer (ch);
                const float* dry = dryBuf.getReadPointer (ch);
                for (int i = 0; i < n; ++i) out[i] = dry[i] * (1.f - wet) + out[i] * wet;
            }
    }

    // Kill fade-in. Repeated hits restart at zero, so the action always works.
    if (killFade < 1.f)
    {
        const float inc = 1.f / std::max (1.f, 0.008f * (float) currentSampleRate);
        for (int i = 0; i < n; ++i)
        {
            killFade = std::min (1.f, killFade + inc);
            for (int ch = 0; ch < numCh; ++ch) buffer.getWritePointer (ch)[i] *= killFade;
        }
    }

    for (int i = 0; i < djx::kNumFx; ++i) uiAmt[i].store (settings.amt[(size_t) i]);
    uiEngage.store (engine.engageGain());
}

// ------------------------------------------------------------------------------------------
void LunaFXProcessor::setCurrentProgram (int index)
{
    const auto& ps = params::presets();
    if (! isPositiveAndBelow (index, (int) ps.size())) return;
    currentProgram = index;
    currentUserPresetName.clear();
    uiPresetRevision.fetch_add (1, std::memory_order_relaxed);

    // Reset every preset-scoped parameter, then apply the preset. This is the
    // same behaviour as the previous version, but the slot assignments and the
    // new effect controls are included.
    for (auto& d : params::defs())
        if (d.inPreset)
            if (auto* p = apvts.getParameter (d.id))
                p->setValueNotifyingHost (p->getDefaultValue());

    for (auto& kv : ps[(size_t) index].v)
        if (auto* p = apvts.getParameter (kv.id))
            p->setValueNotifyingHost (jlimit (0.f, 1.f, p->convertTo0to1 (kv.val)));

    if (auto* killParam = apvts.getParameter ("kill"))
        killParam->setValueNotifyingHost (killParam->convertTo0to1 (0.f));

    updateHostDisplay();
}

const String LunaFXProcessor::getProgramName (int index)
{
    if (index == currentProgram && currentUserPresetName.isNotEmpty())
        return currentUserPresetName;
    const auto& ps = params::presets();
    return isPositiveAndBelow (index, (int) ps.size()) ? String (ps[(size_t) index].name.c_str()) : String();
}

// ------------------------------------------------------------------------------------------
void LunaFXProcessor::randomizeNow()
{
    using namespace djx;
    Random& R = Random::getSystemRandom();
    const float chaos = raw.chaos->load();
    auto flag = [this] (int i)
    {
        if (auto* p = apvts.getRawParameterValue ("rnd" + String (i))) return p->load() > 0.5f;
        return false;
    };
    auto setNorm = [this] (const std::string& id, float norm)
    {
        if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (jlimit (0.f, 1.f, norm));
    };
    auto setReal = [this] (const std::string& id, float value)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (jlimit (0.f, 1.f, p->convertTo0to1 (value)));
    };

    std::vector<int> pool;     // instruments belonging to enabled categories
    for (int i = 0; i < kNumFx; ++i)
        if (params::isVisibleFx (i) && kFxCategory[i] >= 0 && kFxCategory[i] <= 5 && flag (kFxCategory[i]))
            pool.push_back (i);
    if (pool.empty())
        for (int i = 0; i < kNumFx; ++i) if (params::isVisibleFx (i)) pool.push_back (i);

    bool anyOn = false;
    for (auto& d : params::defs())
    {
        if (d.group < 0 || d.group > 5 || ! flag (d.group)) continue;
        auto* p = apvts.getParameter (d.id);
        if (p == nullptr) continue;

        if (d.id.rfind ("a_", 0) == 0)
        {
            const bool on = R.nextFloat() < 0.10f + 0.55f * chaos;
            anyOn = anyOn || on;
            setNorm (d.id, on ? 0.4f + 0.6f * R.nextFloat() : 0.f);
        }
        else if (d.kind == params::kChoice || d.kind == params::kInt)
        {
            if (R.nextFloat() < 0.35f + 0.65f * chaos) setNorm (d.id, R.nextFloat());
        }
        else
        {
            const float cur = p->getValue(), target = R.nextFloat();
            setNorm (d.id, cur + (target - cur) * (0.35f + 0.65f * chaos));
        }
    }

    if (! anyOn && ! pool.empty())
        setNorm (params::amtId (pool[(size_t) R.nextInt ((int) pool.size())]), 0.6f + 0.4f * R.nextFloat());

    if (flag (5)) // randomise the eight slot assignments and levels
    {
        // Fisher-Yates on the category-filtered pool, then take the first eight.
        for (int i = (int) pool.size() - 1; i > 0; --i)
        {
            const int j = R.nextInt (i + 1);
            std::swap (pool[(size_t) i], pool[(size_t) j]);
        }
        for (int slot = 0; slot < params::kNumSlots; ++slot)
        {
            const int fx = slot < (int) pool.size() ? pool[(size_t) slot] : -1;
            setReal (params::slotChoiceId (slot), (float) (fx >= 0 ? params::choiceForFx (fx) : 0));
            setReal (params::slotEnableId (slot), R.nextFloat() < 0.82f ? 1.f : 0.f);
            setReal (params::slotLevelId (slot), -6.f + 8.f * R.nextFloat());
            if (fx >= 0)
                setReal (params::amtId (fx), 0.45f + 0.45f * R.nextFloat());
        }
    }
}

// ------------------------------------------------------------------------------------------
void LunaFXProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("userPresetName", currentUserPresetName, nullptr);
    state.setProperty ("layoutVersion", 8, nullptr);
    state.setProperty ("version", params::kVersion, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void LunaFXProcessor::migrateAndSanitiseState (ValueTree& vt)
{
    // Reset preset-scoped parameters that are absent from older/partial state
    // instead of leaving values from the previously loaded preset behind.
    StringArray present;
    for (int i = 0; i < vt.getNumChildren(); ++i)
    {
        const auto child = vt.getChild (i);
        if (child.hasType ("PARAM"))
            present.add (child.getProperty ("id").toString());
    }
    for (auto& d : params::defs())
        if (d.inPreset && ! present.contains (String (d.id.c_str())))
            if (auto* p = apvts.getParameter (d.id))
                p->setValueNotifyingHost (p->getDefaultValue());

    // Only eight slots exist. Keep the first occurrence of each instrument and
    // silently unassign later duplicates so a malformed state cannot process an
    // instrument twice or route audio through a hidden slot.
    bool used[djx::kNumFx] {};
    for (int slot = 0; slot < params::kNumSlots; ++slot)
    {
        const int fx = slotFx (slot);
        const bool valid = params::isVisibleFx (fx) && ! used[std::max (0, fx)];
        if (fx >= 0 && valid) used[fx] = true;
        if (fx >= 0 && ! valid)
            if (auto* p = apvts.getParameter (params::slotChoiceId (slot)))
                p->setValueNotifyingHost (0.f);
    }

    // Legacy 16-node sessions are accepted: n0..n7 keep their values, and
    // n8..n15 are simply ignored. The removed program index is clamped.
    currentProgram = jlimit (0, jmax (0, getNumPrograms() - 1), (int) vt.getProperty ("program", 0));
    currentUserPresetName = vt.getProperty ("userPresetName", String()).toString();

    if (auto* killParam = apvts.getParameter ("kill"))
        killParam->setValueNotifyingHost (killParam->convertTo0to1 (0.f));
    if (auto* engageParam = apvts.getParameter ("engage"))
        engageParam->setValueNotifyingHost (engageParam->convertTo0to1 (0.f));

    uiPresetRevision.fetch_add (1, std::memory_order_relaxed);
}

void LunaFXProcessor::setStateInformation (const void* data, int size)
{
    // Hosts can restore old, partial, or externally edited project state. Do not
    // trust the stored program index or persist a pressed momentary KILL button.
    if (data == nullptr || size <= 0) return;

    if (auto xml = getXmlFromBinary (data, size))
    {
        if (! xml->hasTagName (apvts.state.getType())) return;

        auto vt = ValueTree::fromXml (*xml);
        if (! vt.isValid() || vt.getType() != apvts.state.getType()) return;

        // Drop removed 16-node and other unknown PARAM children before restore.
        // Old n0..n7 values are retained and keep working; n8..n15 are ignored
        // rather than kept as stale automation lanes forever.
        for (int i = vt.getNumChildren(); --i >= 0;)
        {
            const auto child = vt.getChild (i);
            if (! child.hasType ("PARAM")) continue;
            const auto id = child.getProperty ("id").toString();
            if (params::find (id.toStdString()) == nullptr)
                vt.removeChild (i, nullptr);
        }

        apvts.replaceState (vt);
        migrateAndSanitiseState (vt);
        updateHostDisplay();
    }
}

AudioProcessorEditor* LunaFXProcessor::createEditor() { return new LunaFXEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LunaFXProcessor(); }
