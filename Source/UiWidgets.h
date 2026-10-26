// UI building blocks. Visual language: dark navy -> violet gradient, round "terrain" XY pad,
// pill-shaped effect modules and a smooth lunar XY surface.
#pragma once
#include "PluginProcessor.h"
#include <functional>

namespace ui
{
using namespace juce;

inline std::atomic<int> gUiTheme { 0 }; // 0 Night, 1 Day, 2 Moonlight
inline void setUiTheme (int t) { gUiTheme.store (jlimit (0, 2, t)); }
inline Colour cBgTop()   { const int t=gUiTheme.load(); return t==1 ? Colour(0xffe8e5df) : (t==2 ? Colour(0xff252932) : Colour(0xff0a0c13)); }
inline Colour cBg()      { const int t=gUiTheme.load(); return t==1 ? Colour(0xfff6f4ef) : (t==2 ? Colour(0xff101217) : Colour(0xff080a10)); }
inline Colour cPanel()   { const int t=gUiTheme.load(); return t==1 ? Colour(0xffe7e4dc) : (t==2 ? Colour(0xff1b1e25) : Colour(0xff0f1119)); }
inline Colour cLine()    { const int t=gUiTheme.load(); return t==1 ? Colour(0xffc8c3b8) : (t==2 ? Colour(0xff4b515d) : Colour(0xff1c2235)); }
inline Colour cText()    { return gUiTheme.load()==1 ? Colour(0xff25272b) : Colour(0xffeeece7); }
inline Colour cMuted()   { return gUiTheme.load()==1 ? Colour(0xff70716f) : Colour(0xff9a9da4); }
inline Colour cAccent()  { const int t=gUiTheme.load(); return t==1 ? Colour(0xff666b70) : (t==2 ? Colour(0xffb9c1d0) : Colour(0xff7c5ef8)); }
inline Colour cAccent2() { const int t=gUiTheme.load(); return t==1 ? Colour(0xff8d8b82) : (t==2 ? Colour(0xffe2e6ef) : Colour(0xffa87cf8)); }
inline Colour cWarn()    { return gUiTheme.load()==1 ? Colour(0xffb05c4d) : Colour(0xffb56b60); }

inline void setBool (RangedAudioParameter* p, bool v)  { p->beginChangeGesture(); p->setValueNotifyingHost (v ? 1.f : 0.f); p->endChangeGesture(); }
inline void setChoice (RangedAudioParameter* p, int i) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 ((float) i)); p->endChangeGesture(); }
inline bool isOn (RangedAudioParameter* p)             { return p->getValue() > 0.5f; }
inline int  choiceIndex (RangedAudioParameter* p)      { return (int) (p->convertFrom0to1 (p->getValue()) + 0.5f); }

// ======================================================================================
class LunaLnF : public LookAndFeel_V4
{
public:
    LunaLnF() { updateTheme(); }

    void updateTheme()
    {
        setColour (ComboBox::backgroundColourId, cPanel());
        setColour (ComboBox::outlineColourId, cLine());
        setColour (ComboBox::textColourId, cText());
        setColour (ComboBox::arrowColourId, cMuted());
        setColour (PopupMenu::backgroundColourId, cPanel());
        setColour (PopupMenu::textColourId, cText());
        setColour (PopupMenu::highlightedBackgroundColourId, cAccent().withAlpha (0.55f));
        setColour (PopupMenu::highlightedTextColourId, cText());
        setColour (Label::textColourId, cMuted());
        setColour (Slider::trackColourId, cAccent());
        setColour (TextButton::buttonColourId, cPanel());
        setColour (TextButton::textColourOffId, cMuted());
        setColour (TextButton::textColourOnId, cBg());
    }

    void drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider& s) override
    {
        auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.f);
        const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float th = jmax (2.f, r * 0.13f), ar = r - th * 0.5f;

        Path track; track.addCentredArc (c.x, c.y, ar, ar, 0.f, a0, a1, true);
        g.setColour (cLine()); g.strokePath (track, PathStrokeType (th, PathStrokeType::curved, PathStrokeType::rounded));

        const float ang = a0 + pos * (a1 - a0);
        Path val; val.addCentredArc (c.x, c.y, ar, ar, 0.f, a0, ang, true);
        g.setColour (s.findColour (Slider::rotarySliderFillColourId));
        g.strokePath (val, PathStrokeType (th, PathStrokeType::curved, PathStrokeType::rounded));

        const float kr = r - th * 1.9f;
        g.setColour (Colour (0xff1c1d29)); g.fillEllipse (c.x - kr, c.y - kr, kr * 2.f, kr * 2.f);
        g.setColour (cLine().brighter (0.2f)); g.drawEllipse (c.x - kr, c.y - kr, kr * 2.f, kr * 2.f, 1.f);
        Path p; p.addRoundedRectangle (-1.4f, -kr + 2.f, 2.8f, kr * 0.5f, 1.4f);
        g.setColour (cText()); g.fillPath (p, AffineTransform::rotation (ang).translated (c.x, c.y));
    }

    void drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box) override
    {
        auto b = Rectangle<float> (0.f, 0.f, (float) w, (float) h).reduced (0.5f);
        g.setColour (box.findColour (ComboBox::backgroundColourId)); g.fillRoundedRectangle (b, 7.f);
        g.setColour (cLine().brighter (0.15f)); g.drawRoundedRectangle (b, 7.f, 1.f);
        Path p; const float ax = (float) w - 15.f, ay = (float) h * 0.5f - 2.f;
        p.addTriangle (ax, ay, ax + 8.f, ay, ax + 4.f, ay + 5.f);
        g.setColour (cMuted()); g.fillPath (p);
    }

    void drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        Colour fill = on ? b.findColour (TextButton::buttonOnColourId) : cPanel();
        if (over) fill = fill.brighter (0.08f);
        if (down) fill = fill.darker (0.1f);
        g.setColour (fill); g.fillRoundedRectangle (r, 7.f);
        g.setColour (on ? fill : cLine().brighter (0.15f)); g.drawRoundedRectangle (r, 7.f, 1.f);
    }

    void drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, Slider::SliderStyle, Slider& s) override
    {
        const float cy = (float) y + (float) h * 0.5f;
        g.setColour (cLine()); g.fillRoundedRectangle ((float) x, cy - 2.f, (float) w, 4.f, 2.f);
        g.setColour (s.findColour (Slider::trackColourId)); g.fillRoundedRectangle ((float) x, cy - 2.f, pos - (float) x, 4.f, 2.f);
        g.setColour (cText()); g.fillRoundedRectangle (pos - 3.f, cy - 8.f, 6.f, 16.f, 3.f);
    }
};

// ======================================================================================
// Generic parameter control (rotary / combo / toggle) bound through the APVTS
class ParamControl : public Component
{
public:
    ParamControl (AudioProcessorValueTreeState& s, const String& id, const String& caption, Colour col, bool showCaption = true)
        : showCap (showCaption)
    {
        auto* p = s.getParameter (id);
        jassert (p != nullptr);

        if (auto* cp = dynamic_cast<AudioParameterChoice*> (p))
        {
            combo = std::make_unique<ComboBox>();
            combo->addItemList (cp->choices, 1);
            addAndMakeVisible (*combo);
            comboAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (s, id, *combo);
        }
        else if (dynamic_cast<AudioParameterBool*> (p) != nullptr)
        {
            button = std::make_unique<TextButton> (caption);
            button->setClickingTogglesState (true);
            button->setColour (TextButton::buttonOnColourId, col);
            addAndMakeVisible (*button);
            buttonAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (s, id, *button);
            showCap = false;
        }
        else
        {
            slider = std::make_unique<Slider>();
            slider->setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
            slider->setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
            slider->setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
            slider->setColour (Slider::rotarySliderFillColourId, col);
            slider->setPopupDisplayEnabled (true, true, nullptr);
            slider->textFromValueFunction = [unit = (p != nullptr ? p->getLabel() : String()), lo = p->getNormalisableRange().start, hi = p->getNormalisableRange().end] (double value)
            {
                String suffix = unit;
                double shown = value;
                int decimals = (std::abs (value) >= 100.0 || value == std::floor (value)) ? 0 : (std::abs (value) >= 10.0 ? 1 : 2);
                if (unit == "%" && lo == 0.0 && hi == 1.0) { shown = value * 100.0; decimals = 0; }
                else if (unit == "Hz" && std::abs (value) >= 1000.0) { shown = value / 1000.0; suffix = "kHz"; decimals = 2; }
                String out (shown, decimals);
                if (out.containsChar ('.')) out = out.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
                if (out.isEmpty() || out == "-0") out = "0";
                if (suffix.isNotEmpty()) out << " " << suffix;
                return out;
            };
            addAndMakeVisible (*slider);
            sliderAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (s, id, *slider);
            slider->setDoubleClickReturnValue (true, (double) p->convertFrom0to1 (p->getDefaultValue()));
        }

        if (showCap)
        {
            cap.setText (caption, dontSendNotification);
            cap.setJustificationType (Justification::centred);
            cap.setColour (Label::textColourId, cMuted());
            cap.setFont (cap.getFont().withHeight (11.f));
            cap.setInterceptsMouseClicks (false, false);
            addAndMakeVisible (cap);
        }
    }

    void resized() override
    {
        auto b = getLocalBounds();
        if (showCap) cap.setBounds (b.removeFromBottom (14));
        if (slider)
        {
            const int sz = jmin (b.getWidth(), b.getHeight());
            slider->setBounds (b.withSizeKeepingCentre (sz, sz));
        }
        if (combo)  combo->setBounds (b.withSizeKeepingCentre (b.getWidth(), jmin (26, b.getHeight())));
        if (button) button->setBounds (b.withSizeKeepingCentre (b.getWidth(), jmin (28, b.getHeight())));
    }

private:
    bool showCap;
    Label cap;
    std::unique_ptr<Slider> slider;
    std::unique_ptr<ComboBox> combo;
    std::unique_ptr<TextButton> button;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> sliderAtt;
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> comboAtt;
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> buttonAtt;
};

// ======================================================================================
// One effect card for the Rack page: amount knob + detail knobs + activity bar
class FxStrip : public Component
{
public:
    FxStrip (LunaFXProcessor& p, int fxIndex)
        : proc (p), idx (fxIndex), col (params::catColour (djx::kFxCategory[fxIndex]))
    {
        amount = std::make_unique<ParamControl> (p.apvts, String (params::amtId (fxIndex)), "Amount", col);
        addAndMakeVisible (*amount);
        for (auto& id : params::fxInfo()[(size_t) fxIndex].details)
        {
            const auto* d = params::find (id);
            auto c = std::make_unique<ParamControl> (p.apvts, String (id), d != nullptr ? String (d->name) : String (id), col);
            addAndMakeVisible (*c);
            details.push_back (std::move (c));
        }
    }

    void refresh()
    {
        const float a = proc.uiAmt[idx].load();
        const bool e = proc.uiEngage.load() > 0.3f;
        if (std::abs (a - shown) > 0.004f || e != engaged) { shown = a; engaged = e; repaint(); }
    }

    void paint (Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (2.f);
        g.setColour (cPanel()); g.fillRoundedRectangle (b, 12.f);
        g.setColour (col.withAlpha (0.18f + 0.5f * shown)); g.drawRoundedRectangle (b, 12.f, 1.2f);

        auto t = b.reduced (10.f, 6.f);
        g.setColour (cText()); g.setFont (14.f);
        g.drawText (String (djx::kFxNames[idx]), t.removeFromTop (20.f), Justification::centredLeft);

        auto bar = t.removeFromTop (6.f).reduced (0.f, 1.f);
        g.setColour (cLine()); g.fillRoundedRectangle (bar, 2.f);
        g.setColour (engaged ? col : col.withAlpha (0.45f));
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * shown), 2.f);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (6);
        b.removeFromTop (30);
        if (details.empty()) { amount->setBounds (b); return; }
        amount->setBounds (b.removeFromLeft ((int) ((float) b.getWidth() * 0.40f)));
        const int n = (int) details.size();
        const int cols = jlimit (1, n, b.getWidth() / 46);
        const int rows = (n + cols - 1) / cols;
        const int cw = b.getWidth() / cols, ch = b.getHeight() / rows;
        for (int i = 0; i < n; ++i)
            details[(size_t) i]->setBounds (b.getX() + (i % cols) * cw, b.getY() + (i / cols) * ch, cw, ch);
    }

private:
    LunaFXProcessor& proc;
    int idx;
    Colour col;
    float shown = 0.f;
    bool engaged = false;
    std::unique_ptr<ParamControl> amount;
    std::vector<std::unique_ptr<ParamControl>> details;
};

// ======================================================================================
// Round XY pad with an animated voxel-terrain texture, rings and a puck
class CircleXYPad : public Component, private Timer
{
public:
    explicit CircleXYPad (LunaFXProcessor& p) : proc (p)
    {
        px = p.apvts.getParameter ("padX");   py = p.apvts.getParameter ("padY");
        pe = p.apvts.getParameter ("engage"); pTouch = p.apvts.getParameter ("padTouch");
        pSpring = p.apvts.getParameter ("padSpring"); pByp = p.apvts.getParameter ("bypass");
        for (int ni = 0; ni < 16; ++ni)
        {
            nChoice[ni] = p.apvts.getRawParameterValue ("n" + juce::String (ni));
            nEnable[ni] = p.apvts.getRawParameterValue ("n" + juce::String (ni) + "e");
        }
        img = Image (Image::ARGB, kN, kN, true);
        setMouseCursor (MouseCursor::CrosshairCursor);
    }

    Rectangle<float> outerCircle() const
    {
        auto b = getLocalBounds().toFloat().reduced (4.f);
        return b.withSizeKeepingCentre (jmin (b.getWidth(), b.getHeight()), jmin (b.getWidth(), b.getHeight()));
    }
    Rectangle<float> innerCircle() const { auto o = outerCircle(); return o.reduced (o.getWidth() * 0.0275f); }

    void tick (float dt)
    {
        float energy = 0.f;
        for (int i = 0; i < djx::kNumFx; ++i) energy += proc.uiAmt[i].load();
        energy = jmin (1.f, energy / 3.f);
        const float eng = proc.uiEngage.load();
        phase += dt * (0.35f + 1.6f * eng + 1.2f * energy);
        render (energy);
        repaint();
    }

    void paint (Graphics& g) override
    {
        const auto o = outerCircle(), in = innerCircle();
        const auto c = in.getCentre();
        const float R = in.getWidth() * 0.5f;
        const float eng = proc.uiEngage.load();
        const bool byp = isOn (pByp);

        g.setColour (Colour (0xff07080c)); g.fillEllipse (o);
        g.setColour (cLine()); g.drawEllipse (o.reduced (1.f), 1.4f);

        {
            Graphics::ScopedSaveState ss (g);
            Path clip; clip.addEllipse (in);
            g.reduceClipRegion (clip);
            g.setImageResamplingQuality (Graphics::highResamplingQuality); // smooth plasma texture
            g.drawImage (img, (int) in.getX(), (int) in.getY(), (int) in.getWidth() + 1, (int) in.getHeight() + 1, 0, 0, kN, kN);
            g.setGradientFill (ColourGradient (Colours::transparentBlack, c.x, c.y, Colours::black.withAlpha (0.55f), c.x + R, c.y, true));
            g.fillEllipse (in);
            if (byp) { g.setColour (Colours::black.withAlpha (0.62f)); g.fillEllipse (in); }
        }

        // ---- 16 rim nodes: node 0 = top, clockwise ----
        float nw[16] {};
        params::nodeWeights (px->getValue(), py->getValue(), nw);
        const float depth = proc.apvts.getRawParameterValue ("padDepth")->load();
        const Point<float> pk = puckPos();
        static constexpr float kPi2    = 3.14159265358979f;
        static constexpr float kTwoPi2 = 6.28318530717959f;

        for (int i = 0; i < 16; ++i)
        {
            const float ang = kPi2 * 0.5f - i * kTwoPi2 / 16.f;
            const Point<float> np (c.x + std::cos (ang) * (R + 1.f),
                                   c.y - std::sin (ang) * (R + 1.f));
            const float wt = nw[i] * depth;
            int        fx  = nChoice[i] ? (int) (nChoice[i]->load() + 0.5f) - 1 : -1;
            bool       ena = nEnable[i] != nullptr && nEnable[i]->load() > 0.5f;
            const Colour nc = (ena && fx >= 0 && fx < djx::kNumFx)
                              ? params::catColour (djx::kFxCategory[fx])
                              : Colour (0xff2a3045);

            // Blend line puck -> node
            if (ena && wt > 0.02f)
            {
                g.setColour (nc.withAlpha (0.30f * wt));
                g.drawLine (pk.x, pk.y, np.x, np.y, 1.4f * wt + 0.4f);
            }

            // Node backing + fill
            const float nr = 4.5f + 7.f * wt;
            g.setColour (Colour (0xff080a10));
            g.fillEllipse (np.x - nr - 2.f, np.y - nr - 2.f, (nr + 2.f) * 2.f, (nr + 2.f) * 2.f);
            g.setColour (ena ? nc.withAlpha (0.25f + 0.75f * wt) : Colour (0xff2a3045));
            g.fillEllipse (np.x - nr, np.y - nr, nr * 2.f, nr * 2.f);
            g.setColour (ena ? nc.withAlpha (0.6f + 0.4f * wt) : Colour (0xff3a4050));
            g.drawEllipse (np.x - nr - 0.5f, np.y - nr - 0.5f, (nr + 0.5f) * 2.f, (nr + 0.5f) * 2.f, 1.f);

            // FX name (hover-quality: always show when weight > threshold)
            if (ena && fx >= 0 && wt > 0.08f)
            {
                g.setColour (Colour (0xffc8d4e8).withAlpha (0.4f + 0.6f * jmin (1.f, wt * 2.f)));
                g.setFont (Font (9.f, Font::plain));
                const float lx = c.x + std::cos (ang) * (R + 20.f);
                const float ly = c.y - std::sin (ang) * (R + 20.f);
                g.drawText (String (djx::kFxNames[fx]), (int) lx - 32, (int) ly - 7, 64, 14,
                            Justification::centred, false);
            }
        }

        // Concentric rings around puck
        const float pulse = 1.f + 0.07f * std::sin (phase * 3.f) * eng;
        for (float rr : { 70.f, 38.f })
        {
            const float r2 = rr * (R / 215.f) * pulse;
            g.setColour (Colours::white.withAlpha (0.10f + 0.16f * eng));
            g.drawEllipse (pk.x - r2, pk.y - r2, r2 * 2.f, r2 * 2.f, 1.2f);
        }

        // Puck (Current-style: large white ring + inner dot)
        const float pr = 14.f;
        if (eng > 0.05f)
        {
            g.setGradientFill (ColourGradient (
                Colour (0xff7c5ef8).withAlpha (0.4f * eng), pk.x, pk.y,
                Colours::transparentBlack, pk.x + 52.f, pk.y, true));
            g.fillEllipse (pk.x - 52.f, pk.y - 52.f, 104.f, 104.f);
        }
        g.setColour (Colour (0xff080a10).withAlpha (0.5f));
        g.fillEllipse (pk.x - pr, pk.y - pr, pr * 2.f, pr * 2.f);
        g.setColour (Colours::white);
        g.drawEllipse (pk.x - pr, pk.y - pr, pr * 2.f, pr * 2.f, 2.2f);
        g.setColour (Colours::white.withAlpha (0.9f));
        g.fillEllipse (pk.x - 3.f, pk.y - 3.f, 6.f, 6.f);

        if (byp)
        {
            g.setColour (cWarn()); g.setFont (16.f);
            g.drawText ("BYPASSED", in.withSizeKeepingCentre (200.f, 30.f).translated (0.f, R * 0.55f), Justification::centred);
        }
    }

    void mouseDown (const MouseEvent& e) override
    {
        springTimerOff();
        px->beginChangeGesture(); py->beginChangeGesture();
        gestureOpen = true;
        setFromMouse (e.position);
        if (isOn (pTouch) && ! isOn (pe))
        {
            touchEngaged = true;
            pe->beginChangeGesture(); pe->setValueNotifyingHost (1.f);
        }
    }
    void mouseDrag (const MouseEvent& e) override { setFromMouse (e.position); }
    void mouseUp (const MouseEvent&) override
    {
        if (touchEngaged) { pe->setValueNotifyingHost (0.f); pe->endChangeGesture(); touchEngaged = false; }
        if (isOn (pSpring)) startTimerHz (60);
        else closeGestures();
    }
    void mouseDoubleClick (const MouseEvent&) override
    {
        px->beginChangeGesture(); py->beginChangeGesture();
        px->setValueNotifyingHost (0.5f); py->setValueNotifyingHost (0.5f);
        px->endChangeGesture(); py->endChangeGesture();
    }

private:
    static constexpr int kN = 96;

    int choiceIndexOf (const char* id) const
    {
        return (int) (proc.apvts.getRawParameterValue (id)->load() + 0.5f);
    }
    Point<float> puckPos() const
    {
        const auto in = innerCircle(); const auto c = in.getCentre(); const float R = in.getWidth() * 0.5f;
        return { c.x + (px->getValue() - 0.5f) * 2.f * R, c.y - (py->getValue() - 0.5f) * 2.f * R };
    }
    void setFromMouse (Point<float> p)
    {
        const auto in = innerCircle(); const auto c = in.getCentre(); const float R = in.getWidth() * 0.5f;
        float dx = (p.x - c.x) / R, dy = (c.y - p.y) / R;
        const float len = std::sqrt (dx * dx + dy * dy);
        if (len > 1.f) { dx /= len; dy /= len; }
        px->setValueNotifyingHost (0.5f + 0.5f * dx);
        py->setValueNotifyingHost (0.5f + 0.5f * dy);
    }
    void closeGestures() { if (gestureOpen) { px->endChangeGesture(); py->endChangeGesture(); gestureOpen = false; } }
    void springTimerOff() { stopTimer(); closeGestures(); }
    void timerCallback() override
    {
        float x = px->getValue(), y = py->getValue();
        x += (0.5f - x) * 0.25f; y += (0.5f - y) * 0.25f;
        if (std::abs (x - 0.5f) < 0.004f && std::abs (y - 0.5f) < 0.004f) { x = y = 0.5f; stopTimer(); px->setValueNotifyingHost (x); py->setValueNotifyingHost (y); closeGestures(); return; }
        px->setValueNotifyingHost (x); py->setValueNotifyingHost (y);
    }

    void render (float energy)
    {
        const float t = phase;
        const float bx = px->getValue(), by = 1.f - py->getValue();
        Image::BitmapData bd (img, Image::BitmapData::writeOnly);
        for (int j = 0; j < kN; ++j)
        for (int i = 0; i < kN; ++i)
        {
            const float u = (float) i / (float) (kN - 1);
            const float v = (float) j / (float) (kN - 1);
            // Plasma layers
            const float p1 = 0.5f + 0.5f * std::sin (u * 7.f + t * 0.9f
                                + std::cos (v * 5.f + t * 0.5f) * 1.8f);
            const float p2 = 0.5f + 0.5f * std::sin (v * 9.f - t * 0.7f
                                + std::sin (u * 6.f + t * 0.3f) * 1.5f);
            const float p3 = 0.5f + 0.5f * std::sin ((u + v) * 11.f + t * 1.1f
                                + energy * 3.14f);
            // Puck glow
            const float pdx = u - bx, pdy = v - by;
            const float puckGlow = std::exp (-(pdx * pdx + pdy * pdy) * 22.f);
            // Radial vignette
            const float cu = u - 0.5f, cv = v - 0.5f;
            const float vig = std::max (0.f, 1.f - (cu * cu + cv * cv) * 3.8f);
            // Purple / blue / navy plasma
            const float r  = (0.04f + 0.22f * p1 * p2 + 0.42f * puckGlow + 0.12f * p3 * energy) * vig;
            const float gr = (0.02f + 0.07f * p2  + 0.20f * puckGlow + 0.05f * energy * p1) * vig;
            const float b  = (0.14f + 0.50f * (p1 * 0.6f + p2 * 0.4f)
                              + 0.58f * puckGlow + 0.28f * p3) * vig;
            bd.setPixelColour (i, j, Colour::fromFloatRGBA (
                jlimit (0.f, 1.f, r), jlimit (0.f, 1.f, gr), jlimit (0.f, 1.f, b), 1.f));
        }
    }

    LunaFXProcessor& proc;
    RangedAudioParameter *px, *py, *pe, *pTouch, *pSpring, *pByp;
    std::atomic<float> *nChoice[16], *nEnable[16]; // cached to avoid per-frame string lookup crash
    Image img;
    float phase = 0.f;
    bool touchEngaged = false, gestureOpen = false;
};

// ======================================================================================
// Left side readout — replaced by 16-node rim display in CircleXYPad.
// Kept as a stub so any remaining instantiation compiles cleanly.
class CornerReadout : public Component
{
public:
    CornerReadout (LunaFXProcessor& /*p*/, int /*cornerIndex*/) {}
    void update() {}
    void paint (Graphics&) override {}
};

// ======================================================================================
// Pill: power glyph + name + activity bars. Bound to a randomizer-pool flag.
class PillToggle : public Component
{
public:
    PillToggle (LunaFXProcessor& p, const String& paramId, const String& text, Colour c)
        : pr (p.apvts.getParameter (paramId)), label (text), col (c)
    {
        setMouseCursor (MouseCursor::PointingHandCursor);
    }
    void setActivity (float a) { a = jlimit (0.f, 1.f, a); if (std::abs (a - activity) > 0.01f) { activity = a; repaint(); } }

    void paint (Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.f);
        const bool on = isOn (pr);
        g.setColour (cPanel().withAlpha (0.92f)); g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
        g.setColour (on ? col.withAlpha (0.55f) : cLine()); g.drawRoundedRectangle (b, b.getHeight() * 0.5f, 1.3f);

        // power glyph
        const auto c = Point<float> (b.getX() + b.getHeight() * 0.5f, b.getCentreY());
        const float r = 6.f;
        Path pw; pw.addCentredArc (c.x, c.y, r, r, 0.f, 0.65f, MathConstants<float>::twoPi - 0.65f, true);
        pw.startNewSubPath (c.x, c.y - r - 1.f); pw.lineTo (c.x, c.y - 1.f);
        g.setColour (on ? col : cMuted().withAlpha (0.6f));
        g.strokePath (pw, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));

        g.setColour (on ? cText() : cMuted().withAlpha (0.7f)); g.setFont (14.f);
        g.drawText (label, b.withTrimmedLeft (b.getHeight()).withTrimmedRight (46.f), Justification::centred);

        // two little level bars
        for (int k = 0; k < 2; ++k)
        {
            const float bh = 16.f, bw = 4.f;
            const float x = b.getRight() - 40.f + (float) k * 9.f, y = b.getCentreY() - bh * 0.5f;
            g.setColour (cLine()); g.fillRoundedRectangle (x, y, bw, bh, 2.f);
            const float lv = (k == 0 ? activity : activity * 0.7f) * bh;
            g.setColour (on ? col : cMuted().withAlpha (0.4f)); g.fillRoundedRectangle (x, y + bh - lv, bw, lv, 2.f);
        }
    }
    void mouseDown (const MouseEvent&) override { setBool (pr, ! isOn (pr)); repaint(); }

private:
    RangedAudioParameter* pr;
    String label;
    Colour col;
    float activity = 0.f;
};

// ======================================================================================
// Big "ACCENT" footswitch (latch / momentary). Click the small label to flip the mode.
class AccentButton : public Component
{
public:
    explicit AccentButton (LunaFXProcessor& p) : proc (p)
    {
        pe = p.apvts.getParameter ("engage"); pm = p.apvts.getParameter ("engMode");
        setMouseCursor (MouseCursor::PointingHandCursor);
    }
    Rectangle<float> discArea() const { auto b = getLocalBounds().toFloat(); b.removeFromBottom (24.f); return b; }

    void paint (Graphics& g) override
    {
        auto d = discArea(); const float s = jmin (d.getWidth(), d.getHeight());
        const auto c = d.withSizeKeepingCentre (s, s).reduced (6.f);
        const float e = proc.uiEngage.load();
        if (e > 0.01f)
        {
            g.setGradientFill (ColourGradient (cAccent().withAlpha (0.6f * e), c.getCentreX(), c.getCentreY(), Colours::transparentBlack, c.getCentreX() + s * 0.8f, c.getCentreY(), true));
            g.fillEllipse (c.expanded (s * 0.3f));
        }
        g.setColour (cPanel()); g.fillEllipse (c);
        g.setColour (e > 0.5f ? cAccent2() : cLine().brighter (0.3f)); g.drawEllipse (c, 3.f);
        g.setColour (e > 0.5f ? cText() : cMuted()); g.setFont (15.f);
        g.drawText ("ACCENT", c, Justification::centred);

        g.setColour (cMuted()); g.setFont (12.f);
        g.drawText (isOn (pm) ? "HOLD" : "LATCH", getLocalBounds().removeFromBottom (24), Justification::centred);
    }
    void mouseDown (const MouseEvent& e) override
    {
        if (e.position.y > (float) getHeight() - 24.f) { setChoice (pm, isOn (pm) ? 0 : 1); repaint(); return; }
        if (isOn (pm)) { pe->beginChangeGesture(); pe->setValueNotifyingHost (1.f); held = true; }
        else           { pe->beginChangeGesture(); pe->setValueNotifyingHost (isOn (pe) ? 0.f : 1.f); pe->endChangeGesture(); }
    }
    void mouseUp (const MouseEvent&) override
    {
        if (held) { pe->setValueNotifyingHost (0.f); pe->endChangeGesture(); held = false; }
    }
private:
    LunaFXProcessor& proc;
    RangedAudioParameter *pe, *pm;
    bool held = false;
};

// ======================================================================================
// Momentary panic/kill: one rising edge clears the current block and DSP delay buffers.
class KillButton : public Component
{
public:
    explicit KillButton (LunaFXProcessor& p) : param (p.apvts.getParameter ("kill"))
    { setMouseCursor (MouseCursor::PointingHandCursor); }
    void paint (Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat().reduced (1.f);
        const bool pressed = isOn (param);
        const Colour col = pressed ? cWarn() : cPanel();
        g.setColour (col); g.fillRoundedRectangle (b, 7.f);
        g.setColour (pressed ? cWarn() : cLine().brighter (0.2f)); g.drawRoundedRectangle (b, 7.f, 1.2f);
        g.setColour (pressed ? cBg() : cText()); g.setFont (12.f);
        g.drawText ("KILL", b, Justification::centred);
    }
    void mouseDown (const MouseEvent&) override
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (1.f));
        repaint();
    }
    void mouseUp (const MouseEvent&) override
    {
        param->setValueNotifyingHost (param->convertTo0to1 (0.f));
        param->endChangeGesture();
        repaint();
    }
private:
    RangedAudioParameter* param;
};

// ======================================================================================
class BypassButton : public Component
{
public:
    explicit BypassButton (LunaFXProcessor& p) : pb (p.apvts.getParameter ("bypass")) { setMouseCursor (MouseCursor::PointingHandCursor); }

    void paint (Graphics& g) override
    {
        const bool bypassed = isOn (pb);
        auto b = getLocalBounds().toFloat().reduced (1.f);
        const float rad = b.getHeight() * 0.5f;
        g.setColour (bypassed ? cWarn() : cPanel()); g.fillRoundedRectangle (b, rad);
        g.setColour (bypassed ? cWarn() : cLine().brighter (0.3f)); g.drawRoundedRectangle (b, rad, 1.4f);

        const auto c = Point<float> (b.getX() + rad + 2.f, b.getCentreY());
        const Colour ic = bypassed ? Colour (0xff1c0e03) : cAccent2();
        Path pw; pw.addCentredArc (c.x, c.y, 6.5f, 6.5f, 0.f, 0.65f, MathConstants<float>::twoPi - 0.65f, true);
        pw.startNewSubPath (c.x, c.y - 8.f); pw.lineTo (c.x, c.y - 1.f);
        g.setColour (ic); g.strokePath (pw, PathStrokeType (2.f, PathStrokeType::curved, PathStrokeType::rounded));

        g.setFont (13.f);
        g.setColour (bypassed ? Colour (0xff1c0e03) : cText());
        g.drawText (bypassed ? "BYPASSED" : "BYPASS", b.withTrimmedLeft (rad * 2.f + 4.f), Justification::centredLeft);
    }
    void mouseDown (const MouseEvent&) override { setBool (pb, ! isOn (pb)); repaint(); }
private:
    RangedAudioParameter* pb;
};

// ======================================================================================
class IconButton : public Button
{
public:
    enum Glyph { Shuffle, Prev, Next };
    explicit IconButton (Glyph gl) : Button (""), glyph (gl) { setMouseCursor (MouseCursor::PointingHandCursor); }

    void paintButton (Graphics& g, bool over, bool down) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.f);
        g.setColour (down ? cLine().brighter (0.25f) : (over ? cLine().brighter (0.1f) : Colours::transparentBlack));
        g.fillRoundedRectangle (b, 8.f);
        g.setColour (cLine().brighter (0.35f)); g.drawRoundedRectangle (b, 8.f, 1.3f);
        g.setColour (cText());
        const auto c = b.getCentre(); const float s = jmin (b.getWidth(), b.getHeight()) * 0.2f;
        if (glyph == Shuffle)
        {
            Path p;
            p.addArrow (Line<float> (c.x - s * 1.4f, c.y + s, c.x + s * 1.4f, c.y - s), 1.6f, 7.f, 6.f);
            p.addArrow (Line<float> (c.x - s * 1.4f, c.y - s, c.x + s * 1.4f, c.y + s), 1.6f, 7.f, 6.f);
            g.fillPath (p);
        }
        else
        {
            const float d = glyph == Prev ? -1.f : 1.f;
            Path p; p.startNewSubPath (c.x - d * s * 0.6f, c.y - s); p.lineTo (c.x + d * s * 0.6f, c.y); p.lineTo (c.x - d * s * 0.6f, c.y + s);
            g.strokePath (p, PathStrokeType (1.9f, PathStrokeType::curved, PathStrokeType::rounded));
        }
    }
private:
    Glyph glyph;
};

// ======================================================================================
class NavTab : public Component
{
public:
    NavTab (int iconType, const String& text) : icon (iconType), label (text) { setMouseCursor (MouseCursor::PointingHandCursor); }
    std::function<void()> onClick;
    bool selected = false;

    void paint (Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        if (selected) { g.setColour (cAccent()); g.fillRect (0.f, 6.f, 2.f, b.getHeight() - 12.f); }
        const auto c = Point<float> (b.getCentreX(), 26.f);
        g.setColour (selected ? cText() : cMuted());
        const PathStrokeType st (1.8f, PathStrokeType::curved, PathStrokeType::rounded);
        if (icon == 0)        // play: ring + dot
        {
            g.drawEllipse (c.x - 12.f, c.y - 12.f, 24.f, 24.f, 1.8f);
            g.fillEllipse (c.x + 3.f, c.y - 6.f, 4.f, 4.f); g.fillEllipse (c.x - 5.f, c.y + 3.f, 5.f, 5.f);
        }
        else if (icon == 1)   // rack: three faders
        {
            for (int i = 0; i < 3; ++i)
            {
                const float x = c.x - 9.f + (float) i * 9.f;
                Path p; p.startNewSubPath (x, c.y - 11.f); p.lineTo (x, c.y + 11.f); g.strokePath (p, st);
                g.fillRoundedRectangle (x - 4.f, c.y - 4.f + (float) ((i * 5) % 9) - 3.f, 8.f, 4.f, 1.5f);
            }
        }
        else                  // setup: sliders (horizontal)
        {
            for (int i = 0; i < 3; ++i)
            {
                const float y = c.y - 8.f + (float) i * 8.f;
                Path p; p.startNewSubPath (c.x - 12.f, y); p.lineTo (c.x + 12.f, y); g.strokePath (p, st);
                g.fillEllipse (c.x - 6.f + (float) ((i * 9) % 13), y - 3.f, 6.f, 6.f);
            }
        }
        g.setFont (12.f);
        g.drawText (label, getLocalBounds().removeFromBottom (24), Justification::centred);
    }
    void mouseDown (const MouseEvent&) override { if (onClick) onClick(); }
private:
    int icon; String label;
};

// preset name in the top bar, click for the list
class PresetLabel : public Component
{
public:
    explicit PresetLabel (LunaFXProcessor& p) : proc (p) { setMouseCursor (MouseCursor::PointingHandCursor); }
    void paint (Graphics& g) override
    {
        g.setColour (cText());
        g.setFont (15.f);
        g.drawText (String (proc.getProgramName (proc.getCurrentProgram())).toUpperCase(), getLocalBounds(), Justification::centred);
    }
    void mouseDown (const MouseEvent&) override
    {
        PopupMenu m;
        for (int i = 0; i < proc.getNumPrograms(); ++i) m.addItem (i + 1, proc.getProgramName (i), true, i == proc.getCurrentProgram());
        Component::SafePointer<PresetLabel> sp (this);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [sp] (int r)
        {
            if (sp != nullptr && r > 0) { sp->proc.setCurrentProgram (r - 1); sp->repaint(); }
        });
    }
private:
    LunaFXProcessor& proc;
};
} // namespace ui
