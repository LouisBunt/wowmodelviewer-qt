#ifndef THEME_H
#define THEME_H

// The design system: colour, measure, type, motion — in one place, for the whole application.
//
// It used to be colour only, and only as strings; radii, paddings, row heights and font sizes
// were hardcoded at some forty call sites, each panel wrote its own stylesheet, and the fonts
// were named per widget in POINTS, which scale with the system DPI while the pixel-tall bars
// around them do not. That combination is what made the window feel "off": eight surfaces
// within nine luminance points, eight corner radii, three label-column widths, and no state on
// anything clickable.
//
// The rules this file encodes:
//   * Four grounds, not eight. Neighbours differ enough to be seen as different.
//   * Hover, pressed, zebra and gloss are ALPHA over the ground, never new opaque tokens.
//   * The accent has four jobs: focus ring, active tab, selection, one primary action.
//   * Sizes come from met::, type from typo::, and every number is multiplied by ui::scale().
//   * Colour that carries game meaning (item quality) lives in GameColours.h, not here.
//
// Everything is consumed through Theme::sheet() (one application stylesheet), Theme::palette()
// and Theme::font(); a widget that sets its own colours is a bug, and tools/qss-lint.ps1 says so.

#include <QColor>
#include <QFont>
#include <QIcon>
#include <QPalette>
#include <QString>

// --- colour tokens, by ROLE ---------------------------------------------------------------
// Named for what they are for, not what they look like: a redesign changes the value, never
// the call site. Neutral greys with a single point of violet in the panels, so the accent has
// a ground to sit on rather than floating over a dead grey.
namespace tok {

// Grounds, darkest first.
const char* const bgVoid    = "#050506";   // behind the model, splitter grooves, gaps
const char* const bgChrome  = "#0a0a0c";   // title bar, tool bar, status bar
const char* const bgWell    = "#0d0d10";   // recessed: trees, lists, inputs, scrubber groove
const char* const bgPanel   = "#121215";   // browser column, inspector column, timeline
const char* const bgRaised  = "#1b1b21";   // buttons, combos, menus, HUD
const char* const bgRaisedHover = "#24242a";
const char* const bgPressed = "#141417";

// Overlays. Alpha over whatever is underneath, so one value works on every ground.
const char* const ovHover   = "rgba(255,255,255,0.06)";
const char* const ovPressed = "rgba(0,0,0,0.20)";
const char* const ovAlt     = "rgba(255,255,255,0.025)";   // zebra rows
const char* const ovTop     = "rgba(255,255,255,0.09)";    // 1px top edge: the "gloss"

// Lines.
const char* const lineHair   = "rgba(255,255,255,0.07)";   // dividers
const char* const lineBorder = "#26262b";                  // inputs at rest, cards
const char* const lineStrong = "#34343a";                  // hover on inputs, floating surfaces

// Type. Contrast measured on bgPanel: text 16:1, soft 10:1, muted 5.6:1 (AA), dim 3.3:1.
const char* const fgText     = "#ededf0";
const char* const fgSoft     = "#b9b9c0";
const char* const fgMuted    = "#8c8c95";                  // smallest colour that may carry text
const char* const fgDim      = "#5f5f68";                  // NON-TEXT only: rules, inactive strokes
const char* const fgDisabled = "rgba(237,237,240,0.38)";

// Accent, split by job. The saturated tone draws lines; the darker one fills, because white on
// #a855f7 is 3.95:1 and fails, while white on #7c3aed is 5.7:1 and passes.
const char* const accent          = "#a855f7";   // focus ring, active tab, selection bar, playhead
const char* const accentFill      = "#7c3aed";   // filled primary action
const char* const accentFillHover = "#8b5cf6";
const char* const accentFillPress = "#6d28d9";
const char* const accentText      = "#c4b5fd";   // accent-coloured text and icons
const char* const accentTint      = "rgba(168,85,247,0.14)";   // selected row
const char* const accentGlow      = "rgba(168,85,247,0.35)";   // outer focus ring, splash halo
const char* const onAccent        = "#ffffff";

// Brand. The wordmark and nothing else.
const char* const brandWordmark  = "#d8c7f5";
const char* const brandWatermark = "rgba(216,199,245,0.05)";

// Signals. Never a row fill: a glyph, a dot, a 3px edge, or a 14% tint.
const char* const ok        = "#58c27a";
const char* const okTint    = "rgba(88,194,122,0.14)";
const char* const warn      = "#e0b341";
const char* const warnTint  = "rgba(224,179,65,0.14)";
const char* const danger    = "#e5484d";
const char* const dangerTint= "rgba(229,72,77,0.14)";

}  // namespace tok

// --- the one scaling factor ---------------------------------------------------------------
// Qt 5.13's AA_EnableHighDpiScaling rounds the device pixel ratio to a whole number (150% would
// become 2x; the rounding policy only arrives in 5.14), and GLHost reads the back buffer with
// glReadPixels(0, 0, width(), height()) — with a devicePixelRatio above 1 that captures a
// quarter of the frame. So the application does its own scaling instead: one factor, applied to
// every measure and every pixel font size, the way Blender scales its whole interface from one
// unit. --ui-scale pins it, so a screenshot taken on one machine matches another.
namespace ui {
void setScale(double s);          // clamped to 1.0 .. 3.0, rounded to a quarter
double scale();
inline int px(int v) { return (int)(v * scale() + 0.5); }
}  // namespace ui

// --- measures -----------------------------------------------------------------------------
// A 4px grid. Anything not on it is a mistake, not a nuance. The functions return SCALED
// pixels; the raw constants exist for arithmetic that has to happen before scaling.
namespace met {

// Spacing, by role rather than by number: 4 within a group, 8 between siblings, 12 between
// cards, 16 from a panel edge, 24 between sections, 32 below a heading.
enum Space { Tight = 2, Snug = 4, Gap = 8, Pad = 12, Edge = 16, Section = 24, Head = 32 };
inline int sp(Space s) { return ui::px((int)s); }

// Control heights. Five, and no others.
inline int hIcon() { return ui::px(24); }   // icon button hit area, minimum touch target
inline int hRow()  { return ui::px(28); }   // list and tree rows, small buttons
inline int hCtl()  { return ui::px(32); }   // inputs, combos, ordinary buttons
inline int hCta()  { return ui::px(36); }   // the primary action, tool bar
inline int hBar()  { return ui::px(40); }   // title bar
inline int hRow2() { return ui::px(44); }   // two-line row (an equipment slot)

// Radii: 4 for things in the page, 8 for the window and popovers, 0 where edges meet.
inline int rCtl()  { return ui::px(4); }
inline int rCard() { return ui::px(8); }

// Column limits for the splitter, in scaled pixels.
inline int browserMin() { return ui::px(240); }
inline int browserDef() { return ui::px(272); }
inline int browserMax() { return ui::px(420); }
inline int inspectorMin() { return ui::px(300); }
inline int inspectorDef() { return ui::px(340); }
inline int inspectorMax() { return ui::px(480); }
inline int canvasMin()    { return ui::px(560); }

}  // namespace met

// --- type ----------------------------------------------------------------------------------
// Pixel sizes, always. Three weights, no italics, nothing above 600. The families are bundled
// (Inter, IBM Plex Mono, Cinzel) so the layout is measured against the face the user actually
// sees — IBM Plex Sans was named everywhere but installed nowhere, and every fixed width in the
// old code was tuned against a Segoe UI substitution nobody intended.
namespace typo {

enum Style {
  Caption,    // 11/500 uppercase +0.5 — section titles, slot names, counters
  Small,      // 12/400 — status bar, shortcuts, second lines
  Body,       // 13/400 — the default: lists, labels, controls, menus
  Strong,     // 13/500 — buttons, tabs, item names, active segments
  Title,      // 15/600 — panel heads, empty-state headline, dialog titles
  Mono,       // 12/400 — numbers, ids, paths (tabular figures)
  Wordmark,   // 15 — MIDNIGHT, in the game's face when it is mounted, else Cinzel
  Display     // 28 — splash and about only
};

// Loads the bundled faces. Call once, before any widget exists; returns false when a face is
// missing from the resources, in which case the fallbacks below apply and the UI still works.
bool loadFonts();

// The face names actually resolved at runtime (bundled family, or the platform fallback).
QString uiFamily();      // Inter → Segoe UI Variable Text → Segoe UI → sans-serif
QString monoFamily();    // IBM Plex Mono → Consolas → monospace
QString displayFamily(); // the game's ornamental face when set, else Cinzel → Georgia

// Overrides the display family once the game archives are mounted (the ornamental face lives
// in the game data and is not available while the window is being built).
void setDisplayFamily(const QString& family);

QFont font(Style s);

}  // namespace typo

// --- motion ---------------------------------------------------------------------------------
// One duration for hover, one for everything else, and zero when the run is scripted or the
// user has switched animations off in Windows. Layout is never animated.
namespace fx {
void setEnabled(bool on);
bool enabled();
inline int quick() { return enabled() ? 100 : 0; }
inline int normal() { return enabled() ? 160 : 0; }
}  // namespace fx

// --- the stylesheet, the palette, the icons ---------------------------------------------------
namespace Theme {

// The whole application's stylesheet, built from the tokens above. Installed once on
// qApp; no widget carries its own. Variants are selected by dynamic properties, e.g.
//   button->setProperty("variant", "primary");   // QPushButton[variant="primary"]
//   label->setProperty("role", "section");       // QLabel[role="section"]
QString sheet();

// Fusion reads disabled colours from the palette, not from the stylesheet, so both are set.
QPalette palette();

// Re-applies the stylesheet to a widget after a dynamic property changed. Qt does not do this
// on its own: the property changes, the rule matches, and nothing repaints until this runs.
void repolish(QWidget* w);

// An icon from the bundled Lucide set (resources/icons/<name>.svg), recoloured for the three
// states. Returns a null icon for an unknown name — visibly missing rather than crashing.
QIcon icon(const QString& name, const QColor& colour = QColor(tok::fgSoft));

}  // namespace Theme

#endif  // THEME_H
