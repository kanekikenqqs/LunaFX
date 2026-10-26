#pragma once
#include "PluginProcessor.h"
#include "UiWidgets.h"

// Design canvas – host window is scaled to match
constexpr int kDesignW = 1440, kDesignH = 860;
constexpr int kNavW    = 58;    // left nav strip
constexpr int kTopH    = 50;    // top bar
constexpr int kSplitX  = 700;   // XY pad | right panel split
constexpr int kRightX  = kNavW + kSplitX;

// ──────────────────────────────────────────────────────────────────────────────
//  RingPadNode  — one assignable instrument slot on the circle rim
// ──────────────────────────────────────────────────────────────────────────────
struct RingPadNode
{
    float   angleDeg  = 0.f;    // position on the ring
    int     fxIndex   = -1;     // -1 = unassigned
    bool    active    = false;  // is this FX currently producing sound?
    float   activity  = 0.f;   // 0..1 level meter
};

// ──────────────────────────────────────────────────────────────────────────────
//  FxDetailPanel  — right-hand knob grid + shape strip for a selected FX
// ──────────────────────────────────────────────────────────────────────────────
class FxDetailPanel : public juce::Component
{
public:
    explicit FxDetailPanel (LunaFXProcessor& p);
    void showFx (int fxIndex);            // -1 = clear / nothing selected
    void refresh();                        // call from timer to update values
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    LunaFXProcessor& proc;
    int               currentFx = -1;
    // up to 8 knobs for the selected FX detail params
    std::vector<std::unique_ptr<ui::ParamControl>> knobs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxDetailPanel)
};

// ──────────────────────────────────────────────────────────────────────────────
//  FxCategoryPanel  — category tabs + scrollable FX list with amount + toggle
// ──────────────────────────────────────────────────────────────────────────────
class FxCategoryPanel : public juce::Component,
                        public juce::Button::Listener
{
public:
    explicit FxCategoryPanel (LunaFXProcessor& p, FxDetailPanel& detail);
    void refresh();         // call from timer
    void resized() override;
    void paint (juce::Graphics&) override;
    void buttonClicked (juce::Button*) override;

    // returns currently selected FX index (-1 = none)
    int selectedFx() const { return selFx; }

    void selectCategory (int cat);   // public – called by LunaFXEditor

private:
    void buildCategoryRows();

    LunaFXProcessor& proc;
    FxDetailPanel&   detail;
    int activeCat = 0;
    int selFx     = -1;

    // category tab buttons (6)
    juce::OwnedArray<juce::TextButton> catTabs;
    // per-FX row: name label, amount slider, power toggle
    struct FxRow
    {
        int   fxIdx;
        std::unique_ptr<juce::TextButton>  nameBtn;  // click to select / expand
        std::unique_ptr<juce::Slider>      amt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amtAtt;
        std::unique_ptr<juce::ToggleButton> power;
        bool  onPad = false;
    };
    std::vector<FxRow> rows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxCategoryPanel)
};

// ──────────────────────────────────────────────────────────────────────────────
//  PresetBrowserPanel  — factory + user folder browser
// ──────────────────────────────────────────────────────────────────────────────
class PresetBrowserPanel : public juce::Component,
                           public juce::Button::Listener
{
public:
    explicit PresetBrowserPanel (LunaFXProcessor& p);
    void resized() override;
    void paint (juce::Graphics&) override;
    void buttonClicked (juce::Button*) override;
    void refreshList();

    void saveCurrentPreset();         // public – called by LunaFXEditor

private:
    void loadPreset (int programIndex);

    LunaFXProcessor& proc;
    int  activeFolder = 0;   // 0=Factory, 1=My Presets

    juce::OwnedArray<juce::TextButton> folderBtns;
    juce::ListBox presetList;

    juce::File userPresetDirectory() const;
    std::vector<juce::File> userPresetFiles() const;
    void loadUserPreset (int row);
    std::vector<juce::File> cachedUserPresets;

    struct PresetListModel : public juce::ListBoxModel
    {
        PresetBrowserPanel* owner = nullptr;
        LunaFXProcessor* proc = nullptr;
        int folder = 0;    // 0=factory programs, 1=saved user presets
        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics& g,
                               int w, int h, bool selected) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    } listModel;

    juce::TextButton saveBtn { "+ SAVE" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBrowserPanel)
};

// ──────────────────────────────────────────────────────────────────────────────
//  PlayPage  — XY ring pad + bottom controls
// ──────────────────────────────────────────────────────────────────────────────
class PlayPage : public juce::Component
{
public:
    explicit PlayPage (LunaFXProcessor& p);
    void tick (float dt);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp   (const juce::MouseEvent&) override;

    // call when preset changes to re-read node assignments
    void syncNodesToPreset();

private:
    void drawRingPad (juce::Graphics&);
    void drawNode    (juce::Graphics&, const RingPadNode&, juce::Point<float> centre, float R);
    RingPadNode* hitTestNode (juce::Point<float> pos, float R, juce::Point<float> centre);

    LunaFXProcessor& proc;
    ui::CircleXYPad  pad;      // the existing XY component (reused for puck drag)
    ui::AccentButton accent;
    std::unique_ptr<ui::ParamControl> chaos, rate, depth, feedback;
    ui::IconButton shuffleBtn { ui::IconButton::Shuffle },
                   prevBtn    { ui::IconButton::Prev    },
                   nextBtn    { ui::IconButton::Next    };

    // 8 ring nodes (replaces CornerReadout)
    static constexpr int kNumNodes = 8;
    RingPadNode nodes[kNumNodes];

    // drag state for node assignment
    int  draggingNode = -1;
    bool dragConsumed = false;

    juce::String shownPreset;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayPage)
};

// ──────────────────────────────────────────────────────────────────────────────
//  SetupPage — global settings and 16 configurable XY routing nodes
// ──────────────────────────────────────────────────────────────────────────────
class SetupPage : public juce::Component
{
public:
    explicit SetupPage (LunaFXProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    std::vector<std::unique_ptr<ui::ParamControl>> knobs, combos, toggles;
    std::vector<std::unique_ptr<ui::ParamControl>> zoneCombos, zoneToggles; // 16 configurable rim nodes
};

// ──────────────────────────────────────────────────────────────────────────────
//  LunaFXEditor
// ──────────────────────────────────────────────────────────────────────────────
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
    void showNav (int idx);  // 0=XY 1=PITCH 2=FX 3=GLITCH 4=MOD 5=GEN 6=TIME 7=PRE 8=SET

    LunaFXProcessor& proc;
    ui::LunaLnF      lnf;
    Content          content { *this };

    // top-bar widgets
    juce::TextButton  presetPrev { "<" }, presetNext { ">" };
    ui::PresetLabel   presetLabel;
    juce::TextButton  saveBtn { "SAVE" }, folderBtn { "FOLDER" };
    juce::Slider      inSlider, outSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inAtt, outAtt;
    std::unique_ptr<ui::ParamControl> dryWet;
    ui::KillButton    kill;
    ui::BypassButton  bypass;

    // left nav buttons (8)
    juce::OwnedArray<juce::TextButton> navBtns;
    int  activeNav = 0;

    // pages
    PlayPage         play;
    SetupPage        setup;

    // right panel (persistent)
    FxDetailPanel    fxDetail;
    FxCategoryPanel  fxCats;
    PresetBrowserPanel presets;
    bool presetPanelVisible = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LunaFXEditor)
};
