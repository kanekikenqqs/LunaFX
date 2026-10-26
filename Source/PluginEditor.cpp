#include <algorithm>
#include "PluginEditor.h"

using namespace juce;
using namespace ui;

// ============================================================  colour helpers
static Colour lunaBase()    { return Colour (0xff090a0e); }
static Colour lunaPanelDk() { return Colour (0xff0d0e16); }
static Colour lunaPanelMd() { return Colour (0xff13141e); }
static Colour lunaPanelLt() { return Colour (0xff191b26); }
static Colour lunaBorder()  { return Colour (0xff2a2d3e); }
static Colour lunaBorderBr(){ return Colour (0xff3a3e58); }
static Colour lunaText()    { return Colour (0xffebe8f5); }
static Colour lunaSub()     { return Colour (0xff9b98ae); }
static Colour lunaDim()     { return Colour (0xff373a50); }

static Colour catCol (int cat)
{
    return params::catColour (jlimit (0, 5, cat));
}
// cat names used for top-bar / header
static const char* kCatName[6] = { "PITCH", "FX", "GLITCH", "MOD", "INTERNAL", "TIME" };
static constexpr int kVisibleCategories[5] = { 0, 1, 2, 3, 5 };
static constexpr const char* kVisibleCategoryNames[5] = { "PITCH", "FX", "GLITCH", "MOD", "TIME" };
static int visibleCategoryForTab (int tab) { return (tab >= 0 && tab < 5) ? kVisibleCategories[tab] : 0; }

// ============================================================  small draw helpers
static void fillRR (Graphics& g, Rectangle<float> r, float rad, Colour c)
    { g.setColour (c); g.fillRoundedRectangle (r, rad); }
static void strokeRR (Graphics& g, Rectangle<float> r, float rad, Colour c, float thick = 1.f)
    { g.setColour (c); g.drawRoundedRectangle (r, rad, thick); }

// ============================================================  FxDetailPanel
FxDetailPanel::FxDetailPanel (LunaFXProcessor& p) : proc (p) {}

void FxDetailPanel::showFx (int fxIndex)
{
    if (fxIndex < -1 || fxIndex >= djx::kNumFx || (fxIndex >= 0 && ! params::isVisibleFx (fxIndex))) fxIndex = -1;
    if (fxIndex == currentFx) return;
    currentFx = fxIndex;
    knobs.clear();
    if (fxIndex < 0) { repaint(); return; }

    const auto& info = params::fxInfo()[(size_t) fxIndex];
    for (const auto& det : info.details)
    {
        const auto* pd = params::find (det);
        if (!pd) continue;
        int cat = djx::kFxCategory[fxIndex];
        auto kn = std::make_unique<ParamControl> (proc.apvts, det.c_str(),
                                                  pd->name.c_str(), catCol (cat));
        addAndMakeVisible (*kn);
        knobs.push_back (std::move (kn));
    }
    // also add the amount knob
    auto amtId = params::amtId (fxIndex);
    auto amtKn = std::make_unique<ParamControl> (proc.apvts, amtId.c_str(), "Mix",
                                                  catCol (djx::kFxCategory[fxIndex]));
    addAndMakeVisible (*amtKn);
    knobs.insert (knobs.begin(), std::move (amtKn));

    resized();
    repaint();
}

void FxDetailPanel::refresh()
{
    for (auto& k : knobs) k->repaint();
}

void FxDetailPanel::resized()
{
    if (knobs.empty()) return;
    auto area  = getLocalBounds().reduced (10, 8);
    area.removeFromTop (36); // reserve the title strip; controls must not cover the FX name
    const int kw   = 88;
    const int kh   = 92;
    const int cols = jmax (1, area.getWidth() / kw);
    int x = area.getX(), y = area.getY(), col = 0;
    for (auto& k : knobs)
    {
        k->setBounds (x, y, kw, kh);
        x += kw;
        if (++col >= cols) { col = 0; x = area.getX(); y += kh; }
    }
}

void FxDetailPanel::paint (Graphics& g)
{
    if (currentFx < 0)
    {
        g.setColour (lunaSub());
        g.setFont (Font (12.f));
        g.drawText ("Select an FX on the ring to edit",
                    getLocalBounds().toFloat(), Justification::centred);
        return;
    }
    // header bar
    int cat = djx::kFxCategory[currentFx];
    auto hdr = getLocalBounds().removeFromTop (36).toFloat();
    g.setColour (catCol (cat).withAlpha (0.12f));
    g.fillRoundedRectangle (hdr, 6.f);
    // left colour strip
    g.setColour (catCol (cat));
    g.fillRoundedRectangle (hdr.withWidth (4.f), 2.f);
    // text
    g.setFont (Font (14.f).boldened());
    g.setColour (catCol (cat));
    g.drawText (String (kCatName[cat]) + "  —  " +
                String (djx::kFxNames[currentFx]).toUpperCase(),
                hdr.withLeft (12.f).withTrimmedRight (8.f),
                Justification::centredLeft);
}

// ============================================================  FxCategoryPanel
FxCategoryPanel::FxCategoryPanel (LunaFXProcessor& p, FxDetailPanel& d)
    : proc (p), detail (d)
{
    // Only categories that contain real instruments are exposed.
    lastProgram = proc.getCurrentProgram();
    lastPresetRevision = proc.uiPresetRevision.load (std::memory_order_relaxed);
    for (int i = 0; i < 5; ++i)
    {
        auto* btn = catTabs.add (new TextButton (kVisibleCategoryNames[i]));
        btn->addListener (this);
        addAndMakeVisible (btn);
    }
    selectCategory (0);
}

void FxCategoryPanel::selectCategory (int cat, bool clearSelection)
{
    if (cat != 0 && cat != 1 && cat != 2 && cat != 3 && cat != 5) cat = 0;
    if (clearSelection)
    {
        selFx = -1;
        proc.uiSelectedFx.store (-1, std::memory_order_relaxed);
        detail.showFx (-1);
    }
    activeCat = cat;
    rows.clear();
    for (auto* c : catTabs) removeChildComponent (c);
    for (auto* c : catTabs) addAndMakeVisible (c);

    // build rows for every FX in this category
    for (int i = 0; i < djx::kNumFx; ++i)
    {
        if (! params::isVisibleFx (i) || djx::kFxCategory[i] != cat) continue;
        FxRow row;
        row.fxIdx = i;

        row.nameBtn = std::make_unique<TextButton> (djx::kFxNames[i]);
        row.nameBtn->addListener (this);
        addAndMakeVisible (*row.nameBtn);

        row.amt = std::make_unique<Slider> (Slider::LinearHorizontal, Slider::NoTextBox);
        row.amt->setPopupDisplayEnabled (true, true, nullptr);
        row.amt->setTooltip ("Maximum Mix. When assigned to the XY ring, the puck's distance scales the live mix.");
        row.amtAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (
                proc.apvts, params::amtId (i), *row.amt);
        addAndMakeVisible (*row.amt);

        row.power = std::make_unique<ToggleButton>();
        row.power->setClickingTogglesState (false);
        row.power->onClick = [this, i] {
            auto* par = proc.apvts.getParameter (params::amtId (i));
            if (par) {
                const float cur = par->convertFrom0to1 (par->getValue());
                par->beginChangeGesture();
                par->setValueNotifyingHost (par->convertTo0to1 (cur > 0.01f ? 0.f : 0.8f));
                par->endChangeGesture();
            }
        };
        addAndMakeVisible (*row.power);

        rows.push_back (std::move (row));
    }
    resized();
    repaint();
}

void FxCategoryPanel::refresh()
{
    const auto revision = proc.uiPresetRevision.load (std::memory_order_relaxed);
    const int program = proc.getCurrentProgram();
    if (revision != lastPresetRevision || program != lastProgram)
    {
        lastPresetRevision = revision;
        lastProgram = program;
        presetFlash = 1.f;
    }
    else if (isVisible()) presetFlash = jmax (0.f, presetFlash - 0.035f);

    // The circle and right-hand rack share one selection. Clicking a rim node
    // selects its instrument here without requiring the user to hunt the list.
    const int ext = proc.uiSelectedFx.load (std::memory_order_relaxed);
    if (params::isVisibleFx (ext) && ext != selFx)
    {
        if (djx::kFxCategory[ext] != activeCat) selectCategory (djx::kFxCategory[ext]);
        selFx = ext;
        detail.showFx (selFx);
    }

    for (auto& row : rows)
    {
        const float amt = proc.apvts.getRawParameterValue (params::amtId (row.fxIdx))->load();
        row.power->setToggleState (amt > 0.01f, dontSendNotification);

        // Compact choice mapping: choice 0 is None, then visible instrument indexes.
        row.onPad = false;
        for (int ni = 0; ni < 16 && !row.onPad; ++ni)
        {
            const auto* pp = proc.apvts.getRawParameterValue ("n" + String (ni));
            const auto* en = proc.apvts.getRawParameterValue ("n" + String (ni) + "e");
            const int fx = pp != nullptr ? params::fxFromChoice ((int) (pp->load() + 0.5f)) : -1;
            row.onPad = (fx == row.fxIdx && en != nullptr && en->load() > 0.5f);
        }
    }
    repaint();
}

void FxCategoryPanel::buttonClicked (Button* btn)
{
    for (int i = 0; i < catTabs.size(); ++i)
        if (catTabs[i] == btn) { selectCategory (visibleCategoryForTab (i), true); return; }
    for (auto& row : rows)
        if (row.nameBtn.get() == btn)
        {
            selFx = (selFx == row.fxIdx) ? -1 : row.fxIdx;
            proc.uiSelectedFx.store (selFx, std::memory_order_relaxed);
            detail.showFx (selFx);
            repaint();
            return;
        }
}

void FxCategoryPanel::resized()
{
    auto area = getLocalBounds();
    // category tabs row
    auto tabs = area.removeFromTop (28);
    const int count = jmax (1, catTabs.size());
    const int tw = tabs.getWidth() / count;
    for (int i = 0; i < catTabs.size(); ++i)
        catTabs[i]->setBounds (tabs.getX() + i * tw, tabs.getY(), tw - 2, 26);

    area.removeFromTop (4);
    area.removeFromTop (15); // small legend row, drawn in paint()
    area.removeFromTop (2);
    const int rowH = 36;
    for (auto& row : rows)
    {
        auto r = area.removeFromTop (rowH);
        // Reserve room for the on-pad badge; it must never sit on top of the Mix slider.
        row.nameBtn->setBounds (r.getX() + 16, r.getY() + 4, 124, 28);
        row.power  ->setBounds (r.getRight() - 28, r.getY() + 8, 20, 20);
        row.amt    ->setBounds (r.getX() + 188, r.getY() + 10,
                                 r.getWidth() - 188 - 34, 16);
    }
}

void FxCategoryPanel::paint (Graphics& g)
{
    // category tab styling
    for (int i = 0; i < catTabs.size(); ++i)
    {
        auto b = catTabs[i]->getBounds().toFloat();
        const int cat = visibleCategoryForTab (i);
        const bool active = cat == activeCat;
        bool presetUsesCategory = false;
        if (presetFlash > 0.f)
        {
            for (int fx = 0; fx < djx::kNumFx; ++fx)
                if (params::isVisibleFx (fx) && djx::kFxCategory[fx] == cat
                    && proc.apvts.getRawParameterValue (params::amtId (fx))->load() > 0.01f)
                    presetUsesCategory = true;
            // Also glow a category when the preset routes one of its instruments
            // onto the ring, even if its current maximum Mix is still zero.
            for (int ni = 0; ni < 16 && ! presetUsesCategory; ++ni)
            {
                const auto* ch = proc.apvts.getRawParameterValue ("n" + String (ni));
                const auto* en = proc.apvts.getRawParameterValue ("n" + String (ni) + "e");
                const int fx = ch != nullptr ? params::fxFromChoice ((int) (ch->load() + 0.5f)) : -1;
                if (en != nullptr && en->load() > 0.5f && params::isVisibleFx (fx)
                    && djx::kFxCategory[fx] == cat)
                    presetUsesCategory = true;
            }
        }
        const Colour cc = catCol (cat);
        fillRR   (g, b, 5.f, active ? cc.withAlpha (0.9f)
                                  : presetUsesCategory ? cc.withAlpha (0.16f + 0.24f * presetFlash) : lunaPanelMd());
        strokeRR (g, b, 5.f, cc, active ? 2.f : (presetUsesCategory ? 1.4f + 0.7f * presetFlash : 1.f));
        catTabs[i]->setColour (TextButton::textColourOffId, active ? lunaBase() : (presetUsesCategory ? lunaText() : cc));
        catTabs[i]->setColour (TextButton::buttonColourId, Colours::transparentBlack);
        catTabs[i]->setColour (TextButton::buttonOnColourId, Colours::transparentBlack);
    }
    // Keep the pad/rack relationship obvious: row Mix is the ceiling and the
    // puck's proximity supplies the live amount. The secondary meter is live mix.
    g.setColour (lunaSub().withAlpha (0.95f));
    g.setFont (Font (9.f).boldened());
    g.drawText ("PAD PROXIMITY -> LIVE MIX  |  SLIDER = MAX",
                Rectangle<int> (8, 30, getWidth() - 16, 14), Justification::centredLeft);
    // FX rows
    for (auto& row : rows)
    {
        const bool sel    = (row.fxIdx == selFx);
        const float amt   = proc.apvts.getRawParameterValue (params::amtId (row.fxIdx))->load();
        const float liveMix = jlimit (0.f, 1.f, proc.uiAmt[row.fxIdx].load (std::memory_order_relaxed));
        const bool  on    = amt > 0.01f;       // rack maximum is enabled
        const bool  live  = liveMix > 0.015f;  // circle currently brings the instrument in
        const Colour col  = catCol (activeCat);
        auto rb = Rectangle<float>(
            (float) getLocalBounds().getX(),
            (float) row.nameBtn->getBounds().getY() - 3,
            (float) getLocalBounds().getWidth(),
            34.f);
        const float flash = presetFlash * (row.onPad ? 0.68f : (on ? 0.26f : 0.f));
        fillRR   (g, rb, 5.f, sel ? col.withAlpha (0.18f + flash) : lunaPanelLt().brighter (flash));
        strokeRR (g, rb, 5.f, sel ? col : col.withAlpha (0.22f + flash), sel ? 2.f : (1.f + flash));
        // dot
        const float dy = rb.getCentreY();
        g.setColour (live ? col : (on ? col.withAlpha (0.42f) : lunaDim()));
        g.fillEllipse ((float) row.nameBtn->getX() - 14.f, dy - 4.f, 8.f, 8.f);
        // ON PAD badge
        if (row.onPad)
        {
            const float bx = (float) row.nameBtn->getRight() + 6.f;
            auto badge = Rectangle<float> (bx, dy - 9.f, 40.f, 18.f);
            fillRR   (g, badge, 4.f, col.withAlpha (0.18f));
            strokeRR (g, badge, 4.f, col);
            g.setColour (col);
            g.setFont (Font (9.f).boldened());
            g.drawText ("ON PAD", badge, Justification::centred);
        }
        // power ring
        auto pb = row.power->getBounds().toFloat().reduced (2.f);
        g.setColour (on ? col : lunaDim());
        g.drawEllipse (pb, 2.f);
        if (on) g.fillEllipse (pb.reduced (4.f));
        // Two thin meters explain the relationship: the upper line is the
        // configured maximum Mix, the bright lower line is the current XY-scaled mix.
        auto sb = row.amt->getBounds().toFloat();
        const float barY = sb.getBottom() + 1.f;
        g.setColour (lunaBorder());
        g.fillRoundedRectangle (sb.getX(), barY, sb.getWidth(), 2.f, 1.f);
        g.setColour (col.withAlpha (on ? 0.55f : 0.16f));
        g.fillRoundedRectangle (sb.getX(), barY, sb.getWidth() * amt, 2.f, 1.f);
        g.setColour (lunaBorder());
        g.fillRoundedRectangle (sb.getX(), barY + 3.f, sb.getWidth(), 2.f, 1.f);
        g.setColour (col.withAlpha (live ? 0.98f : 0.28f));
        g.fillRoundedRectangle (sb.getX(), barY + 3.f, sb.getWidth() * liveMix, 2.f, 1.f);
        // FX name
        row.nameBtn->setColour (TextButton::textColourOffId, live ? lunaText() : (on ? lunaSub() : lunaDim()));
        row.nameBtn->setColour (TextButton::buttonColourId, Colours::transparentBlack);
        row.nameBtn->setColour (TextButton::buttonOnColourId, Colours::transparentBlack);
    }
}

// ============================================================  PresetBrowserPanel
PresetBrowserPanel::PresetBrowserPanel (LunaFXProcessor& p) : proc (p)
{
    listModel.owner = this;
    listModel.proc = &p;
    listModel.folder = 0;

    // Keep only working destinations visible; empty placeholder tabs were confusing.
    for (const char* lbl : { "Factory", "My Presets" })
    {
        auto* btn = folderBtns.add (new TextButton (lbl));
        btn->addListener (this);
        addAndMakeVisible (btn);
    }
    addAndMakeVisible (presetList);
    presetList.setModel (&listModel);
    presetList.setRowHeight (28);
    presetList.setColour (ListBox::backgroundColourId, Colour (0xff0d0e16));

    saveBtn.addListener (this);
    addAndMakeVisible (saveBtn);
    refreshList();
}

void PresetBrowserPanel::refreshList()
{
    cachedUserPresets = userPresetFiles();
    presetList.updateContent();
    if (activeFolder == 0) presetList.selectRow (proc.getCurrentProgram());
    else presetList.deselectAllRows();
    repaint();
}

juce::File PresetBrowserPanel::userPresetDirectory() const
{
    return File::getSpecialLocation (File::userApplicationDataDirectory)
        .getChildFile ("LunaLabs").getChildFile ("Luna FX").getChildFile ("Presets");
}

std::vector<juce::File> PresetBrowserPanel::userPresetFiles() const
{
    std::vector<File> result;
    const auto dir = userPresetDirectory();
    Array<File> found;
    if (dir.isDirectory()) dir.findChildFiles (found, File::findFiles, false, "*.lfxpreset");
    for (const auto& f : found) result.push_back (f);
    std::sort (result.begin(), result.end(), [] (const File& a, const File& b)
    {
        return a.getFileName().compareIgnoreCase (b.getFileName()) < 0;
    });
    return result;
}

void PresetBrowserPanel::loadUserPreset (int row)
{
    const auto files = userPresetFiles();
    if (! isPositiveAndBelow (row, (int) files.size())) return;
    MemoryBlock state;
    if (files[(size_t) row].loadFileAsData (state) && state.getSize() > 0)
    {
        proc.setStateInformation (state.getData(), (int) state.getSize());
        refreshList();
    }
}

void PresetBrowserPanel::resized()
{
    auto area = getLocalBounds().reduced (6, 4);
    // folder strip
    auto folders = area.removeFromTop (34);
    const int saveW = 64;
    const int folderGap = 4;
    const int fw = jmax (54, (folders.getWidth() - saveW - (folderBtns.size() + 1) * folderGap) / jmax (1, folderBtns.size()));
    for (int i = 0; i < folderBtns.size(); ++i)
        folderBtns[i]->setBounds (folders.getX() + i * (fw + folderGap),
                                  folders.getY() + 3, fw, 28);
    saveBtn.setBounds (folders.getRight() - saveW, folders.getY() + 3, saveW, 28);
    area.removeFromTop (4);
    presetList.setBounds (area);
}

void PresetBrowserPanel::paint (Graphics& g)
{
    g.fillAll (lunaPanelDk());
    g.setColour (lunaBorderBr());
    g.drawLine (0.f, 0.f, (float) getWidth(), 0.f, 1.f);
    // style folder buttons
    for (int i = 0; i < folderBtns.size(); ++i)
    {
        const bool sel = (i == activeFolder);
        auto b = folderBtns[i]->getBounds().toFloat();
        fillRR   (g, b, 5.f, sel ? cAccent().withAlpha (0.18f) : lunaPanelMd());
        strokeRR (g, b, 5.f, sel ? cAccent() : lunaBorder(), sel ? 2.f : 1.f);
        folderBtns[i]->setColour (TextButton::textColourOffId, sel ? cAccent() : lunaSub());
        folderBtns[i]->setColour (TextButton::buttonColourId, Colours::transparentBlack);
    }
    // save button
    auto sb = saveBtn.getBounds().toFloat();
    fillRR   (g, sb, 5.f, lunaPanelMd());
    strokeRR (g, sb, 5.f, cAccent());
    saveBtn.setColour (TextButton::textColourOffId, cAccent());
    saveBtn.setColour (TextButton::buttonColourId, Colours::transparentBlack);
    // header label
    g.setColour (lunaSub());
    g.setFont (Font (10.f).boldened());
    g.drawText ("PRESET BROWSER",
                Rectangle<int> (6, 0, 120, 22), Justification::centredLeft);
}

void PresetBrowserPanel::buttonClicked (Button* btn)
{
    for (int i = 0; i < folderBtns.size(); ++i)
        if (folderBtns[i] == btn) { activeFolder = i; listModel.folder = i; refreshList(); return; }
    if (btn == &saveBtn) saveCurrentPreset();
}

void PresetBrowserPanel::loadPreset (int idx)
    { if (idx >= 0 && idx < proc.getNumPrograms()) proc.setCurrentProgram (idx); }

void PresetBrowserPanel::saveCurrentPreset()
{
    // pop a dialog to name the preset (simple alert for now)
    // JUCE 8: no modal loops – show a simple inline rename editor
    auto* ed = new juce::TextEditor();
    ed->setText (proc.getProgramName (proc.getCurrentProgram()));
    ed->setSelectAllWhenFocused (true);
    ed->setBounds (saveBtn.getBounds().expanded (40, 0).withY (saveBtn.getBottom() + 4));
    addAndMakeVisible (*ed);
    ed->grabKeyboardFocus();
    // confirm on Return / dismiss on Escape using a lambda-based key listener
    Component::SafePointer<PresetBrowserPanel> sp (this);
    Component::SafePointer<juce::TextEditor>   ep (ed);
    ed->onReturnKey = [sp, ep]
    {
        if (sp != nullptr && ep != nullptr)
        {
            auto name = ep->getText().trim();
            name = name.replaceCharacters ("\\/:*?\"<>|", "_________").trim();
            if (name.isNotEmpty())
            {
                auto dir = sp->userPresetDirectory();
                const auto createResult = dir.createDirectory();
                if (createResult.wasOk() || dir.isDirectory())
                {
                    const auto previousName = sp->proc.getProgramName (sp->proc.getCurrentProgram());
                    sp->proc.setUserPresetName (name);
                    MemoryBlock state;
                    sp->proc.getStateInformation (state);
                    const auto target = dir.getChildFile ("Preset - " + name + ".lfxpreset");
                    if (! target.replaceWithData (state.getData(), state.getSize()))
                        sp->proc.setUserPresetName (previousName);
                    else
                    {
                        sp->activeFolder = 1;
                        sp->listModel.folder = 1;
                    }
                }
            }
            sp->refreshList();
            sp->removeChildComponent (ep.getComponent());
            delete ep.getComponent();
        }
    };
    ed->onEscapeKey = [sp, ep]
    {
        if (sp != nullptr && ep != nullptr)
        {
            sp->removeChildComponent (ep.getComponent());
            delete ep.getComponent();
        }
    };
}

int PresetBrowserPanel::PresetListModel::getNumRows()
{
    if (owner == nullptr || proc == nullptr) return 0;
    return folder == 0 ? proc->getNumPrograms() : (int) owner->cachedUserPresets.size();
}

void PresetBrowserPanel::PresetListModel::paintListBoxItem
    (int row, Graphics& g, int w, int h, bool sel)
{
    if (sel) { g.setColour (cAccent().withAlpha (0.2f)); g.fillRoundedRectangle (2.f, 1.f, (float) w - 4.f, (float) h - 2.f, 4.f); }
    g.setColour (sel ? lunaText() : lunaSub());
    g.setFont (Font (12.f));
    String name;
    if (folder == 0)
    {
        const auto& factory = params::presets();
        if (row >= 0 && row < (int) factory.size()) name = factory[(size_t) row].name;
    }
    else if (owner != nullptr)
    {
        const auto& files = owner->cachedUserPresets;
        if (isPositiveAndBelow (row, (int) files.size())) name = files[(size_t) row].getFileNameWithoutExtension().fromFirstOccurrenceOf ("Preset - ", false, false);
    }
    g.drawText (name, Rectangle<int> (12, 0, w - 12, h), Justification::centredLeft);
}

void PresetBrowserPanel::PresetListModel::listBoxItemClicked (int row, const MouseEvent& /*e*/)
{
    if (owner == nullptr || proc == nullptr) return;
    if (folder == 0) owner->loadPreset (row);
    else owner->loadUserPreset (row);
}

// ============================================================  PlayPage ring pad
PlayPage::PlayPage (LunaFXProcessor& p)
    : proc (p), pad (p), accent (p)
{
    addAndMakeVisible (pad);     // the existing puck-drag component
    addAndMakeVisible (accent);
    addAndMakeVisible (shuffleBtn);
    addAndMakeVisible (prevBtn);
    addAndMakeVisible (nextBtn);

    shuffleBtn.onClick = [this] { proc.randomizeNow(); };
    prevBtn.onClick    = [this] { const int n = proc.getNumPrograms();
                                  proc.setCurrentProgram ((proc.getCurrentProgram() + n - 1) % n); };
    nextBtn.onClick    = [this] { proc.setCurrentProgram ((proc.getCurrentProgram() + 1) % proc.getNumPrograms()); };

    chaos    = std::make_unique<ParamControl> (p.apvts, "chaos",    "Chaos",    cAccent());
    rate     = std::make_unique<ParamControl> (p.apvts, "choRate",  "Rate",     catCol (3));
    depth    = std::make_unique<ParamControl> (p.apvts, "padDepth", "Depth",    catCol (0));
    feedback = std::make_unique<ParamControl> (p.apvts, "dlyFb",    "Feedback", catCol (5));

    addAndMakeVisible (*chaos);
    addAndMakeVisible (*rate);
    addAndMakeVisible (*depth);
    addAndMakeVisible (*feedback);

    // Tracking model mirrors the 16 real nodes drawn and controlled by CircleXYPad.
    for (int i = 0; i < kNumNodes; ++i)
        nodes[i].angleDeg = 90.f - 22.5f * (float) i;
    syncNodesToPreset();
}

void PlayPage::syncNodesToPreset()
{
    for (int i = 0; i < kNumNodes; ++i)
    {
        const auto* ch = proc.apvts.getRawParameterValue ("n" + String (i));
        const auto* en = proc.apvts.getRawParameterValue ("n" + String (i) + "e");
        nodes[i].fxIndex = ch != nullptr ? params::fxFromChoice ((int) (ch->load() + 0.5f)) : -1;
        nodes[i].active = en != nullptr && en->load() > 0.5f;
    }
    pad.flashPreset();
    repaint();
}

void PlayPage::tick (float /*dt*/)
{
    // update node activity from uiAmt
    for (int i = 0; i < kNumNodes; ++i)
    {
        const int fx = nodes[i].fxIndex;
        nodes[i].activity = params::isVisibleFx (fx) ? proc.uiAmt[fx].load() : 0.f;
        nodes[i].active   = nodes[i].activity > 0.02f;
    }

    const String name = proc.getProgramName (proc.getCurrentProgram());
    if (name != shownPreset)
    {
        shownPreset = name;
        syncNodesToPreset();  // ← preset switch → ring updates
        repaint();
    }

    pad.tick (1.f / 30.f);
    accent.repaint();
}

void PlayPage::resized()
{
    const auto b = getLocalBounds();
    // Reserve a dedicated bottom performance strip. In the previous layout the
    // circle overlapped the knobs and the ACCENT button, making both hard to use.
    const int pw = jmax (180, jmin (b.getWidth() - 34, b.getHeight() - 220));
    const int px = (b.getWidth() - pw) / 2;
    pad.setBounds (px, 12, pw, pw);

    const int kh = 84, kw = 72;
    const int ky = b.getHeight() - kh - 8;
    const int knobX[4] = { 38, 122, b.getWidth() - 122 - kw, b.getWidth() - 38 - kw };
    ParamControl* controls[4] = { chaos.get(), rate.get(), depth.get(), feedback.get() };
    for (int i = 0; i < 4; ++i)
        controls[i]->setBounds (knobX[i], ky, kw, kh);

    accent.setBounds (b.getCentreX() - 42, b.getHeight() - 100, 84, 92);

    // shuffle/prev/next cluster near top-right
    const int bx = b.getRight() - 120;
    shuffleBtn.setBounds (bx,      16, 34, 28);
    prevBtn   .setBounds (bx + 38, 16, 34, 28);
    nextBtn   .setBounds (bx + 76, 16, 34, 28);
}

void PlayPage::mouseDown (const MouseEvent& e)
{
    const auto b  = pad.getBounds();
    const float R = b.getWidth() * 0.5f;
    const auto  c = b.getCentre().toFloat();
    draggingNode  = -1;
    dragConsumed  = false;
    RingPadNode* n = hitTestNode (e.position.toFloat(), R, c);
    if (n) draggingNode = (int)(n - nodes);
}

void PlayPage::mouseDrag (const MouseEvent& e)
{
    if (draggingNode < 0) return;
    const auto b = pad.getBounds().toFloat();
    const auto c = b.getCentre();
    const float dx = e.position.x - c.x;
    const float dy = e.position.y - c.y;
    // snap angle to nearest 45°
    float ang = std::atan2 (dy, dx) * 180.f / MathConstants<float>::pi;
    if (ang < 0.f) ang += 360.f;
    const float snapped = std::round (ang / 45.f) * 45.f;
    nodes[draggingNode].angleDeg = snapped;
    dragConsumed = true;
    repaint();
}

void PlayPage::mouseUp (const MouseEvent& /*e*/)
{
    draggingNode = -1;
}

RingPadNode* PlayPage::hitTestNode (Point<float> pos, float R, Point<float> c)
{
    for (auto& n : nodes)
    {
        const float rad = MathConstants<float>::pi / 180.f * n.angleDeg;
        Point<float> np (c.x + std::cos (rad) * R, c.y + std::sin (rad) * R);
        if (pos.getDistanceFrom (np) < 14.f) return &n;
    }
    return nullptr;
}

void PlayPage::drawNode (Graphics& g, const RingPadNode& n,
                          Point<float> c, float R)
{
    const int   fx  = n.fxIndex;
    const Colour col = (fx >= 0 && fx < djx::kNumFx)
                       ? catCol (djx::kFxCategory[fx])
                       : lunaDim();
    const float rad = MathConstants<float>::pi / 180.f * n.angleDeg;
    const Point<float> np (c.x + std::cos (rad) * R, c.y + std::sin (rad) * R);

    // spoke
    g.setColour (n.active ? col.withAlpha (0.45f) : lunaDim().withAlpha (0.5f));
    g.drawLine (c.x, c.y, np.x, np.y, 1.f);

    // glow when active
    if (n.active)
        for (int i = 12; i > 0; i -= 2)
            g.setColour (col.withAlpha (0.035f * n.activity * i))
            , g.fillEllipse (np.x - i, np.y - i, i * 2.f, i * 2.f);

    // node circle
    g.setColour (n.active ? col : lunaPanelLt());
    g.fillEllipse (np.x - 9.f, np.y - 9.f, 18.f, 18.f);
    g.setColour (col);
    g.drawEllipse (np.x - 9.f, np.y - 9.f, 18.f, 18.f, 2.f);

    // label — push outward so it never overlaps
    const float LOFF = 22.f;
    const float lx   = c.x + std::cos (rad) * (R + LOFF);
    const float ly   = c.y + std::sin (rad) * (R + LOFF);
    const String lbl = (fx >= 0) ? juce::String (djx::kFxNames[fx]) : "--";

    g.setFont (Font (10.f).boldened());
    g.setColour (n.active ? col : lunaSub());

    // anchor label away from centre so it doesn't overlap the node
    const float lw = g.getCurrentFont().getStringWidthFloat (lbl);
    float ax = lx;
    if (std::cos (rad) > 0.1f)       ax = lx;                // right side
    else if (std::cos (rad) < -0.1f) ax = lx - lw;           // left side
    else                             ax = lx - lw * 0.5f;    // top/bottom
    g.drawSingleLineText (lbl, (int) ax, (int) (ly + (std::sin (rad) > 0.f ? 3.f : -3.f)));
}

void PlayPage::drawRingPad (Graphics& g)
{
    const auto b  = pad.getBounds();
    const float R = b.getWidth() * 0.5f;
    const auto  c = b.getCentre().toFloat();

    // terrain fill (simple gradient mesh behind the XY pad)
    g.setGradientFill (ColourGradient (Colour (0xff0e1020), c.x, c.y,
                                       Colour (0xff070810), c.x + R, c.y + R, true));
    g.fillEllipse (c.x - R, c.y - R, R * 2.f, R * 2.f);

    // subtle grid inside pad
    g.setColour (Colour (0x0a8888cc));
    const int grid = 4;
    for (int i = 1; i < grid; ++i)
    {
        const float t = (float) i / grid;
        g.drawLine (c.x - R, c.y - R + t * R * 2.f, c.x + R, c.y - R + t * R * 2.f, 1.f);
        g.drawLine (c.x - R + t * R * 2.f, c.y - R, c.x - R + t * R * 2.f, c.y + R, 1.f);
    }

    // ring border
    g.setColour (lunaBorderBr());
    g.drawEllipse (c.x - R, c.y - R, R * 2.f, R * 2.f, 2.f);

    // draw all 16 rim nodes (8 per half-circle)
    for (const auto& n : nodes) drawNode (g, n, c, R);

    // hint text below ring (only when no FX active)
    bool anyActive = false;
    for (auto& n : nodes) anyActive |= n.active;
    if (!anyActive)
    {
        g.setFont (Font (10.f));
        g.setColour (lunaDim());
        g.drawText ("drag nodes to reassign  •  click node to edit",
                    (int)(c.x - 140), (int)(c.y + R + 28), 280, 16,
                    Justification::centred);
    }
}

void PlayPage::paint (Graphics& g)
{
    // CircleXYPad is the single source of truth for the ring. Do not draw a
    // second decorative ring underneath it (duplicate nodes confused routing).
    // preset name top-right (avoid overlapping nav/tabs)
    if (shownPreset.isNotEmpty())
    {
        auto tr = getLocalBounds().removeFromTop (56);
        tr = tr.removeFromRight (200).reduced (4, 0);
        g.setFont (Font (11.f));
        g.setColour (lunaSub());
        g.drawText ("PRESET", tr, Justification::topRight);
        g.setFont (Font (18.f).boldened());
        g.setColour (lunaText());
        g.drawFittedText (shownPreset.toUpperCase(),
                          tr.withTrimmedTop (14), Justification::topRight, 1, 0.8f);
    }
}

// ============================================================  SetupPage — global settings + 16-node routing
SetupPage::SetupPage (LunaFXProcessor& p)
{
    auto add = [&] (std::vector<std::unique_ptr<ParamControl>>& v,
                    const String& id, const String& cap, Colour c)
    {
        // ParamControl has its own null guard as a second line of defence.
        v.push_back (std::make_unique<ParamControl> (p.apvts, id, cap, c));
        addAndMakeVisible (*v.back());
    };
    add (knobs, "fade",     "Engage Fade", cAccent());
    add (knobs, "chaos",    "Random Amount", cAccent());
    add (knobs, "padDepth", "XY Depth", cAccent2());

    add (combos, "engMode",  "Footswitch", cAccent());
    add (combos, "autoRand", "Auto Randomize", cAccent());
    add (combos, "uiTheme",  "Visual Theme", cAccent2());

    add (toggles, "midi",      "MIDI Hold = Accent", cAccent());
    add (toggles, "padTouch",  "Touch Pad = Accent", cAccent());
    add (toggles, "padSpring", "Spring Return", cAccent());

    // This section was left as empty IDs in the previous version. Constructing
    // ParamControl with those IDs dereferenced a null parameter; the corresponding
    // controls are now bound to the real n0..n15 and n0e..n15e APVTS parameters.
    for (int i = 0; i < 16; ++i)
    {
        const String id = "n" + String (i);
        const String enabledId = id + "e";
        add (zoneCombos, id, "Node " + String (i + 1), (i < 4) ? cAccent() : cAccent2());
        add (zoneToggles, enabledId, "Enable", (i < 4) ? cAccent() : cAccent2());
    }
}

void SetupPage::resized()
{
    auto area = getLocalBounds().reduced (18, 12);
    area.removeFromTop (48); // title and helper text

    const int gap = 8;
    auto knobsRow = area.removeFromTop (94);
    const int kw = (knobsRow.getWidth() - 2 * gap) / 3;
    for (int i = 0; i < (int) knobs.size(); ++i)
        knobs[(size_t) i]->setBounds (knobsRow.getX() + i * (kw + gap), knobsRow.getY(), kw, knobsRow.getHeight());

    area.removeFromTop (4);
    auto comboRow = area.removeFromTop (54);
    const int cw = (comboRow.getWidth() - 2 * gap) / 3;
    for (int i = 0; i < (int) combos.size(); ++i)
        combos[(size_t) i]->setBounds (comboRow.getX() + i * (cw + gap), comboRow.getY(), cw, comboRow.getHeight());

    area.removeFromTop (4);
    auto toggleRow = area.removeFromTop (34);
    const int tw = (toggleRow.getWidth() - 2 * gap) / 3;
    for (int i = 0; i < (int) toggles.size(); ++i)
        toggles[(size_t) i]->setBounds (toggleRow.getX() + i * (tw + gap), toggleRow.getY(), tw, toggleRow.getHeight());

    area.removeFromTop (10);
    constexpr int cols = 4, rows = 4;
    const int colW = (area.getWidth() - (cols - 1) * gap) / cols;
    const int rowH = (area.getHeight() - (rows - 1) * 6) / rows;
    for (int i = 0; i < 16; ++i)
    {
        const int row = i / cols, col = i % cols;
        auto cell = Rectangle<int> (area.getX() + col * (colW + gap),
                                    area.getY() + row * (rowH + 6), colW, rowH);
        zoneCombos[(size_t) i]->setBounds (cell.removeFromTop (jmin (36, rowH - 24)));
        zoneToggles[(size_t) i]->setBounds (cell.removeFromTop (24));
    }
}

void SetupPage::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (8.f);
    fillRR   (g, b, 16.f, cPanel());
    strokeRR (g, b, 16.f, cLine());
    g.setColour (cText()); g.setFont (Font (18.f).boldened());
    g.drawText ("SETUP & XY ROUTING", b.withTrimmedLeft (20.f).withHeight (38.f), Justification::centredLeft);
    g.setColour (cMuted()); g.setFont (Font (10.f));
    g.drawText ("Choose an effect for each point around the pad and switch points off when not needed.",
                b.withTrimmedLeft (20.f).withY (32.f).withHeight (20.f), Justification::centredLeft);
}

// ============================================================  LunaFXEditor
LunaFXEditor::LunaFXEditor (LunaFXProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      presetLabel (p), kill (p), bypass (p),
      play (p), setup (p),
      fxDetail (p), fxCats (p, fxDetail), presets (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    // ---- top-bar
    // addAndMakeVisible individually: MSVC rejects mixed-subclass brace-init deduction
    content.addAndMakeVisible (presetPrev);
    content.addAndMakeVisible (presetNext);
    content.addAndMakeVisible (presetLabel);
    content.addAndMakeVisible (saveBtn);
    content.addAndMakeVisible (folderBtn);
    content.addAndMakeVisible (kill);
    content.addAndMakeVisible (bypass);

    presetPrev.onClick = [this] { const int n = proc.getNumPrograms();
                                   proc.setCurrentProgram ((proc.getCurrentProgram() + n - 1) % n);
                                   presets.refreshList(); };
    presetNext.onClick = [this] { proc.setCurrentProgram ((proc.getCurrentProgram() + 1) % proc.getNumPrograms());
                                   presets.refreshList(); };
    saveBtn  .onClick  = [this]
    {
        showNav (6);
        presets.refreshList();
        presets.saveCurrentPreset();
    };  // open the browser before placing the inline save field
    folderBtn.onClick  = [this] { presetPanelVisible = !presetPanelVisible;
                                   presets.setVisible (presetPanelVisible); repaint(); };

    for (auto* s : { &inSlider, &outSlider })
    {
        s->setSliderStyle (Slider::LinearHorizontal);
        s->setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        s->setPopupDisplayEnabled (true, true, nullptr);
        content.addAndMakeVisible (*s);
    }
    inAtt  = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "inGain",  inSlider);
    outAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "outGain", outSlider);
    inSlider .setDoubleClickReturnValue (true, 0.0);
    outSlider.setDoubleClickReturnValue (true, 0.0);

    dryWet = std::make_unique<ParamControl> (proc.apvts, "dryWet", "Dry/Wet", cAccent2());
    content.addAndMakeVisible (*dryWet);

    // ---- left nav: the former PRE item now has its own SET page beside presets.
    const char* navLabels[8] = { "XY","PITCH","FX","GLITCH","MOD","TIME","PRE","SET" };
    for (int i = 0; i < 8; ++i)
    {
        auto* btn = navBtns.add (new TextButton (navLabels[i]));
        btn->onClick = [this, i] { showNav (i); };
        content.addAndMakeVisible (btn);
    }

    // ---- pages / panels
    content.addAndMakeVisible (play);
    content.addAndMakeVisible (setup);
    content.addAndMakeVisible (fxDetail);
    content.addAndMakeVisible (fxCats);
    content.addAndMakeVisible (presets);
    presets.setVisible (false);

    showNav (0);

    setResizable (true, true);
    setResizeLimits (kDesignW * 2 / 3, kDesignH * 2 / 3, kDesignW * 2, kDesignH * 2);
    getConstrainer()->setFixedAspectRatio ((double) kDesignW / (double) kDesignH);
    setSize (kDesignW, kDesignH);
    startTimerHz (30);
}

LunaFXEditor::~LunaFXEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void LunaFXEditor::showNav (int idx)
{
    idx = jlimit (0, 7, idx);
    activeNav = idx;

    play.setVisible (idx <= 5);
    setup.setVisible (idx == 7);
    presets.setVisible (idx == 6);

    if (idx >= 1 && idx <= 5)
        fxCats.selectCategory (visibleCategoryForTab (idx - 1), true);

    // The rack stays visible beside the live XY surface. Only PRE and SET use
    // the full right-hand area, so a pad selection always has a matching menu.
    const bool showRack = idx <= 5;
    fxCats.setVisible (showRack);
    fxDetail.setVisible (showRack);

    for (int i = 0; i < navBtns.size(); ++i)
    {
        navBtns[i]->setColour (TextButton::textColourOffId, i == idx ? lunaText() : lunaDim());
        navBtns[i]->setColour (TextButton::buttonColourId, Colours::transparentBlack);
    }
    repaint();
}

void LunaFXEditor::paint (Graphics& g)
{
    // overall bg gradient
    g.setGradientFill (ColourGradient (Colour (0xff0d0e18), 0.f, 0.f,
                                       Colour (0xff090a10), 0.f, (float) getHeight(), false));
    g.fillAll();
}

void LunaFXEditor::paintContent (Graphics& g)
{
    const auto W = (float) kDesignW;
    const auto H = (float) kDesignH;

    // ---- top bar fill
    g.setColour (Colour (0xff0b0c14));
    g.fillRect (0.f, 0.f, W, (float) kTopH);
    g.setColour (lunaBorderBr());
    g.drawLine (0.f, (float) kTopH, W, (float) kTopH, 1.f);

    // ---- LOGO  —  "LUNA" (white) + "FX" (purple)
    g.setFont (Font (26.f).boldened());
    g.setColour (lunaText());
    g.drawText ("LUNA", Rectangle<int> (16, 10, 80, 30), Justification::centredLeft);
    const float lw = g.getCurrentFont().getStringWidthFloat ("LUNA");
    g.setColour (catCol (0));    // purple accent
    g.setFont (Font (18.f).boldened());
    g.drawText ("FX", Rectangle<float> (18.f + lw + 4.f, 14.f, 36.f, 24.f), Justification::centredLeft);

    // version badge
    auto vbadge = Rectangle<float> (138.f, 13.f, 44.f, 22.f);
    fillRR   (g, vbadge, 4.f, Colour (0xff1a1c2a));
    strokeRR (g, vbadge, 4.f, lunaBorder());
    g.setFont (Font (9.f)); g.setColour (lunaSub());
    g.drawText ("1.2.1", vbadge, Justification::centred);

    // ---- IN / OUT labels (above sliders)
    g.setFont (Font (10.f)); g.setColour (lunaSub());
    g.drawText ("IN",  Rectangle<int> (860, 4, 36, 14), Justification::centredLeft);
    g.drawText ("OUT", Rectangle<int> (976, 4, 36, 14), Justification::centredLeft);

    // ---- left nav bg
    g.setColour (Colour (0xff0b0c14));
    g.fillRect (0.f, (float) kTopH, (float) kNavW, H - (float) kTopH);
    g.setColour (lunaBorderBr());
    g.drawLine ((float) kNavW, (float) kTopH, (float) kNavW, H, 1.f);

    // nav active accent strip
    const int navH  = (kDesignH - kTopH - 20) / jmax (1, navBtns.size());
    const int ny    = kTopH + activeNav * navH;
    g.setColour (Colour (0xff181a28));
    g.fillRect (0.f, (float) ny, (float) kNavW, (float) navH);
    const Colour navAccent = (activeNav >= 1 && activeNav <= 5)
                             ? catCol (visibleCategoryForTab (activeNav - 1)) : cAccent();
    g.setColour (navAccent);
    g.fillRect (0.f, (float) ny, 3.f, (float) navH);

    // ---- right panel bg
    g.setColour (Colour (0xff0d0e18));
    g.fillRect ((float) kRightX, (float) kTopH, W - (float) kRightX, H - (float) kTopH);
    g.setColour (lunaBorderBr());
    g.drawLine ((float) kRightX, (float) kTopH, (float) kRightX, H, 1.f);

    // ---- status bar
    g.setColour (Colour (0xff080910));
    g.fillRect (0.f, H - 20.f, W, 20.f);
    g.setColour (lunaBorder());
    g.drawLine (0.f, H - 20.f, W, H - 20.f, 1.f);
    g.setFont (Font (9.f));
    g.setColour (lunaDim());
    g.drawText ("LUNA FX  v1.2.1", Rectangle<float> (8.f, H - 18.f, 130.f, 16.f), Justification::centredLeft);
    g.drawText ("44100 Hz  \u2022  512 smp", Rectangle<float> (W * 0.5f - 80.f, H - 18.f, 160.f, 16.f), Justification::centred);
    g.setColour (catCol (3)); // green CPU indicator
    g.drawText ("CPU  --%", Rectangle<float> (W - 70.f, H - 18.f, 64.f, 16.f), Justification::centredLeft);
}

void LunaFXEditor::resized()
{
    content.setBounds (0, 0, kDesignW, kDesignH);
    content.setTransform (AffineTransform::scale ((float) getWidth() / (float) kDesignW));

    // ---- top-bar widgets
    presetPrev .setBounds (372, 9, 28, 32);
    presetLabel.setBounds (408, 9, 256, 32);
    presetNext .setBounds (672, 9, 28, 32);
    saveBtn    .setBounds (710, 11, 58, 28);
    folderBtn  .setBounds (774, 11, 78, 28);
    inSlider   .setBounds (862, 18, 96, 20);
    outSlider  .setBounds (978, 18, 96, 20);
    dryWet    ->setBounds (1086, 0, 58, kTopH);
    kill       .setBounds (1154, 10, 60, 30);
    bypass     .setBounds (1224, 10, 100, 30);

    // ---- left nav buttons
    const int navH = (kDesignH - kTopH - 20) / jmax (1, navBtns.size());
    for (int i = 0; i < navBtns.size(); ++i)
        navBtns[i]->setBounds (0, kTopH + i * navH, kNavW, navH);

    // ---- play area (left portion of page area)
    const Rectangle<int> padArea (kNavW, kTopH, kSplitX, kDesignH - kTopH - 20);
    play .setBounds (padArea);
    setup.setBounds (kNavW, kTopH, kDesignW - kNavW, kDesignH - kTopH - 20);

    // ---- right panel split: top = fxDetail, middle = fxCats, bottom = presets
    const int rw  = kDesignW - kRightX;
    const int rh  = kDesignH - kTopH - 20;
    const int detH = jmin (320, rh / 2);          // detail panel height
    const int catH = rh - detH - 100;              // category list height
    fxDetail.setBounds (kRightX, kTopH,           rw, detH);
    fxCats  .setBounds (kRightX, kTopH + detH,    rw, catH);
    presets .setBounds (kRightX, kTopH,            rw, rh);
}

void LunaFXEditor::timerCallback()
{
    if (play.isVisible()) play.tick (1.f / 30.f);
    fxCats.refresh();
    fxDetail.refresh();

    const int theme = (int)(proc.apvts.getRawParameterValue ("uiTheme")->load() + 0.5f);
    if (ui::gUiTheme.load() != theme)
    {
        ui::setUiTheme (theme);
        lnf.updateTheme();
        content.repaint(); repaint();
    }
    bypass.repaint();
    kill  .repaint();
}
