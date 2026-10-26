#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <vector>
#include "Engine.h"
#include "Params.h"

#define DJX_RAW_PARAMS(X) \
    X(bypass) X(kill) X(inGain) X(outGain) X(dryWet) X(engage) X(fade) X(midi) X(autoRand) X(chaos) \
    X(padX) X(padY) X(padDepth) X(padTouch) X(padSpring) \
    X(pitchWin) X(harmDown) X(harmUp) X(whamPos) X(diveDepth) X(diveTime) X(diveCurve) X(detCents) \
    X(fuzzDrive) X(fuzzTone) X(tapeDrive) X(tapeTone) X(tapeWear) X(tapeBias) X(ringFreq) X(filtMode) X(filtFreq) X(filtRes) X(filtRate) X(filtDepth) X(wahSens) X(radioLow) X(radioHigh) X(radioDrive) \
    X(glDiv) X(tsTime) X(frzSize) X(grainSize) X(grainDensity) X(grainPitch) X(grainJitter) \
    X(choRate) X(choDepth) X(flRate) X(flFb) X(phRate) X(phFb) X(trDiv) X(trDepth) X(vibRate) X(vibDepth) \
    X(deathyRate) X(deathyDepth) X(deathyTone) X(deathyRes) X(hauntRate) X(hauntDepth) X(hauntTone) X(hauntRes) \
    X(dlyDiv) X(dlyMs) X(dlyFb) X(dlyTone) X(dlyPing) X(dlyWidth) X(dlySat) X(dlyMod) X(dlyDuck) \
    X(revDecay) X(revDamp) X(revPre) X(revSize) X(revRoom) X(revMod) X(revWidth) X(revFreeze) \
    X(shDecay) X(shFb) X(shInt) X(shTone) X(shWidth) X(shMod)

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

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float> uiAmt[djx::kNumFx];
    std::atomic<float> uiEngage { 0.f };
    std::atomic<int> uiSelectedFx { -1 };
    std::atomic<unsigned int> uiPresetRevision { 0 };

private:
    void handleAsyncUpdate() override { randomizeNow(); }
    void buildSettings (djx::Settings& s, bool engagedNow);
    void applySettingsAndRun (float* L, float* R, int n, const djx::Settings& s, const djx::Tempo& t);

    struct Raw
    {
#define X(n) std::atomic<float>* n = nullptr;
        DJX_RAW_PARAMS(X)
#undef X
        std::atomic<float>* amt[djx::kNumFx] {};
        std::atomic<float>* nodeChoice[16] {};
        std::atomic<float>* nodeEnable[16] {};
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LunaFXProcessor)
};
