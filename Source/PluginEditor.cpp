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
static const char* kCatName[6] = { "PITCH", "FX", "GLITCH", "MOD", "GEN", "TIME" };

// ============================================================  small draw helpers
static void fillRR (Graphics& g, Rectangle<float> r, float rad, Colour c)
    { g.setColour (c); g.fillRoundedRectangle (r, rad); }
static void strokeRR (Graphics& g, Rectangle<float> r, float rad, Colour c, float thick = 1.f)
    { g.setColour (c); g.drawRoundedRectangle (r, rad, thick); }

// ============================================================  FxDetailPanel
FxDetailPanel::FxDetailPanel (LunaFXProcessor& p) : proc (p) {}

void FxDetailPanel::showFx (int fxIndex)
{
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
    auto amtKn = std::make_unique<ParamControl> (proc.apvts, amtId.c_str(), "Amount",
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
    const int kw   = 90;
    const int kh   = 90;
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
    // create 6 category tab buttons
    for (int i = 0; i < 6; ++i)
    {
        auto* btn = catTabs.add (new TextButton (kCatName[i]));
        btn->addListener (this);
        addAndMakeVisible (btn);
    }
    selectCategory (0);
}

void FxCategoryPanel::selectCategory (int cat)
{
    activeCat = cat;
    rows.clear();
    for (auto* c : catTabs) removeChildComponent (c);
    for (auto* c : catTabs) addAndMakeVisible (c);

    // build rows for every FX in this category
    for (int i = 0; i < djx::kNumFx; ++i)
    {
        if (djx::kFxCategory[i] != cat) continue;
        FxRow row;
        row.fxIdx = i;

        row.nameBtn = std::make_unique<TextButton> (djx::kFxNames[i]);
        row.nameBtn->addListener (this);
        addAndMakeVisible (*row.nameBtn);

        row.amt = std::make_unique<Slider> (Slider::LinearHorizontal, Slider::NoTextBox);
        row.amt->setPopupDisplayEnabled (true, true, nullptr);
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
    for (auto& row : rows)
    {
        const float amt = proc.apvts.getRawParameterValue (params::amtId (row.fxIdx))->load();
        row.power->setToggleState (amt > 0.01f, dontSendNotification);

        // check if any of the 16 rim nodes has this FX assigned
        row.onPad = false;
        for (int ni = 0; ni < 16 && !row.onPad; ++ni)
        {
            const auto* pp = proc.apvts.getRawParameterValue ("n" + String (ni));
            if (pp && (int) (pp->load() + 0.5f) == row.fxIdx + 1) row.onPad = true;
        }
    }
    repaint();
}

void FxCategoryPanel::buttonClicked (Button* btn)
{
    for (int i = 0; i < catTabs.size(); ++i)
        if (catTabs[i] == btn) { selectCategory (i); return; }
    for (auto& row : rows)
        if (row.nameBtn.get() == btn)
        {
            selFx = (selFx == row.fxIdx) ? -1 : row.fxIdx;
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
    const int tw = tabs.getWidth() / 6;
    for (int i = 0; i < 6; ++i)
        catTabs[i]->setBounds (tabs.getX() + i * tw, tabs.getY(), tw - 2, 26);

    area.removeFromTop (4);
    const int rowH = 36;
    for (auto& row : rows)
    {
        auto r = area.removeFromTop (rowH);
        row.nameBtn->setBounds (r.getX(), r.getY() + 4, 130, 28);
        row.power  ->setBounds (r.getRight() - 28, r.getY() + 8, 20, 20);
        row.amt    ->setBounds (r.getX() + 136, r.getY() + 10,
                                 r.getWidth() - 136 - 34, 16);
    }
}

void FxCategoryPanel::paint (Graphics& g)
{
    // category tab styling
    for (int i = 0; i < catTabs.size(); ++i)
    {
        auto b = catTabs[i]->getBounds().toFloat();
        const bool active = (i == activeCat);
        fillRR   (g, b, 5.f, active ? catCol (i).withAlpha (0.9f) : lunaPanelMd());
        strokeRR (g, b, 5.f, catCol (i), active ? 2.f : 1.f);
        catTabs[i]->setColour (TextButton::textColourOffId, active ? lunaBase() : catCol (i));
        catTabs[i]->setColour (TextButton::buttonColourId, Colours::transparentBlack);
        catTabs[i]->setColour (TextButton::buttonOnColourId, Colours::transparentBlack);
    }
    // FX rows
    for (auto& row : rows)
    {
        const bool sel    = (row.fxIdx == selFx);
        const float amt   = proc.apvts.getRawParameterValue (params::amtId (row.fxIdx))->load();
        const bool  on    = amt > 0.01f;
        const Colour col  = catCol (activeCat);
        auto rb = Rectangle<float>(
            (float) getLocalBounds().getX(),
            (float) row.nameBtn->getBounds().getY() - 3,
            (float) getLocalBounds().getWidth(),
            34.f);
        fillRR   (g, rb, 5.f, sel ? col.withAlpha (0.14f) : lunaPanelLt());
        strokeRR (g, rb, 5.f, sel ? col : lunaBorder(), sel ? 2.f : 1.f);
        // dot
        const float dy = rb.getCentreY();
        g.setColour (on ? col : lunaDim());
        g.fillEllipse ((float) row.nameBtn->getX() - 14.f, dy - 4.f, 8.f, 8.f);
        // ON PAD badge
        if (row.onPad)
        {
            const float bx = (float) row.nameBtn->getRight() + 6.f;
            auto badge = Rectangle<float> (bx, dy - 9.f, 44.f, 18.f);
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
        // amt bar track
        auto sb = row.amt->getBounds().toFloat();
        g.setColour (lunaBorder());
        g.fillRoundedRectangle (sb.withHeight (4.f).withY (sb.getCentreY() - 2.f), 2.f);
        g.setColour (col.withAlpha (0.8f));
        g.fillRoundedRectangle (sb.withHeight (4.f).withY (sb.getCentreY() - 2.f)
                                    .withWidth (sb.getWidth() * amt), 2.f);
        // FX name
        row.nameBtn->setColour (TextButton::textColourOffId, on ? lunaText() : lunaSub());
        row.nameBtn->setColour (TextButton::buttonColourId, Colours::transparentBlack);
        row.nameBtn->setColour (TextButton::buttonOnColourId, Colours::transparentBlack);
    }
}

// ============================================================  PresetBrowserPanel
PresetBrowserPanel::PresetBrowserPanel (LunaFXProcessor& p) : proc (p)
{
    listModel.proc = &p;
    listModel.folder = 0;

    for (const char* lbl : { "Factory", "My Presets", "DJ Drops", "Templates" })
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
}

void PresetBrowserPanel::refreshList()
{
    presetList.updateContent();
    presetList.selectRow (proc.getCurrentProgram());
    repaint();
}

void PresetBrowserPanel::resized()
{
    auto area = getLocalBounds().reduced (6, 4);
    // folder strip
    auto folders = area.removeFromTop (34);
    const int fw = (folders.getWidth() - 60) / 4;
    for (int i = 0; i < 4; ++i)
        folderBtns[i]->setBounds (folders.getX() + i * (fw + 4),
                                   folders.getY() + 3, fw, 28);
    saveBtn.setBounds (folders.getRight() - 56, folders.getY() + 3, 56, 28);
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
            auto name = ep->getText();
            if (name.isNotEmpty())
                sp->proc.changeProgramName (sp->proc.getCurrentProgram(), name);
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

int  PresetBrowserPanel::PresetListModel::getNumRows()
    { return folder == 0 ? proc->getNumPrograms() : 0; }

void PresetBrowserPanel::PresetListModel::paintListBoxItem
    (int row, Graphics& g, int w, int h, bool sel)
{
    if (sel) { g.setColour (cAccent().withAlpha (0.2f)); g.fillRoundedRectangle (2.f, 1.f, (float) w - 4.f, (float) h - 2.f, 4.f); }
    g.setColour (sel ? lunaText() : lunaSub());
    g.setFont (Font (12.f));
    g.drawText (proc->getProgramName (row),
                Rectangle<int> (12, 0, w - 12, h), Justification::centredLeft);
}

void PresetBrowserPanel::PresetListModel::listBoxItemClicked (int row, const MouseEvent& /*e*/)
    { proc->setCurrentProgram (row); }

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

    // Default node angles (evenly spaced, 8 nodes)
    static const float kAngles[8]  = { 225.f, 270.f, 315.f, 0.f, 45.f, 90.f, 135.f, 180.f };
    // Default FX assignments matching old TL/TR/BL/BR + 4 edge defaults
    static const int   kDefFx[8]   = {
        djx::Fx::OctDown, djx::Fx::Shimmer, djx::Fx::Scatter, djx::Fx::Fuzz,
        djx::Fx::Filter,  djx::Fx::Riser,   djx::Fx::Chorus,  djx::Fx::Delay };
    for (int i = 0; i < kNumNodes; ++i)
    {
        nodes[i].angleDeg = kAngles[i];
        nodes[i].fxIndex  = kDefFx[i];
    }
    syncNodesToPreset();
}

void PlayPage::syncNodesToPreset()
{
    // 16-node system: node FX are read directly from n0..n15 params in CircleXYPad::paint().
    repaint();
}

void PlayPage::tick (float /*dt*/)
{
    // update node activity from uiAmt
    for (int i = 0; i < kNumNodes; ++i)
    {
        const int fx = nodes[i].fxIndex;
        nodes[i].activity = (fx >= 0 && fx < djx::kNumFx) ? proc.uiAmt[fx].load() : 0.f;
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
    const auto b  = getLocalBounds();
    const int  pw = jmin (b.getWidth() - 20, b.getHeight() - 140);
    const int  px = (b.getWidth() - pw) / 2;
    const int  py = 16;
    pad.setBounds (px, py, pw, pw);

    accent.setBounds (b.getCentreX() - 44, b.getHeight() - 118, 88, 104);

    // bottom bar: four knobs
    const int kw = 78, kh = 90, ky = b.getHeight() - 130;
    const int totalKw = 4 * kw + 3 * 6;
    int kx = (b.getWidth() - totalKw) / 2 - 70;
    for (auto* k : { chaos.get(), rate.get(), depth.get(), feedback.get() })
    {
        k->setBounds (kx, ky, kw, kh);
        kx += kw + 6;
    }

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

    // draw all 8 nodes
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
    drawRingPad (g);

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

// ============================================================  SetupPage (unchanged layout, new name)
SetupPage::SetupPage (LunaFXProcessor& p)
{
    auto add = [&] (std::vector<std::unique_ptr<ParamControl>>& v,
                    const char* id, const char* cap, Colour c)
    {
        v.push_back (std::make_unique<ParamControl> (p.apvts, id, cap, c));
        addAndMakeVisible (*v.back());
    };
    add (knobs, "fade",     "Engage Fade",   cAccent());
    add (knobs, "chaos",    "Chaos",          cAccent());
    add (knobs, "padDepth", "Pad Depth",      cAccent2());
    add (combos, "engMode",  "Footswitch Mode", cAccent());
    add (combos, "autoRand", "Auto Randomize",  cAccent());
    // padMode removed (16-node system uses angular blending)
    add (combos, "uiTheme",  "Visual Theme",     cAccent2());
    add (toggles, "midi",      "MIDI Hold = Accent",  cAccent());
    add (toggles, "padTouch",  "Touch Pad = Accent",  cAccent());
    add (toggles, "padSpring", "Pad Springs Back",    cAccent());

    // old 8-zone params removed
    const char* zIds[8]  = {};
    const char* zOn[8]   = {};
    const char* zNames[8]= { "Top Edge","Right Edge","Bottom Edge","Left Edge",
                              "Centre 1","Centre 2","Centre 3","Centre 4" };
    for (int i = 0; i < 8; ++i)
    {
        add (zoneCombos,  zIds[i],  zNames[i], i < 4 ? cAccent() : cAccent2());
        add (zoneToggles, zOn[i],   "Enable",  i < 4 ? cAccent() : cAccent2());
    }
}

void SetupPage::resized()
{
    auto b = getLocalBounds().withSizeKeepingCentre (1030, 650);
    b.removeFromTop (60);
    auto k = b.removeFromTop (98);
    const int kw = k.getWidth() / 3;
    for (int i = 0; i < 3; ++i)
        knobs[(size_t)i]->setBounds (k.getX() + i * kw + 42, k.getY(), kw - 84, k.getHeight());
    b.removeFromTop (4);
    auto c = b.removeFromTop (54);
    const int cw = c.getWidth() / 4;
    for (int i = 0; i < 4; ++i)
        combos[(size_t)i]->setBounds (c.getX() + i * cw + 8, c.getY(), cw - 16, c.getHeight());
    b.removeFromTop (4);
    auto t = b.removeFromTop (38);
    const int tw = t.getWidth() / 3;
    for (int i = 0; i < 3; ++i)
        toggles[(size_t)i]->setBounds (t.getX() + i * tw + 12, t.getY(), tw - 24, t.getHeight());
    b.removeFromTop (34);
    const int colW = b.getWidth() / 4;
    for (int i = 0; i < 8; ++i)
    {
        int row = i / 4, col = i % 4;
        zoneCombos[(size_t)i] ->setBounds (b.getX() + col * colW + 8,     b.getY() + row * 106,      colW - 16, 50);
        zoneToggles[(size_t)i]->setBounds (b.getX() + col * colW + colW/2 - 42, b.getY() + row * 106 + 54, 84, 30);
    }
}

void SetupPage::paint (Graphics& g)
{
    auto b = getLocalBounds().withSizeKeepingCentre (1030, 650).toFloat();
    fillRR   (g, b, 18.f, Colour (0xff0f1018));
    strokeRR (g, b, 18.f, lunaBorder());
    g.setColour (lunaText()); g.setFont (Font (18.f).boldened());
    g.drawText ("SETTINGS & XY ROUTING",
                b.removeFromTop (60.f).withTrimmedLeft (28.f), Justification::centredLeft);
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
    saveBtn  .onClick  = [this] { presets.saveCurrentPreset(); };  // delegate
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

    // ---- left nav (8 buttons)
    const char* navLabels[8] = { "XY","PITCH","FX","GLITCH","MOD","GEN","TIME","PRE" };
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
    activeNav = idx;

    // XY pad visible for nav 0; setup visible for nav 7 repurposed or a dedicated item
    play .setVisible (idx <= 6);          // all categories show ring pad on left
    setup.setVisible (false);             // setup is in the PRE page for now
    presets.setVisible (idx == 7);

    // right panel: XY = no category, others open matching category
    if (idx >= 1 && idx <= 6)
    {
        fxCats.selectCategory (idx - 1);  // 1->PITCH(0), 2->FX(1) ...
        fxCats.setVisible (true);
        fxDetail.setVisible (true);
    }
    else
    {
        fxDetail.showFx (-1);
        fxCats.setVisible (idx != 7);    // hide cat panel on preset page
        fxDetail.setVisible (idx != 7);
    }

    for (int i = 0; i < navBtns.size(); ++i)
    {
        navBtns[i]->setColour (TextButton::textColourOffId,
                                i == idx ? lunaText() : lunaDim());
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
    auto vbadge = Rectangle<float> (94.f, 13.f, 34.f, 22.f);
    fillRR   (g, vbadge, 4.f, Colour (0xff1a1c2a));
    strokeRR (g, vbadge, 4.f, lunaBorder());
    g.setFont (Font (9.f)); g.setColour (lunaSub());
    g.drawText ("v1.1", vbadge, Justification::centred);

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
    const int navH  = (kDesignH - kTopH) / 8;
    const int ny    = kTopH + activeNav * navH;
    g.setColour (Colour (0xff181a28));
    g.fillRect (0.f, (float) ny, (float) kNavW, (float) navH);
    const Colour navAccent = (activeNav >= 1 && activeNav <= 6)
                             ? catCol (activeNav - 1) : cAccent();
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
    g.drawText ("LUNA FX  v1.1.0", Rectangle<float> (8.f, H - 18.f, 130.f, 16.f), Justification::centredLeft);
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
    const int navH = (kDesignH - kTopH) / 8;
    for (int i = 0; i < navBtns.size(); ++i)
        navBtns[i]->setBounds (0, kTopH + i * navH, kNavW, navH);

    // ---- play area (left portion of page area)
    const Rectangle<int> padArea (kNavW, kTopH, kSplitX, kDesignH - kTopH - 20);
    play .setBounds (padArea);
    setup.setBounds (padArea);

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
