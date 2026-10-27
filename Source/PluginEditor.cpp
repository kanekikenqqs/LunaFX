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
    cats.setBounds (4, 4, getWidth() - 8, 30);
    for (int i = 0; i < params::kNumSlots; ++i)
        rows[(size_t) i]->setBounds (6, 54 + i * 58, getWidth() - 12, 56);

    const int kw = 100, kh = 108;
    inKnob->setBounds (14, 700, kw, kh);
    outKnob->setBounds (124, 700, kw, kh);
    dryKnob->setBounds (234, 700, kw, kh);
}

void RackPanel::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.f);
    g.setColour (panel().withAlpha (isDay() ? 0.82f : 0.72f));
    g.fillRoundedRectangle (b, 9.f);
    g.setColour (line());
    g.drawRoundedRectangle (b, 9.f, 1.f);

    g.setColour (text());
    g.setFont (Font (12.f).boldened());
    g.drawText ("INSTRUMENT RACK", 10, 36, 200, 18, Justification::centredLeft, false);
    g.setColour (muted());
    g.setFont (Font (8.f).boldened());
    g.drawText ("MAX = CEILING   LIVE = XY", 10, 36, getWidth() - 20, 18,
                Justification::centredRight, false);

    // signal flow
    g.setColour (muted());
    g.setFont (Font (8.f).boldened());
    g.drawText ("SIGNAL FLOW", 14, 532, 140, 14, Justification::centredLeft, false);
    const char* names[5] = { "PITCH", "FX", "GLITCH", "MOD", "TIME" };
    static const int catsIdx[5] = { 0, 1, 2, 3, 5 };
    int y = 550;
    for (int i = 0; i < 5; ++i)
    {
        auto chip = Rectangle<float> (14.f, (float) y, 92.f, 18.f);
        const Colour c = params::catColour (catsIdx[i]);
        g.setColour (c.withAlpha (0.10f));
        g.fillRoundedRectangle (chip, 4.f);
        g.setColour (c.withAlpha (0.55f));
        g.drawRoundedRectangle (chip, 4.f, 1.f);
        g.setColour (muted());
        g.setFont (Font (8.f).boldened());
        g.drawText (names[i], chip, Justification::centred);
        if (i < 4)
        {
            g.setColour (line());
            g.drawLine (110.f, (float) y + 9.f, 128.f, (float) y + 9.f, 1.f);
        }
        y += 22;
    }
    g.setColour (muted().withAlpha (0.8f));
    g.setFont (Font (7.5f));
    g.drawFittedText ("Serial. Slots sum into their category; proximity sets live wet.",
                      Rectangle<int> (14, 662, getWidth() - 28, 14), Justification::topLeft, 1, 0.75f);

    g.setColour (text());
    g.setFont (Font (10.f).boldened());
    g.drawText ("MASTER", 14, 680, 200, 16, Justification::centredLeft, false);
    g.setColour (muted());
    g.setFont (Font (8.f).boldened());
    g.drawText ("GLOBAL", 14, 680, getWidth() - 28, 16, Justification::centredRight, false);
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
        cards.push_back (std::make_unique<ui::KnobCard> (proc.apvts, params::amtId (fx), "Max Mix", c));
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
    area.removeFromTop (54);
    const int cols = 3, gap = 5;
    const int n = (int) cards.size();
    if (n == 0) return;
    const int rowsNeeded = (n + cols - 1) / cols;
    const int cellW = (area.getWidth() - (cols - 1) * gap) / cols;
    const int cellH = jlimit (100, 172, (area.getHeight() - (rowsNeeded - 1) * gap) / jmax (1, rowsNeeded));
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
    g.fillRoundedRectangle (8.f, 8.f, (float) getWidth() - 16.f, 42.f, 6.f);

    g.setColour (muted());
    g.setFont (Font (8.f).boldened());
    g.drawText ("SELECTED SLOT " + String (slot + 1), 16, 12, 220, 12, Justification::centredLeft, false);
    g.setColour (assigned && enabled ? text() : muted());
    g.setFont (Font (16.f).boldened());
    g.drawText (assigned ? String (djx::kFxNames[fx]) : String ("Unassigned"),
                16, 24, getWidth() - 32, 22, Justification::centredLeft, false);
    g.setColour (assigned ? c : muted());
    g.setFont (Font (8.f).boldened());
    g.drawText (assigned ? (enabled ? categoryLabel (djx::kFxCategory[fx]).toUpperCase()
                                    : String ("DISABLED"))
                         : String ("CHOOSE CHANGE IN THE RACK"),
                getWidth() - 180, 12, 168, 12, Justification::centredRight, false);

    if (! assigned)
    {
        g.setColour (muted());
        g.setFont (Font (11.f));
        g.drawFittedText ("This slot is empty. Use CHANGE in the rack to assign one of the instruments. Every visible knob is a real parameter.",
                          getLocalBounds().reduced (24).withTrimmedTop (90), Justification::topLeft, 4);
        return;
    }

    const float live = proc.slotLiveMix (slot);
    const auto* amtParam = proc.apvts.getRawParameterValue (params::amtId (fx));
    const float maxMix = amtParam != nullptr ? amtParam->load() : 0.f;
    auto meter = Rectangle<float> (16.f, 40.f, (float) getWidth() - 32.f, 5.f);
    g.setColour (line());
    g.fillRoundedRectangle (meter, 2.5f);
    g.setColour (c.withAlpha (0.9f));
    g.fillRoundedRectangle (meter.withWidth (jmax (2.f, meter.getWidth() * jlimit (0.f, 1.f, live))), 2.5f);
    g.setColour (text().withAlpha (0.5f));
    g.fillRect (meter.getX() + meter.getWidth() * jlimit (0.f, 1.f, maxMix) - 0.5f,
                meter.getY() - 1.f, 1.f, meter.getHeight() + 2.f);
    g.setColour (muted());
    g.setFont (Font (8.f).boldened());
    g.drawText ("LIVE " + String (roundToInt (live * 100.f)) + "%  ·  MAX "
                + String (roundToInt (maxMix * 100.f)) + "%",
                (int) meter.getX(), 46, (int) meter.getWidth(), 12,
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
    list.setRowHeight (34);
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
    g.setFont (Font (22.f).boldened());
    g.drawText ("PRESET LIBRARY", 16, 10, 340, 30, Justification::centredLeft, false);
    g.setColour (muted());
    g.setFont (Font (9.f).boldened());
    const String subtitle = folder == 0 ? String ("FACTORY PROGRAMMES")
                                        : String ("USER PRESETS · ") + String ((int) cachedUserFiles.size());
    g.drawText (subtitle, 160, 20, 300, 16, Justification::centredLeft, false);

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
        catButtons[i]->setBounds (16, y, 220, 26);
        y += 30;
    }
    list.setBounds (260, 70, getWidth() - 280, getHeight() - 130);
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
    g.setFont (Font (13.f));
    g.drawText (name, 14, 0, w - 150, h, Justification::centredLeft, false);
    g.setColour (muted());
    g.setFont (Font (9.f));
    g.drawText (cat, w - 140, 0, 126, h, Justification::centredRight, false);
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
      accent (p), kill (p), bypass (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    content.addAndMakeVisible (circle);
    content.addAndMakeVisible (rack);
    content.addAndMakeVisible (context);
    content.addAndMakeVisible (accent);
    content.addAndMakeVisible (kill);
    content.addAndMakeVisible (bypass);

    content.addAndMakeVisible (presetPrev);
    content.addAndMakeVisible (presetNext);
    content.addAndMakeVisible (presetName);
    content.addAndMakeVisible (saveBtn);
    content.addAndMakeVisible (browseBtn);
    content.addAndMakeVisible (themeBtn);

    depthKnob = std::make_unique<ui::KnobCard> (proc.apvts, "padDepth", "XY Depth", accent());
    fadeKnob  = std::make_unique<ui::KnobCard> (proc.apvts, "fade", "Fade", accentSoft());
    chaosKnob = std::make_unique<ui::KnobCard> (proc.apvts, "chaos", "Chaos", accentSoft());
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
    setResizeLimits (1008, 630, 2304, 1440);
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

    // left lunar field glow behind the circle
    g.setGradientFill (ColourGradient (accent().withAlpha (isDay() ? 0.06f : 0.075f), 358.f, 420.f,
                                       Colours::transparentBlack, 358.f, 820.f, true));
    g.fillRect (0.f, (float) kTopH, (float) kLeftW, H - (float) kTopH);

    // top bar
    g.setColour (bgTop());
    g.fillRect (0.f, 0.f, W, (float) kTopH);
    g.setColour (line());
    g.drawLine (0.f, (float) kTopH, W, (float) kTopH, 1.f);

    g.setColour (text());
    g.setFont (Font (25.f).boldened());
    g.drawText ("LUNA", 16, 10, 78, 30, Justification::centredLeft, false);
    g.setColour (accent());
    g.setFont (Font (17.f).boldened());
    g.drawText ("FX", 92, 14, 32, 24, Justification::centredLeft, false);

    auto badge = Rectangle<float> (126.f, 15.f, 46.f, 20.f);
    g.setColour (panelRaised());
    g.fillRoundedRectangle (badge, 4.f);
    g.setColour (line());
    g.drawRoundedRectangle (badge, 4.f, 1.f);
    g.setColour (muted());
    g.setFont (Font (8.f).boldened());
    g.drawText ("1.3.0", badge, Justification::centred);

    g.setColour (muted().withAlpha (0.75f));
    g.setFont (Font (8.f).boldened());
    g.drawText ("8 SLOT XY PERFORMANCE INSTRUMENT", 720, 18, 260, 12, Justification::centredLeft, false);
    g.drawText ("PRESET", 220, 2, 100, 11, Justification::centredLeft, false);

    // left performance strip
    g.setColour (line());
    g.drawLine (16.f, 786.f, (float) kLeftW - 16.f, 786.f, 1.f);
    g.setColour (muted());
    g.setFont (Font (8.f).boldened());
    g.drawText ("PERFORMANCE", 20, 770, 140, 12, Justification::centredLeft, false);
    g.drawText ("XY DEPTH · FADE · CHAOS · ACCENT", 160, 770, 330, 12, Justification::centredLeft, false);

    // right panel labels
    g.setColour (muted());
    g.setFont (Font (8.f).boldened());
    g.drawText ("INSTRUMENTS", Rectangle<float> ((float) kRackX + 4.f, 46.f, 120.f, 12.f),
                Justification::centredLeft, false);
    g.drawText ("EFFECT PARAMETERS", Rectangle<float> ((float) kCtxX + 4.f, 46.f, 180.f, 12.f),
                Justification::centredLeft, false);
    g.drawText ("SLOT 1 - 8", Rectangle<float> ((float) kRackX + 230.f, 46.f, 110.f, 12.f),
                Justification::centredRight, false);

    // status
    g.setColour (line());
    g.drawLine (0.f, 884.f, W, 884.f, 1.f);
    g.setColour (muted().withAlpha (0.9f));
    g.setFont (Font (8.f).boldened());
    g.drawText ("LUNA FX  " + String (params::kVersion) + "  ·  8 SLOTS  ·  DISTANCE MIX",
                16, 885, 360, 14, Justification::centredLeft, false);
    g.drawText ("EVERY KNOB IS AN APVTS PARAMETER", 380, 885, 380, 14, Justification::centredLeft, false);
    g.setColour (silver().withAlpha (0.85f));
    g.drawText ("SILVER RING = PRESET ASSIGNED  ·  COLOUR GLOW = LIVE",
                Rectangle<float> (W - 430.f, 885.f, 414.f, 14.f), Justification::centredRight, false);
}

void LunaFXEditor::resized()
{
    content.setBounds (0, 0, kDesignW, kDesignH);
    content.setTransform (AffineTransform::scale ((float) getWidth() / (float) kDesignW));

    circle.setBounds (0, 64, kLeftW, 700);
    rack.setBounds (kRackX, 64, kRackW, 820);
    context.setBounds (kCtxX, 64, kCtxW, 820);

    accent.setBounds (18, 796, 112, 82);
    depthKnob->setBounds (146, 790, 92, 92);
    fadeKnob ->setBounds (244, 790, 92, 92);
    chaosKnob->setBounds (342, 790, 92, 92);
    touchToggle ->setBounds (446, 800, 160, 24);
    springToggle->setBounds (446, 832, 160, 24);

    presetPrev.setBounds (186, 18, 26, 28);
    presetName.setBounds (214, 16, 292, 30);
    presetNext.setBounds (508, 18, 26, 28);
    saveBtn.setBounds (548, 18, 58, 28);
    browseBtn.setBounds (612, 18, 82, 28);
    themeBtn.setBounds (1000, 18, 64, 28);

    kill.setBounds (1196, 16, 70, 30);
    bypass.setBounds (1272, 16, 128, 30);

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

    accent.repaint();
    kill.repaint();
    bypass.repaint();
}
