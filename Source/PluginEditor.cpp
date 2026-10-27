#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

using namespace juce;
using namespace ui;

// =============================================================================
//  Rack
// =============================================================================
RackPanel::RackPanel (LunaFXProcessor& p) : proc (p)
{
    addAndMakeVisible (cats);
    cats.onSelect = [this] (int tab) { categorySelected (tab); };

    for (int i = 0; i < params::kNumSlots; ++i)
    {
        rows[(size_t) i] = std::make_unique<SlotRow> (proc, i);
        rows[(size_t) i]->onSelect = [this] (int s) { if (onSelectSlot) onSelectSlot (s); };
        addAndMakeVisible (*rows[(size_t) i]);
    }

    inKnob  = std::make_unique<ui::KnobCard> (proc.apvts, "inGain",  "Input",   accent());
    outKnob = std::make_unique<ui::KnobCard> (proc.apvts, "outGain", "Output",  accent());
    dryKnob = std::make_unique<ui::KnobCard> (proc.apvts, "dryWet",  "Dry / Wet", accentSoft());
    addAndMakeVisible (*inKnob);
    addAndMakeVisible (*outKnob);
    addAndMakeVisible (*dryKnob);
}

void RackPanel::categorySelected (int tab)
{
    static const int actual[5] = { 0, 1, 2, 3, 5 };
    const int cat = actual[jlimit (0, 4, tab)];
    cats.setActive (tab);
    for (int i = 0; i < params::kNumSlots; ++i)
    {
        const int fx = proc.slotFx (i);
        if (params::isVisibleFx (fx) && djx::kFxCategory[fx] == cat)
        {
            if (onSelectSlot) onSelectSlot (i);
            return;
        }
    }
    if (onSelectSlot) onSelectSlot (selected >= 0 ? selected : 0);
}

bool RackPanel::rawEnabled (int slot) const
{
    if (auto* p = proc.apvts.getRawParameterValue (params::slotEnableId (slot)))
        return p->load() > 0.5f;
    return false;
}

void RackPanel::refresh()
{
    int theme = ui::gTheme.load();
    if (theme != lastTheme)
    {
        lastTheme = theme;
        inKnob->setAccent (accent());
        outKnob->setAccent (accent());
        dryKnob->setAccent (accentSoft());
    }
    inKnob->tick(); outKnob->tick(); dryKnob->tick();

    bool used[5] { false, false, false, false, false };
    for (int i = 0; i < params::kNumSlots; ++i)
    {
        rows[(size_t) i]->refresh();
        const int fx = proc.slotFx (i);
        if (params::isVisibleFx (fx) && rawEnabled (i))
        {
            const int cat = djx::kFxCategory[fx];
            if (cat == 0) used[0] = true;
            else if (cat == 1) used[1] = true;
            else if (cat == 2) used[2] = true;
            else if (cat == 3) used[3] = true;
            else if (cat == 5) used[4] = true;
        }
    }
    for (int i = 0; i < 5; ++i) cats.setUsed (i, used[i]);

    // Keep the category strip in sync when automation or a preset changes the
    // selected slot's instrument.
    if (selected >= 0)
    {
        const int fx = proc.slotFx (selected);
        if (params::isVisibleFx (fx))
        {
            static const int tabForCat[6] = { 0, 1, 2, 3, -1, 4 };
            const int tab = tabForCat[jlimit (0, 5, djx::kFxCategory[fx])];
            if (tab >= 0) cats.setActive (tab);
        }
    }
}

void RackPanel::tick (float dt)
{
    for (auto& r : rows) r->tickFlash (dt);
}

void RackPanel::flashPreset()
{
    for (int i = 0; i < params::kNumSlots; ++i)
        if (params::isVisibleFx (proc.slotFx (i)) && rawEnabled (i))
            rows[(size_t) i]->flashPreset();
}

void RackPanel::setSelectedSlot (int slot)
{
    selected = slot;
    for (int i = 0; i < params::kNumSlots; ++i) rows[(size_t) i]->setSelected (i == slot);
    const int fx = slot >= 0 ? proc.slotFx (slot) : -1;
    if (params::isVisibleFx (fx))
    {
        static const int tabForCat[6] = { 0, 1, 2, 3, -1, 4 };
        const int tab = tabForCat[jlimit (0, 5, djx::kFxCategory[fx])];
        if (tab >= 0) cats.setActive (tab);
    }
    repaint();
}

void RackPanel::resized()
{
    cats.setBounds (6, 6, getWidth() - 12, 32);
    for (int i = 0; i < params::kNumSlots; ++i)
        rows[(size_t) i]->setBounds (6, 70 + i * 90, getWidth() - 12, 88);

    inKnob->setBounds (14, 820, 112, 140);
    outKnob->setBounds (140, 820, 112, 140);
    dryKnob->setBounds (266, 820, 112, 140);
}

void RackPanel::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.f);
    g.setColour (panel().withAlpha (isDay() ? 0.82f : 0.72f));
    g.fillRoundedRectangle (b, 10.f);
    g.setColour (line());
    g.drawRoundedRectangle (b, 10.f, 1.f);

    g.setColour (text());
    g.setFont (Font (14.f).boldened());
    g.drawText ("INSTRUMENT RACK", 12, 40, 230, 22, Justification::centredLeft, false);
    g.setColour (muted());
    g.setFont (Font (9.f).boldened());
    g.drawText ("MIX PER TOOL  ·  LIVE FROM XY", 120, 40, getWidth() - 132, 22,
                Justification::centredRight, false);

    // Master section. The old non-interactive signal-flow menu was removed and
    // the space is now used by the eight rows and these larger master knobs.
    g.setColour (text());
    g.setFont (Font (13.f).boldened());
    g.drawText ("MASTER", 14, 800, 200, 20, Justification::centredLeft, false);
    g.setColour (muted());
    g.setFont (Font (9.f).boldened());
    g.drawText ("GLOBAL", 14, 800, getWidth() - 28, 20, Justification::centredRight, false);
}

// =============================================================================
//  Context
// =============================================================================
ContextPanel::ContextPanel (LunaFXProcessor& p) : proc (p) {}

void ContextPanel::showSlot (int s)
{
    if (s < -1 || s >= params::kNumSlots) s = -1;
    const int newFx = s >= 0 ? proc.slotFx (s) : -2;
    if (s == slot && newFx == fx) return;
    slot = s;
    fx = newFx;
    rebuild();
}

void ContextPanel::rebuild()
{
    cards.clear();
    if (slot >= 0 && params::isVisibleFx (fx))
    {
        const Colour c = fxColour (fx);
        cards.push_back (std::make_unique<ui::KnobCard> (proc.apvts, params::slotLevelId (slot), "Output", c));
        for (const auto& id : params::fxInfo()[(size_t) fx].details)
        {
            const auto* pd = params::find (id);
            if (pd == nullptr) continue;
            cards.push_back (std::make_unique<ui::KnobCard> (proc.apvts, id, pd->name.c_str(), c));
        }
    }
    for (auto& c : cards) addAndMakeVisible (*c);
    resized();
    repaint();
}

void ContextPanel::refresh()
{
    const int newFx = slot >= 0 ? proc.slotFx (slot) : -2;
    if (newFx != fx) { fx = newFx; rebuild(); return; }
    for (auto& c : cards) c->tick();
    const float live = slot >= 0 ? proc.slotLiveMix (slot) : 0.f;
    const auto* mxParam = (slot >= 0 && params::isVisibleFx (fx))
                          ? proc.apvts.getRawParameterValue (params::amtId (fx)) : nullptr;
    const float mx = mxParam != nullptr ? mxParam->load() : 0.f;
    const auto* en = slot >= 0 ? proc.apvts.getRawParameterValue (params::slotEnableId (slot)) : nullptr;
    const bool enabled = en == nullptr || en->load() > 0.5f;
    if (std::abs (live - lastLive) > 0.002f || std::abs (mx - lastMax) > 0.002f
        || enabled != lastEnabled)
    {
        lastLive = live;
        lastMax = mx;
        lastEnabled = enabled;
        repaint();
    }
}

void ContextPanel::tick()
{
    for (auto& c : cards) c->tick();
}

void ContextPanel::resized()
{
    if (getWidth() <= 0 || getHeight() <= 0) return;
    auto area = getLocalBounds().reduced (8);
    area.removeFromTop (70);
    const int cols = 3, gap = 6;
    const int n = (int) cards.size();
    if (n == 0) return;
    const int rowsNeeded = (n + cols - 1) / cols;
    const int cellW = (area.getWidth() - (cols - 1) * gap) / cols;
    const int cellH = jlimit (118, 205, (area.getHeight() - (rowsNeeded - 1) * gap) / jmax (1, rowsNeeded));
    for (int i = 0; i < n; ++i)
    {
        const int r = i / cols, c = i % cols;
        cards[(size_t) i]->setBounds (area.getX() + c * (cellW + gap),
                                      area.getY() + r * (cellH + gap), cellW, cellH);
    }
}

void ContextPanel::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.f);
    g.setColour (panel().withAlpha (isDay() ? 0.82f : 0.72f));
    g.fillRoundedRectangle (b, 9.f);
    g.setColour (line());
    g.drawRoundedRectangle (b, 9.f, 1.f);

    if (slot < 0)
    {
        g.setColour (muted());
        g.setFont (Font (12.f));
        g.drawFittedText ("Select an instrument slot from the rack or circle.",
                          getLocalBounds().reduced (26), Justification::centred, 2);
        return;
    }

    const bool assigned = params::isVisibleFx (fx);
    const Colour c = assigned ? fxColour (fx) : lineBright();
    const auto* enableParam = proc.apvts.getRawParameterValue (params::slotEnableId (slot));
    const bool enabled = enableParam == nullptr || enableParam->load() > 0.5f;
    g.setColour (c.withAlpha (0.09f));
    g.fillRoundedRectangle (8.f, 8.f, (float) getWidth() - 16.f, 60.f, 6.f);

    g.setColour (muted());
    g.setFont (Font (9.5f).boldened());
    g.drawText ("SELECTED SLOT " + String (slot + 1), 16, 11, 240, 14, Justification::centredLeft, false);
    g.setColour (assigned && enabled ? text() : muted());
    g.setFont (Font (19.f).boldened());
    g.drawText (assigned ? String (djx::kFxNames[fx]) : String ("Unassigned"),
                16, 25, getWidth() - 32, 26, Justification::centredLeft, false);
    g.setColour (assigned ? c : muted());
    g.setFont (Font (9.5f).boldened());
    g.drawText (assigned ? (enabled ? categoryLabel (djx::kFxCategory[fx]).toUpperCase()
                                    : String ("DISABLED"))
                         : String ("CHOOSE CHANGE IN THE RACK"),
                getWidth() - 190, 11, 176, 14, Justification::centredRight, false);

    if (! assigned)
    {
        g.setColour (muted());
        g.setFont (Font (12.f));
        g.drawFittedText ("This slot is empty. Use CHANGE in the rack to assign one of the instruments. Every visible knob is a real parameter.",
                          getLocalBounds().reduced (24).withTrimmedTop (90), Justification::topLeft, 4);
        return;
    }

    const float live = proc.slotLiveMix (slot);
    const auto* amtParam = proc.apvts.getRawParameterValue (params::amtId (fx));
    const float maxMix = amtParam != nullptr ? amtParam->load() : 0.f;
    auto meter = Rectangle<float> (16.f, 48.f, (float) getWidth() - 32.f, 6.f);    g.setColour (line());
    g.fillRoundedRectangle (meter, 3.f);
    g.setColour (c.withAlpha (0.9f));
    g.fillRoundedRectangle (meter.withWidth (jmax (2.f, meter.getWidth() * jlimit (0.f, 1.f, live))), 3.f);
    g.setColour (text().withAlpha (0.5f));
    g.fillRect (meter.getX() + meter.getWidth() * jlimit (0.f, 1.f, maxMix) - 0.5f,
                meter.getY() - 1.f, 1.f, meter.getHeight() + 2.f);
    g.setColour (muted());
    g.setFont (Font (9.f).boldened());
    g.drawText ("LIVE " + String (roundToInt (live * 100.f)) + "%  ·  MIX "
                + String (roundToInt (maxMix * 100.f)) + "%",
                (int) meter.getX(), 58, (int) meter.getWidth(), 12,
                Justification::centredRight, false);
}

// =============================================================================
//  Presets
// =============================================================================
PresetOverlay::PresetOverlay (LunaFXProcessor& p) : proc (p)
{
    model.owner = this;
    categories.push_back ("All");
    for (auto& pr : params::presets())
    {
        const String cat (pr.category.c_str());
        if (std::find (categories.begin(), categories.end(), cat) == categories.end())
            categories.push_back (cat);
    }
    for (auto& c : categories)
    {
        auto* b = new TextButton (c);
        b->setColour (TextButton::buttonColourId, Colours::transparentBlack);
        b->setColour (TextButton::textColourOffId, muted());
        b->addListener (this);
        catButtons.add (b);
        addAndMakeVisible (b);
    }

    for (auto* b : { &factoryBtn, &userBtn, &closeBtn, &saveBtn })
    {
        b->addListener (this);
        b->setColour (TextButton::buttonColourId, Colours::transparentBlack);
        b->setColour (TextButton::textColourOffId, muted());
        addAndMakeVisible (*b);
    }
    addAndMakeVisible (list);
    list.setModel (&model);
    list.setRowHeight (38);
    list.setColour (ListBox::backgroundColourId, Colours::transparentBlack);
    list.setColour (ListBox::outlineColourId, line());
    refresh();
}

juce::File PresetOverlay::userDir() const
{
    return File::getSpecialLocation (File::userApplicationDataDirectory)
        .getChildFile ("LunaLabs").getChildFile ("Luna FX").getChildFile ("Presets");
}

std::vector<juce::File> PresetOverlay::userFiles() const
{
    std::vector<File> out;
    Array<File> found;
    const auto dir = userDir();
    if (dir.isDirectory()) dir.findChildFiles (found, File::findFiles, false, "*.lfxpreset");
    for (int i = 0; i < found.size(); ++i) out.push_back (found[i]);
    std::sort (out.begin(), out.end(), [] (const File& a, const File& b)
    {
        return a.getFileName().compareIgnoreCase (b.getFileName()) < 0;
    });
    return out;
}

void PresetOverlay::setFolder (int f)
{
    folder = f;
    rebuild();
}

void PresetOverlay::rebuild()
{
    cachedUserFiles = userFiles();
    rows.clear();
    if (folder == 0)
    {
        const auto& ps = params::presets();
        for (int i = 0; i < (int) ps.size(); ++i)
        {
            const String cat (ps[(size_t) i].category.c_str());
            if (activeCategory != "All" && cat != activeCategory) continue;
            rows.push_back ({ i, -1 });
        }
    }
    else
    {
        for (int i = 0; i < (int) cachedUserFiles.size(); ++i) rows.push_back ({ -1, i });
    }
    list.updateContent();
    if (folder == 0)
    {
        for (int r = 0; r < (int) rows.size(); ++r)
            if (rows[(size_t) r].program == proc.getCurrentProgram()) { list.selectRow (r, true); break; }
    }
    list.repaint();
    repaint();
}

void PresetOverlay::refresh()
{
    rebuild();
}

void PresetOverlay::loadRow (int row)
{
    if (row < 0 || row >= (int) rows.size()) return;
    const auto r = rows[(size_t) row];
    if (r.program >= 0)
    {
        proc.setCurrentProgram (r.program);
        proc.uiPresetRevision.fetch_add (1, std::memory_order_relaxed);
    }
    else if (r.file >= 0 && r.file < (int) cachedUserFiles.size())
    {
        MemoryBlock mb;
        if (cachedUserFiles[(size_t) r.file].loadFileAsData (mb) && mb.getSize() > 0)
            proc.setStateInformation (mb.getData(), (int) mb.getSize());
        proc.uiPresetRevision.fetch_add (1, std::memory_order_relaxed);
    }
    rebuild();
}

void PresetOverlay::buttonClicked (Button* b)
{
    if (b == &closeBtn) { if (onClose) onClose(); return; }
    if (b == &saveBtn) { openSave(); return; }
    if (b == &factoryBtn) { folder = 0; rebuild(); return; }
    if (b == &userBtn) { folder = 1; rebuild(); return; }
    for (int i = 0; i < catButtons.size(); ++i)
        if (catButtons[i] == b)
        {
            activeCategory = categories[(size_t) i];
            rebuild();
            return;
        }
}

void PresetOverlay::openSave()
{
    if (saveEditor != nullptr)
    {
        saveEditor->grabKeyboardFocus();
        return;
    }
    saveEditor = std::make_unique<TextEditor>();
    saveEditor->setText (proc.getProgramName (proc.getCurrentProgram()));
    saveEditor->setSelectAllWhenFocused (true);
    saveEditor->setBounds (16, 54, jmax (220, getWidth() - 32), 28);
    saveEditor->setColour (TextEditor::backgroundColourId, panelRaised());
    addAndMakeVisible (*saveEditor);
    saveEditor->toFront (true);
    saveEditor->grabKeyboardFocus();

    Component::SafePointer<PresetOverlay> sp (this);
    saveEditor->onReturnKey = [sp]
    {
        if (sp == nullptr || sp->saveEditor == nullptr) return;
        const String name = sp->saveEditor->getText();
        sp->saveWithName (name);
        if (sp->saveEditor != nullptr)
        {
            auto* dead = sp->saveEditor.release();
            sp->removeChildComponent (dead);
            Component::SafePointer<PresetOverlay> keep (sp.getComponent());
            MessageManager::callAsync ([keep, dead]
            {
                if (keep != nullptr && keep->saveEditor.get() == dead) keep->saveEditor.reset();
                else delete dead;
            });
        }
    };
    saveEditor->onEscapeKey = [sp]
    {
        if (sp == nullptr || sp->saveEditor == nullptr) return;
        auto* dead = sp->saveEditor.release();
        sp->removeChildComponent (dead);
        Component::SafePointer<PresetOverlay> keep (sp.getComponent());
        MessageManager::callAsync ([keep, dead]
        {
            if (keep != nullptr && keep->saveEditor.get() == dead) keep->saveEditor.reset();
            else delete dead;
        });
    };
}

void PresetOverlay::saveWithName (const String& rawName)
{
    String name = rawName.trim();
    name = name.replaceCharacters ("\\/:*?\"<>|", "_________").trim();
    if (name.isEmpty()) return;
    auto dir = userDir();
    const auto created = dir.createDirectory();
    if (! created.wasOk() && ! dir.isDirectory()) return;

    const String previous = proc.getProgramName (proc.getCurrentProgram());
    proc.setUserPresetName (name);
    MemoryBlock state;
    proc.getStateInformation (state);
    const auto target = dir.getChildFile (name + ".lfxpreset");
    if (! target.replaceWithData (state.getData(), state.getSize()))
    {
        proc.setUserPresetName (previous);
        return;
    }
    proc.uiPresetRevision.fetch_add (1, std::memory_order_relaxed);
    folder = 1;
    rebuild();
}

void PresetOverlay::paint (Graphics& g)
{
    g.fillAll (bgTop().withAlpha (0.985f));
    g.setColour (line());
    g.drawLine (0.f, 51.f, (float) getWidth(), 51.f, 1.f);

    g.setColour (text());
    g.setFont (Font (24.f).boldened());
    g.drawText ("PRESET LIBRARY", 16, 8, 380, 36, Justification::centredLeft, false);
    g.setColour (muted());
    g.setFont (Font (10.f).boldened());
    const String subtitle = folder == 0 ? String ("FACTORY PROGRAMMES")
                                        : String ("USER PRESETS · ") + String ((int) cachedUserFiles.size());
    g.drawText (subtitle, 178, 20, 300, 18, Justification::centredLeft, false);

    for (int i = 0; i < catButtons.size(); ++i)
    {
        const bool on = categories[(size_t) i] == activeCategory;
        auto r = catButtons[i]->getBounds().toFloat().reduced (1.f);
        if (on)
        {
            g.setColour (accent().withAlpha (0.16f));
            g.fillRoundedRectangle (r, 4.f);
            g.setColour (accent());
            g.drawRoundedRectangle (r, 4.f, 1.f);
        }
        catButtons[i]->setColour (TextButton::textColourOffId, on ? text() : muted());
    }
    for (int i = 0; i < 2; ++i)
    {
        auto* b = i == 0 ? &factoryBtn : &userBtn;
        const bool on = (i == folder);
        auto r = b->getBounds().toFloat().reduced (1.f);
        if (on)
        {
            g.setColour (accent().withAlpha (0.16f));
            g.fillRoundedRectangle (r, 4.f);
            g.setColour (accent());
            g.drawRoundedRectangle (r, 4.f, 1.f);
        }
        b->setColour (TextButton::textColourOffId, on ? text() : muted());
    }
    auto sr = saveBtn.getBounds().toFloat().reduced (1.f);
    g.setColour (accent().withAlpha (0.15f));
    g.fillRoundedRectangle (sr, 5.f);
    g.setColour (accent());
    g.drawRoundedRectangle (sr, 5.f, 1.f);

    g.setColour (muted().withAlpha (0.8f));
    g.setFont (Font (9.f));
    g.drawFittedText ("Presets restore slot assignments, enable states, maximum mixes, output levels and all effect parameters. "
                      "The host receives parameter notifications through the normal APVTS path.",
                      Rectangle<int> (280, getHeight() - 40, getWidth() - 300, 30),
                      Justification::centredLeft, 2, 0.8f);
}

void PresetOverlay::resized()
{
    saveBtn.setBounds (getWidth() - 130, 12, 56, 28);
    closeBtn.setBounds (getWidth() - 68, 12, 54, 28);
    factoryBtn.setBounds (16, getHeight() - 78, 140, 26);
    userBtn.setBounds (162, getHeight() - 78, 140, 26);

    int y = 70;
    for (int i = 0; i < catButtons.size(); ++i)
    {
        catButtons[i]->setBounds (16, y, 244, 28);
        y += 31;
    }
    list.setBounds (300, 70, getWidth() - 320, getHeight() - 130);
}

int PresetOverlay::Model::getNumRows() { return owner != nullptr ? (int) owner->rows.size() : 0; }

void PresetOverlay::Model::paintListBoxItem (int row, Graphics& g, int w, int h, bool selected)
{
    if (owner == nullptr || row < 0 || row >= (int) owner->rows.size()) return;
    const auto r = owner->rows[(size_t) row];
    if (selected)
    {
        g.setColour (accent().withAlpha (0.14f));
        g.fillRoundedRectangle (2.f, 2.f, (float) w - 4.f, (float) h - 4.f, 4.f);
    }
    String name, cat;
    if (r.program >= 0)
    {
        const auto& ps = params::presets();
        if (r.program < (int) ps.size())
        {
            name = ps[(size_t) r.program].name.c_str();
            cat = ps[(size_t) r.program].category.c_str();
        }
    }
    else if (r.file >= 0 && r.file < (int) owner->cachedUserFiles.size())
    {
        name = owner->cachedUserFiles[(size_t) r.file].getFileNameWithoutExtension();
        cat = "USER";
    }
    g.setColour (selected ? text() : text().withAlpha (0.86f));
    g.setFont (Font (14.5f));
    g.drawText (name, 14, 0, w - 160, h, Justification::centredLeft, false);
    g.setColour (muted());
    g.setFont (Font (10.f));
    g.drawText (cat, w - 150, 0, 136, h, Justification::centredRight, false);
}

void PresetOverlay::Model::listBoxItemClicked (int row, const MouseEvent&)
{
    if (owner != nullptr) owner->loadRow (row);
}

// =============================================================================
//  Editor
// =============================================================================
LunaFXEditor::LunaFXEditor (LunaFXProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      content (*this), circle (p), rack (p), context (p), presets (p),
      accentButton (p), kill (p), bypass (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    content.addAndMakeVisible (circle);
    content.addAndMakeVisible (rack);
    content.addAndMakeVisible (context);
    content.addAndMakeVisible (accentButton);
    content.addAndMakeVisible (kill);
    content.addAndMakeVisible (bypass);

    content.addAndMakeVisible (presetPrev);
    content.addAndMakeVisible (presetNext);
    content.addAndMakeVisible (presetName);
    content.addAndMakeVisible (saveBtn);
    content.addAndMakeVisible (browseBtn);
    content.addAndMakeVisible (themeBtn);

    depthKnob = std::make_unique<ui::KnobCard> (proc.apvts, "padDepth", "XY Depth", accent(), true);
    fadeKnob  = std::make_unique<ui::KnobCard> (proc.apvts, "fade", "Fade", accentSoft(), true);
    chaosKnob = std::make_unique<ui::KnobCard> (proc.apvts, "chaos", "Chaos", accentSoft(), true);
    content.addAndMakeVisible (*depthKnob);
    content.addAndMakeVisible (*fadeKnob);
    content.addAndMakeVisible (*chaosKnob);

    touchToggle  = std::make_unique<ui::ParamToggle> (proc.apvts, "padTouch", "TOUCH = ACCENT");
    springToggle = std::make_unique<ui::ParamToggle> (proc.apvts, "padSpring", "SPRING RETURN");
    content.addAndMakeVisible (*touchToggle);
    content.addAndMakeVisible (*springToggle);

    for (auto* b : { &presetPrev, &presetNext, &presetName, &saveBtn, &browseBtn, &themeBtn })
    {
        b->setColour (TextButton::buttonColourId, Colours::transparentBlack);
        b->setColour (TextButton::textColourOffId, text());
    }

    presetPrev.onClick = [this]
    {
        const int n = proc.getNumPrograms();
        proc.setCurrentProgram ((proc.getCurrentProgram() + n - 1) % n);
        presets.refresh();
    };
    presetNext.onClick = [this]
    {
        proc.setCurrentProgram ((proc.getCurrentProgram() + 1) % proc.getNumPrograms());
        presets.refresh();
    };
    presetName.onClick = [this] { openPresetBrowser (! presetPanelVisible); };
    browseBtn.onClick  = [this] { openPresetBrowser (! presetPanelVisible); };
    saveBtn.onClick    = [this] { launchSave(); };
    themeBtn.onClick   = [this]
    {
        if (auto* t = proc.apvts.getParameter ("uiTheme"))
            t->setValueNotifyingHost (t->getValue() > 0.5f ? 0.f : 1.f);
    };

    circle.onSelectSlot = [this] (int s) { setSelectedSlot (s); };
    rack.onSelectSlot   = [this] (int s) { setSelectedSlot (s); };
    presets.onClose     = [this] { openPresetBrowser (false); };

    content.addAndMakeVisible (presets);
    presets.setVisible (false);

    lastTheme = ui::gTheme.load();
    lastPresetRevision = proc.uiPresetRevision.load (std::memory_order_relaxed);
    setSelectedSlot (0);
    updatePresetName();
    themeBtn.setButtonText (ui::isDay() ? "NIGHT" : "DAY");

    setResizable (true, true);
    setResizeLimits (1120, 700, 2400, 1500);
    getConstrainer()->setFixedAspectRatio ((double) kDesignW / (double) kDesignH);
    setSize (kDesignW, kDesignH);
    startTimerHz (30);
}

LunaFXEditor::~LunaFXEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void LunaFXEditor::paint (Graphics& g)
{
    g.setGradientFill (ColourGradient (bgTop(), 0.f, 0.f, bgBottom(), 0.f, (float) getHeight(), false));
    g.fillAll();
}

void LunaFXEditor::paintContent (Graphics& g)
{
    const float W = (float) kDesignW, H = (float) kDesignH;
    g.setGradientFill (ColourGradient (bgTop(), 0.f, 0.f, bgBottom(), 0.f, H, false));
    g.fillAll();

    // left lunar field glow behind the octagon
    g.setGradientFill (ColourGradient (accent().withAlpha (isDay() ? 0.06f : 0.075f), 400.f, 470.f,
                                       Colours::transparentBlack, 400.f, 900.f, true));
    g.fillRect (0.f, (float) kTopH, (float) kLeftW, H - (float) kTopH);

    // top bar
    g.setColour (bgTop());
    g.fillRect (0.f, 0.f, W, (float) kTopH);
    g.setColour (line());
    g.drawLine (0.f, (float) kTopH, W, (float) kTopH, 1.f);

    g.setColour (text());
    g.setFont (Font (28.f).boldened());
    g.drawText ("LUNA", 18, 12, 92, 34, Justification::centredLeft, false);
    g.setColour (accent());
    g.setFont (Font (19.f).boldened());
    g.drawText ("FX", 108, 17, 38, 26, Justification::centredLeft, false);

    auto badge = Rectangle<float> (154.f, 18.f, 52.f, 22.f);
    g.setColour (panelRaised());
    g.fillRoundedRectangle (badge, 4.f);
    g.setColour (line());
    g.drawRoundedRectangle (badge, 4.f, 1.f);
    g.setColour (muted());
    g.setFont (Font (9.f).boldened());
    g.drawText ("1.3.0", badge, Justification::centred);

    g.setColour (muted().withAlpha (0.85f));
    g.setFont (Font (9.f).boldened());
    g.drawText ("8 VERTEX OCTAGONAL FIELD", 884, 20, 190, 14, Justification::centredLeft, false);
    g.drawText ("PRESET", 288, 3, 100, 12, Justification::centredLeft, false);

    // left performance strip
    g.setColour (line());
    g.drawLine (16.f, 866.f, (float) kLeftW - 16.f, 866.f, 1.f);
    g.setColour (muted());
    g.setFont (Font (9.f).boldened());
    g.drawText ("PERFORMANCE", 20, 870, 150, 14, Justification::centredLeft, false);
    g.drawText ("XY DEPTH · FADE · CHAOS · ACCENT", 172, 870, 340, 14, Justification::centredLeft, false);

    // right panel labels
    g.setColour (muted());
    g.setFont (Font (9.f).boldened());
    g.drawText ("INSTRUMENTS", Rectangle<float> ((float) kRackX + 4.f, 50.f, 130.f, 14.f),
                Justification::centredLeft, false);
    g.drawText ("EFFECT PARAMETERS", Rectangle<float> ((float) kCtxX + 4.f, 50.f, 190.f, 14.f),
                Justification::centredLeft, false);
    g.drawText ("SLOT 1 - 8", Rectangle<float> ((float) kRackX + 250.f, 50.f, 130.f, 14.f),
                Justification::centredRight, false);

    // status
    g.setColour (line());
    g.drawLine (0.f, 984.f, W, 984.f, 1.f);
    g.setColour (muted().withAlpha (0.9f));
    g.setFont (Font (9.f).boldened());
    g.drawText ("LUNA FX  " + String (params::kVersion) + "  ·  8 SLOTS  ·  OCTAGONAL DISTANCE MIX",
                16, 985, 480, 14, Justification::centredLeft, false);
    g.drawText ("EVERY KNOB IS AN APVTS PARAMETER", 500, 985, 380, 14, Justification::centredLeft, false);
    g.setColour (silver().withAlpha (0.85f));
    g.drawText ("SILVER RING = PRESET  ·  COLOUR ZONES = LIVE FIELD",
                Rectangle<float> (W - 470.f, 985.f, 454.f, 14.f), Justification::centredRight, false);
}

void LunaFXEditor::resized()
{
    content.setBounds (0, 0, kDesignW, kDesignH);
    content.setTransform (AffineTransform::scale ((float) getWidth() / (float) kDesignW));

    circle.setBounds (0, 70, kLeftW, 780);
    rack.setBounds (kRackX, 70, kRackW, 914);
    context.setBounds (kCtxX, 70, kCtxW, 914);

    accentButton.setBounds (22, 884, 112, 88);
    depthKnob->setBounds (170, 878, 110, 104);
    fadeKnob ->setBounds (292, 878, 110, 104);
    chaosKnob->setBounds (414, 878, 110, 104);
    touchToggle ->setBounds (548, 890, 180, 26);
    springToggle->setBounds (548, 924, 180, 26);

    presetPrev.setBounds (250, 18, 32, 32);
    presetName.setBounds (288, 16, 340, 34);
    presetNext.setBounds (636, 18, 32, 32);
    saveBtn.setBounds (682, 18, 66, 30);
    browseBtn.setBounds (756, 18, 96, 30);
    themeBtn.setBounds (1080, 18, 96, 30);

    kill.setBounds (1270, 16, 86, 32);
    bypass.setBounds (1364, 16, 150, 32);

    presets.setBounds (0, kTopH, kDesignW, kDesignH - kTopH);
}

void LunaFXEditor::setSelectedSlot (int slot)
{
    slot = jlimit (0, params::kNumSlots - 1, slot);
    selectedSlot = slot;
    proc.uiSelectedSlot.store (slot, std::memory_order_relaxed);
    rack.setSelectedSlot (slot);
    circle.setSelectedSlot (slot);
    context.showSlot (slot);
}

void LunaFXEditor::openPresetBrowser (bool show)
{
    presetPanelVisible = show;
    if (show)
    {
        presets.setBounds (0, kTopH, kDesignW, kDesignH - kTopH);
        presets.refresh();
        presets.setVisible (true);
        presets.toFront (true);
        browseBtn.setButtonText ("CLOSE");
    }
    else
    {
        presets.setVisible (false);
        browseBtn.setButtonText ("PRESETS");
        context.refresh();
        rack.refresh();
    }
    repaint();
}

void LunaFXEditor::launchSave()
{
    openPresetBrowser (true);
    presets.openSave();
}

void LunaFXEditor::updatePresetName()
{
    const String name = proc.getProgramName (proc.getCurrentProgram());
    const String text = name.isNotEmpty() ? name : String ("Init");
    if (text != lastPresetName)
    {
        lastPresetName = text;
        presetName.setButtonText (text);
        presetName.setTooltip ("Preset: " + text + " - click to open the library");
    }
}

void LunaFXEditor::timerCallback()
{
    const auto* themeParam = proc.apvts.getRawParameterValue ("uiTheme");
    const int theme = themeParam != nullptr ? (int) (themeParam->load() + 0.5f) : 0;
    if (theme != lastTheme)
    {
        lastTheme = theme;
        ui::setTheme (theme);
        lnf.updateTheme();
        themeBtn.setButtonText (ui::isDay() ? "NIGHT" : "DAY");
        for (auto* b : { &presetPrev, &presetNext, &presetName, &saveBtn, &browseBtn, &themeBtn })
            b->setColour (TextButton::textColourOffId, text());
        depthKnob->setAccent (accent());
        fadeKnob->setAccent (accentSoft());
        chaosKnob->setAccent (accentSoft());
        touchToggle->repaint();
        springToggle->repaint();
        context.showSlot (-1);
        context.showSlot (selectedSlot);
        circle.repaint();
        rack.repaint();
        context.repaint();
        presets.repaint();
        content.repaint();
        repaint();
    }

    const unsigned int rev = proc.uiPresetRevision.load (std::memory_order_relaxed);
    if (rev != lastPresetRevision)
    {
        lastPresetRevision = rev;
        rack.flashPreset();
        circle.flashPreset();
        context.refresh();
        updatePresetName();
        presets.refresh();
        repaint();
    }

    const int wanted = proc.uiSelectedSlot.load (std::memory_order_relaxed);
    if (wanted >= 0 && wanted != selectedSlot)
    {
        selectedSlot = wanted;
        rack.setSelectedSlot (wanted);
        circle.setSelectedSlot (wanted);
        context.showSlot (wanted);
    }

    const float dt = 1.f / 30.f;
    circle.tick (dt);
    rack.tick (dt);
    rack.refresh();
    context.refresh();
    depthKnob->tick();
    fadeKnob->tick();
    chaosKnob->tick();
    updatePresetName();

    accentButton.repaint();
    kill.repaint();
    bypass.repaint();
}
