#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <vector>
#include "Engine.h"
#include "Params.h"

#define DJX_RAW_PARAMS(X) \
    X(bypass) X(kill) X(inGain) X(outGain) X(dryWet) X(engage) X(fade) X(midi) X(autoRand) X(chaos) \
    X(padX) X(padY) X(padDepth) X(padTouch) X(padSpring) \
    X(pitchWin) X(harmDown) X(harmUp) X(harmDownFine) X(harmUpFine) X(harmDownLvl) X(harmUpLvl) X(harmSpread) \
    X(whamPos) X(diveDepth) X(diveTime) X(diveCurve) X(detCents) \
    X(fuzzDrive) X(fuzzTone) X(fuzzBias) X(fuzzMode) X(fuzzComp) \
    X(tapeDrive) X(tapeTone) X(tapeWear) X(tapeBias) X(tapeComp) \
    X(ringFreq) X(ringShape) X(ringSpread) \
    X(filtMode) X(filtShape) X(filtFreq) X(filtRes) X(filtRate) X(filtDepth) \
    X(radMode) X(radioLow) X(radioHigh) X(radioDrive) X(radRes) X(radNoise) X(wahSens) \
    X(glDiv) X(tsTime) X(frzSize) \
    X(grainSize) X(grainDensity) X(grainPitch) X(grainJitter) X(grainPos) X(grainReverse) X(grainSpread) X(grainFb) \
    X(choRate) X(choDepth) X(flRate) X(flFb) X(phRate) X(phFb) X(trDiv) X(trDepth) X(vibRate) X(vibDepth) \
    X(deathyRate) X(deathyDepth) X(deathyTone) X(deathyRes) \
    X(hauntRate) X(hauntDepth) X(hauntTone) X(hauntRes) \
    X(ghostRate) X(ghostDepth) X(ghostTone) X(ghostRes) \
    X(tremorRate) X(tremorDepth) X(tremorShape) X(tremorRes) \
    X(dlyMode) X(dlyDiv) X(dlyMs) X(dlyFb) X(dlyTone) X(dlyPing) X(dlyWidth) X(dlySat) X(dlyMod) X(dlyDuck) X(dlyFreeze) \
    X(revDecay) X(revDamp) X(revPre) X(revSize) X(revRoom) X(revDiff) X(revEarly) X(revMod) X(revWidth) X(revFreeze) \
    X(shDecay) X(shFb) X(shInt) X(shLow) X(shTone) X(shWidth) X(shMod)

class LunaFXProcessor : public juce::AudioProcessor,
                        private juce::AsyncUpdater
{
public:
    LunaFXProcessor();
    ~LunaFXProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Luna FX"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter ("bypass"); }

    int getNumPrograms() override { return (int) params::presets().size(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}
    void setUserPresetName (const juce::String& name) { currentUserPresetName = name; }

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void randomizeNow();

    // Real-time safe panic request. The audio thread consumes the counter and
    // clears every delay, granular and reverb tail without touching parameters.
    void requestKill() noexcept { killRequest.fetch_add (1, std::memory_order_relaxed); }
    unsigned int killRequestCount() const noexcept { return killRequest.load (std::memory_order_relaxed); }

    int  slotFx (int slot) const noexcept;
    float slotLiveMix (int slot) const noexcept;

    template <typename Fn>
    void forEachSlot (Fn&& fn) const
    {
        for (int i = 0; i < params::kNumSlots; ++i) fn (i, slotFx (i));
    }

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float> uiAmt[djx::kNumFx];
    std::atomic<float> uiSlotWet[params::kNumSlots];
    std::atomic<float> uiEngage { 0.f };
    std::atomic<int> uiSelectedSlot { -1 };
    std::atomic<unsigned int> uiPresetRevision { 0 };

private:
    void handleAsyncUpdate() override { randomizeNow(); }
    void buildSettings (djx::Settings& s, bool engagedNow);
    void migrateAndSanitiseState (juce::ValueTree& vt);

    struct Raw
    {
#define X(n) std::atomic<float>* n = nullptr;
        DJX_RAW_PARAMS(X)
#undef X
        std::atomic<float>* amt[djx::kNumFx] {};
        std::atomic<float>* slotChoice[params::kNumSlots] {};
        std::atomic<float>* slotEnable[params::kNumSlots] {};
        std::atomic<float>* slotLevel[params::kNumSlots] {};
    } raw;

    djx::Engine engine;
    djx::Settings settings;
    std::vector<float> monoR;
    double currentSampleRate = 48000.0;
    double freeBeat = 0.0;
    int heldNotes = 0;
    bool prevEngaged = false;
    bool prevKill = false;
    int currentProgram = 0;
    juce::String currentUserPresetName;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bypassSm;
    juce::AudioBuffer<float> dryBuf;
    std::vector<float> bypassRamp;
    bool wasBypassed = false;

    // Kill switch fade-in avoids a click while still clearing tails immediately.
    std::atomic<unsigned int> killRequest { 0 };
    unsigned int lastKillRequest = 0;
    float killFade = 1.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LunaFXProcessor)
};
