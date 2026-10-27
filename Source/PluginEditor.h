#pragma once
#include "PluginProcessor.h"
#include "UiWidgets.h"

constexpr int kDesignW = 1440;
constexpr int kDesignH = 900;
constexpr int kTopH    = 58;
constexpr int kLeftW   = 716;
constexpr int kRackX   = 724;
constexpr int kRackW   = 350;
constexpr int kCtxX    = 1082;
constexpr int kCtxW    = 358;

// ─────────────────────────────────────────────────────────────────────────────
//  The eight-slot rack. Every row is one of the eight XY slots.
// ─────────────────────────────────────────────────────────────────────────────
class RackPanel : public juce::Component
{
public:
    explicit RackPanel (LunaFXProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    void tick (float dt);
    void flashPreset();
    void setSelectedSlot (int slot);

    std::function<void (int)> onSelectSlot;

private:
    void categorySelected (int cat);
    bool rawEnabled (int slot) const;

    LunaFXProcessor& proc;
    ui::CategoryBar cats { ui::accent() };
    std::array<std::unique_ptr<ui::SlotRow>, params::kNumSlots> rows;
    std::unique_ptr<ui::KnobCard> inKnob, outKnob, dryKnob;
    int selected = -1;
    int lastTheme = -1;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Context panel for the selected slot: big knobs, real parameters only.
// ─────────────────────────────────────────────────────────────────────────────
class ContextPanel : public juce::Component
{
public:
    explicit ContextPanel (LunaFXProcessor& p);
    void showSlot (int slot);
    void refresh();
    void tick();
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void rebuild();

    LunaFXProcessor& proc;
    int slot = -1;
    int fx = -2;
    float lastLive = -1.f;
    float lastMax = -1.f;
    bool lastEnabled = true;
    std::vector<std::unique_ptr<ui::KnobCard>> cards;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Preset library overlay.
// ─────────────────────────────────────────────────────────────────────────────
class PresetOverlay : public juce::Component,
                      public juce::Button::Listener
{
public:
    explicit PresetOverlay (LunaFXProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void buttonClicked (juce::Button*) override;
    void refresh();
    void openSave();
    void setFolder (int f);

    std::function<void()> onClose;

private:
    struct Row { int program = -1; int file = -1; };
    void rebuild();
    void loadRow (int row);
    juce::File userDir() const;
    std::vector<juce::File> userFiles() const;
    void saveWithName (const juce::String& name);

    struct Model : juce::ListBoxModel
    {
        PresetOverlay* owner = nullptr;
        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    } model;

    LunaFXProcessor& proc;
    juce::OwnedArray<juce::TextButton> catButtons;
    juce::TextButton factoryBtn { "FACTORY" }, userBtn { "MY PRESETS" };
    juce::TextButton closeBtn { "CLOSE" }, saveBtn { "+ SAVE" };
    juce::ListBox list;
    juce::String activeCategory { "All" };
    std::vector<juce::String> categories;
    std::vector<Row> rows;
    std::vector<juce::File> cachedUserFiles;
    int folder = 0;
    std::unique_ptr<juce::TextEditor> saveEditor;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Editor
// ─────────────────────────────────────────────────────────────────────────────
class LunaFXEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit LunaFXEditor (LunaFXProcessor&);
    ~LunaFXEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Content : public juce::Component
    {
        explicit Content (LunaFXEditor& o) : owner (o) {}
        void paint (juce::Graphics& g) override { owner.paintContent (g); }
        LunaFXEditor& owner;
    };

    void paintContent (juce::Graphics&);
    void timerCallback() override;
    void setSelectedSlot (int slot);
    void openPresetBrowser (bool show);
    void launchSave();
    void updatePresetName();

    LunaFXProcessor& proc;
    ui::LunaLnF lnf;
    Content content { *this };

    ui::LunarCircle circle;
    RackPanel rack;
    ContextPanel context;
    PresetOverlay presets;

    ui::AccentButton accentButton;
    ui::KillButton kill;
    ui::BypassButton bypass;

    std::unique_ptr<ui::KnobCard> depthKnob, fadeKnob, chaosKnob;
    std::unique_ptr<ui::ParamToggle> touchToggle, springToggle;

    juce::TextButton presetPrev { "<" }, presetNext { ">" };
    juce::TextButton presetName;
    juce::TextButton saveBtn { "SAVE" }, browseBtn { "PRESETS" };
    juce::TextButton themeBtn { "DAY" };
    juce::TooltipWindow tooltip { this, 620 };

    bool presetPanelVisible = false;
    int selectedSlot = 0;
    unsigned int lastPresetRevision = 0;
    int lastTheme = 0;
    juce::String lastPresetName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LunaFXEditor)
};
