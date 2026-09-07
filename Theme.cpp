#include "Theme.h"

#include <QApplication>
#include <QByteArray>
#include <QFile>
#include <QFontDatabase>
#include <QPainter>
#include <QPixmap>
#include <QSettings>
#include <QStyle>
#include <QSvgRenderer>
#include <QWidget>
#include <QtMath>

// ---------------------------------------------------------------------------------------------
// Scale
// ---------------------------------------------------------------------------------------------
namespace {
double g_scale = 1.0;
bool g_motion = true;

QString g_uiFamily;
QString g_monoFamily;
QString g_displayFamily;
QString g_gameFamily;      // set once the archives are mounted; wins over the bundled fallback
}

void ui::setScale(double s)
{
  if (s < 1.0) s = 1.0;
  if (s > 3.0) s = 3.0;
  // Quarter steps. Anything finer produces half-pixel borders that Qt rounds inconsistently
  // between the stylesheet (integers) and the layout (floats), which shows up as a 1px jitter
  // along every panel edge.
  g_scale = qRound(s * 4.0) / 4.0;
}

double ui::scale() { return g_scale; }

void fx::setEnabled(bool on) { g_motion = on; }
bool fx::enabled() { return g_motion; }

// ---------------------------------------------------------------------------------------------
// Fonts
// ---------------------------------------------------------------------------------------------
namespace {

// Loads one face and returns the family name the font database ended up with -- which is not
// always the file name, and is the only string a QFont lookup will match.
QString addFont(const QString& resource)
{
  const int id = QFontDatabase::addApplicationFont(resource);
  if (id < 0)
    return QString();
  const QStringList fams = QFontDatabase::applicationFontFamilies(id);
  return fams.isEmpty() ? QString() : fams.first();
}

// First installed family from a list, or an empty string. Used for the fallbacks, so a machine
// without the bundled resources still gets a sane face instead of whatever Qt defaults to.
QString firstInstalled(const QStringList& candidates)
{
  const QStringList have = QFontDatabase().families();
  for (const QString& f : candidates)
    if (have.contains(f))
      return f;
  return QString();
}

}  // namespace

bool typo::loadFonts()
{
  // Inter carries the interface. Three static instances: Qt 5.13 cannot select a variable
  // font's weight axis, so a variable file would render at one weight for all three roles.
  const QString regular  = addFont(":/fonts/Inter-Regular.ttf");
  const QString medium   = addFont(":/fonts/Inter-Medium.ttf");
  const QString semibold = addFont(":/fonts/Inter-SemiBold.ttf");
  const QString mono     = addFont(":/fonts/IBMPlexMono-Regular.ttf");
  const QString display  = addFont(":/fonts/Cinzel-Regular.ttf");

  // All three Inter files report the same family ("Inter"); the weight is selected through
  // QFont::setWeight, which the database resolves to the matching file.
  g_uiFamily = regular.isEmpty() ? firstInstalled({"Segoe UI Variable Text", "Segoe UI"}) : regular;
  g_monoFamily = mono.isEmpty() ? firstInstalled({"Consolas", "Courier New"}) : mono;
  g_displayFamily = display.isEmpty() ? firstInstalled({"Georgia", "Times New Roman"}) : display;

  if (g_uiFamily.isEmpty()) g_uiFamily = QStringLiteral("sans-serif");
  if (g_monoFamily.isEmpty()) g_monoFamily = QStringLiteral("monospace");
  if (g_displayFamily.isEmpty()) g_displayFamily = QStringLiteral("serif");

  return !regular.isEmpty() && !medium.isEmpty() && !semibold.isEmpty()
         && !mono.isEmpty() && !display.isEmpty();
}

QString typo::uiFamily() { return g_uiFamily.isEmpty() ? QStringLiteral("Segoe UI") : g_uiFamily; }
QString typo::monoFamily() { return g_monoFamily.isEmpty() ? QStringLiteral("Consolas") : g_monoFamily; }

QString typo::displayFamily()
{
  if (!g_gameFamily.isEmpty())
    return g_gameFamily;
  return g_displayFamily.isEmpty() ? QStringLiteral("Georgia") : g_displayFamily;
}

void typo::setDisplayFamily(const QString& family) { g_gameFamily = family; }

QFont typo::font(Style s)
{
  QFont f(uiFamily());
  f.setStyleStrategy(QFont::PreferAntialias);
  switch (s) {
    case Caption:
      f.setPixelSize(ui::px(11));
      f.setWeight(QFont::Medium);
      f.setCapitalization(QFont::AllUppercase);
      f.setLetterSpacing(QFont::AbsoluteSpacing, ui::px(1) * 0.5);
      break;
    case Small:
      f.setPixelSize(ui::px(12));
      break;
    case Body:
      f.setPixelSize(ui::px(13));
      break;
    case Strong:
      f.setPixelSize(ui::px(13));
      f.setWeight(QFont::Medium);
      break;
    case Title:
      f.setPixelSize(ui::px(15));
      f.setWeight(QFont::DemiBold);
      break;
    case Mono:
      f.setFamily(monoFamily());
      f.setPixelSize(ui::px(12));
      break;
    case Wordmark:
      f.setFamily(displayFamily());
      f.setPixelSize(ui::px(15));
      f.setWeight(QFont::DemiBold);
      f.setLetterSpacing(QFont::AbsoluteSpacing, ui::px(1) * 1.2);
      break;
    case Display:
      f.setFamily(displayFamily());
      f.setPixelSize(ui::px(28));
      f.setWeight(QFont::DemiBold);
      f.setLetterSpacing(QFont::AbsoluteSpacing, ui::px(2));
      break;
  }
  return f;
}

// ---------------------------------------------------------------------------------------------
// Icons
// ---------------------------------------------------------------------------------------------
QIcon Theme::icon(const QString& name, const QColor& colour)
{
  // Lucide draws with stroke="currentColor", which Qt's SVG renderer does not resolve -- there
  // is no cascade and no CSS colour inheritance in the tiny profile. Substituting the literal
  // colour in the source before handing it to the renderer is the whole trick, and it is why
  // the icons can be recoloured per state without shipping one file per colour.
  QFile f(QStringLiteral(":/icons/%1.svg").arg(name));
  if (!f.open(QIODevice::ReadOnly))
    return QIcon();
  QByteArray svg = f.readAll();
  svg.replace("currentColor", colour.name(QColor::HexRgb).toUtf8());

  QSvgRenderer renderer(svg);
  if (!renderer.isValid())
    return QIcon();

  QIcon out;
  // Three sizes, each rendered from the vector at the scaled pixel size rather than scaled
  // after the fact, so the 2px strokes stay crisp at 150 % and 200 %.
  for (int base : {16, 20, 24}) {
    const int px = ui::px(base);
    QPixmap pm(px, px);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    renderer.render(&p, QRectF(0, 0, px, px));
    p.end();
    out.addPixmap(pm, QIcon::Normal);
  }
  return out;
}

// ---------------------------------------------------------------------------------------------
// Palette
// ---------------------------------------------------------------------------------------------
QPalette Theme::palette()
{
  QPalette p;
  const QColor panel(tok::bgPanel), well(tok::bgWell), raised(tok::bgRaised);
  const QColor text(tok::fgText), soft(tok::fgSoft), disabled(237, 237, 240, 97);

  p.setColor(QPalette::Window, panel);
  p.setColor(QPalette::WindowText, text);
  p.setColor(QPalette::Base, well);
  p.setColor(QPalette::AlternateBase, QColor(tok::bgPanel).lighter(106));
  p.setColor(QPalette::Text, text);
  p.setColor(QPalette::Button, raised);
  p.setColor(QPalette::ButtonText, text);
  p.setColor(QPalette::BrightText, QColor(tok::danger));
  p.setColor(QPalette::Highlight, QColor(tok::accentFill));
  p.setColor(QPalette::HighlightedText, QColor(tok::onAccent));
  p.setColor(QPalette::Link, QColor(tok::accentText));
  p.setColor(QPalette::LinkVisited, QColor(tok::accentText));
  p.setColor(QPalette::ToolTipBase, raised);
  p.setColor(QPalette::ToolTipText, text);
  p.setColor(QPalette::PlaceholderText, QColor(tok::fgMuted));
  p.setColor(QPalette::Mid, QColor(tok::lineBorder));
  p.setColor(QPalette::Dark, QColor(tok::bgVoid));
  p.setColor(QPalette::Shadow, QColor(tok::bgVoid));

  // Fusion takes its greyed-out colours from the palette and ignores the stylesheet for them,
  // which is why "disabled" looked like four different things before.
  for (QPalette::ColorRole r : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText,
                                QPalette::HighlightedText})
    p.setColor(QPalette::Disabled, r, disabled);
  p.setColor(QPalette::Disabled, QPalette::Base, well);
  p.setColor(QPalette::Disabled, QPalette::Button, QColor(tok::bgPressed));
  p.setColor(QPalette::Disabled, QPalette::Highlight, QColor(tok::bgRaised));
  Q_UNUSED(soft);
  return p;
}

// ---------------------------------------------------------------------------------------------
// The stylesheet
// ---------------------------------------------------------------------------------------------
namespace {

// Qt's stylesheet syntax has no variables, so the tokens are interpolated. %1-style positional
// arguments would be unreadable at this length; a plain search and replace on named keys keeps
// the sheet legible and the diff meaningful.
QString expand(QString s)
{
  struct KV { const char* key; QString value; };
  const QList<KV> map = {
    {"@void", tok::bgVoid}, {"@chrome", tok::bgChrome}, {"@well", tok::bgWell},
    {"@panel", tok::bgPanel}, {"@raised", tok::bgRaised}, {"@raisedHover", tok::bgRaisedHover},
    {"@pressed", tok::bgPressed},
    {"@ovHover", tok::ovHover}, {"@ovPressed", tok::ovPressed}, {"@ovAlt", tok::ovAlt},
    {"@ovTop", tok::ovTop},
    {"@hair", tok::lineHair}, {"@border", tok::lineBorder}, {"@strong", tok::lineStrong},
    {"@text", tok::fgText}, {"@soft", tok::fgSoft}, {"@muted", tok::fgMuted},
    {"@dim", tok::fgDim}, {"@disabled", tok::fgDisabled},
    {"@accent", tok::accent}, {"@accentFill", tok::accentFill},
    {"@accentFillHover", tok::accentFillHover}, {"@accentFillPress", tok::accentFillPress},
    {"@accentText", tok::accentText}, {"@accentTint", tok::accentTint},
    {"@onAccent", tok::onAccent},
    {"@ok", tok::ok}, {"@warn", tok::warn}, {"@danger", tok::danger},
    {"@dangerTint", tok::dangerTint},
    // measures
    {"@rCtl", QString::number(met::rCtl()) + "px"},
    {"@rCard", QString::number(met::rCard()) + "px"},
    {"@hRow", QString::number(met::hRow()) + "px"},
    {"@hCtl", QString::number(met::hCtl()) + "px"},
    {"@hCta", QString::number(met::hCta()) + "px"},
    {"@hIcon", QString::number(met::hIcon()) + "px"},
    {"@spTight", QString::number(met::sp(met::Tight)) + "px"},
    {"@spSnug", QString::number(met::sp(met::Snug)) + "px"},
    {"@spGap", QString::number(met::sp(met::Gap)) + "px"},
    {"@spPad", QString::number(met::sp(met::Pad)) + "px"},
    {"@spEdge", QString::number(met::sp(met::Edge)) + "px"},
    {"@fsCaption", QString::number(ui::px(11)) + "px"},
    {"@fsSmall", QString::number(ui::px(12)) + "px"},
    {"@fsBody", QString::number(ui::px(13)) + "px"},
    {"@fsTitle", QString::number(ui::px(15)) + "px"},
    {"@uiFamily", typo::uiFamily()},
    {"@monoFamily", typo::monoFamily()},
    {"@scrollW", QString::number(ui::px(10)) + "px"},
  };
  // Longest key first, so "@accentFillHover" is not eaten by "@accentFill".
  QList<KV> sorted = map;
  std::sort(sorted.begin(), sorted.end(), [](const KV& a, const KV& b) {
    return qstrlen(a.key) > qstrlen(b.key);
  });
  for (const KV& kv : sorted)
    s.replace(QLatin1String(kv.key), kv.value);
  return s;
}

}  // namespace

QString Theme::sheet()
{
  // One sheet for the whole application. Read it as a spec: each block says what a family of
  // widgets looks like in every state, and nothing outside this function may set a colour.
  //
  // The load-bearing Qt rule: touching ONE sub-control of a complex widget (a combo's arrow, a
  // scrollbar's handle, a spin box's buttons) makes Qt stop drawing the whole widget natively.
  // Every family below is therefore styled completely, including the parts that would otherwise
  // be invisible -- that omission is what made the old panels look half-native.
  static const char* kSheet = R"QSS(
/* ---- ground ------------------------------------------------------------------------ */
/* No background on the universal rule. "QWidget { background: ... }" forces EVERY widget to
   paint a filled rectangle, including the plain containers that only exist to hold a layout --
   which is how a transparent group inside the title bar became a visibly lighter box on the
   bar's own gradient. Backgrounds are painted by role; everything else shows what is behind
   it, which is what a container should do. */
QWidget            { color: @text; font-family: "@uiFamily"; font-size: @fsBody; }
QWidget:disabled   { color: @disabled; }
QDialog, QMessageBox, QFileDialog { background: @panel; }
QWidget[role="chrome"] { background: @chrome; }
QWidget[role="well"]   { background: @well; }
QWidget[role="panel"]  { background: @panel; }
QWidget[role="void"]   { background: @void; }
/* Anything floating over the GL canvas. GLHost is a native child window, so a HUD element has
   to be native too to sit above it -- and a native window has its own backing store, which
   means it MUST paint an opaque background or it shows whatever was in that memory before.
   The colour matches the GL clear colour so the pill reads as part of the viewport. */
QWidget[role="overlay"], QFrame[role="overlay"] {
  background: @void; border: 1px solid @border; border-radius: @rCtl;
}
QFrame[role="hline"]   { background: @hair; border: none; max-height: 1px; min-height: 1px; }
QFrame[role="vline"]   { background: @hair; border: none; max-width: 1px; min-width: 1px; }

/* ---- type roles --------------------------------------------------------------------- */
QLabel                    { background: transparent; }
QLabel[role="section"]    { color: @muted; font-size: @fsCaption; font-weight: 500; }
QLabel[role="caption"]    { color: @muted; font-size: @fsCaption; }
QLabel[role="hint"]       { color: @muted; font-size: @fsSmall; }
QLabel[role="value"]      { color: @text; }
QLabel[role="title"]      { color: @text; font-size: @fsTitle; font-weight: 600; }
QLabel[role="mono"]       { color: @muted; font-family: "@monoFamily"; font-size: @fsSmall; }
QLabel[role="accent"]     { color: @accentText; }
QLabel[state="ok"]        { color: @ok; }
QLabel[state="warn"]      { color: @warn; }
QLabel[state="error"]     { color: @danger; }

/* ---- buttons ------------------------------------------------------------------------- */
QPushButton {
  background: @raised; color: @text; border: 1px solid @border; border-top-color: @ovTop;
  border-radius: @rCtl; padding: 0 @spPad; min-height: @hCtl; font-weight: 500;
}
QPushButton:hover    { background: @raisedHover; border-color: @strong; }
QPushButton:pressed  { background: @pressed; }
QPushButton:disabled { background: @pressed; border-color: @border; color: @disabled; }
QPushButton[variant="primary"] {
  background: @accentFill; color: @onAccent; border: 1px solid @accentFillHover; border-top-color: @ovTop;
}
QPushButton[variant="primary"]:hover   { background: @accentFillHover; }
QPushButton[variant="primary"]:pressed { background: @accentFillPress; }
QPushButton[variant="primary"]:disabled{ background: @pressed; color: @disabled; border-color: @border; }
QPushButton[variant="quiet"] { background: transparent; border-color: transparent; color: @soft; }
QPushButton[variant="quiet"]:hover { background: @ovHover; color: @text; }
QPushButton[variant="danger"]:hover { background: @dangerTint; border-color: @danger; color: @danger; }

/* ---- tool buttons: the tool bar, the window buttons, every icon action ---------------- */
QToolButton {
  background: transparent; color: @soft; border: 1px solid transparent; border-radius: @rCtl;
  padding: 0 @spGap; min-height: @hRow; font-weight: 500;
}
QToolButton:hover   { background: @ovHover; color: @text; }
QToolButton:pressed { background: @ovPressed; }
QToolButton:checked { background: @accentTint; color: @accentText; border-color: transparent; }
QToolButton:disabled{ color: @disabled; }
QToolButton::menu-indicator { image: none; }
QToolButton[role="icon"] { padding: 0; min-width: 0; }

/* A segmented group: buttons share one outline and the inner edges are square, so the group
   reads as one control instead of three separate ones. */
QToolButton[segment] { background: @raised; border: 1px solid @border; border-top-color: @ovTop; border-radius: 0; }
QToolButton[segment="first"] { border-top-left-radius: @rCtl; border-bottom-left-radius: @rCtl; }
QToolButton[segment="last"]  { border-top-right-radius: @rCtl; border-bottom-right-radius: @rCtl; border-left: none; }
QToolButton[segment="mid"]   { border-left: none; }
QToolButton[segment="only"]  { border-radius: @rCtl; }
QToolButton[segment]:hover   { background: @raisedHover; }
QToolButton[segment]:checked { background: @accentTint; color: @accentText; }
QToolButton[role="window"]   { border-radius: 0; padding: 0; color: @muted; }
QToolButton[role="window"]:hover { background: @ovHover; color: @text; }
QToolButton[role="close"]:hover { background: @danger; color: #ffffff; }
QToolButton[role="hud"] { background: @raised; border: 1px solid @border; }
QToolButton[role="tab"] {
  background: transparent; border: none; border-bottom: 2px solid transparent;
  border-radius: 0; color: @muted; min-height: @hCta; padding: 0 @spPad;
}
QToolButton[role="tab"]:hover   { color: @text; background: @ovHover; }
QToolButton[role="tab"]:checked { color: @text; border-bottom-color: @accent; background: transparent; }

/* ---- keyboard focus ------------------------------------------------------------------ */
/* Only when the widget was reached with the keyboard: MidnightStyle sets kbfocus, so clicking
   a button does not leave a ring behind on it. */
*[kbfocus="true"] { border: 1px solid @accent; }

/* ---- inputs ---------------------------------------------------------------------------- */
QLineEdit, QPlainTextEdit, QTextEdit {
  background: @well; color: @text; border: 1px solid @border; border-radius: @rCtl;
  padding: 0 @spGap; min-height: @hCtl; selection-background-color: @accentFill;
  selection-color: @onAccent;
}
QLineEdit:hover { border-color: @strong; }
QLineEdit:focus { border-color: @accent; }
QLineEdit:disabled { color: @disabled; }

QComboBox {
  background: @raised; color: @text; border: 1px solid @border; border-top-color: @ovTop;
  border-radius: @rCtl; padding: 0 @spGap; min-height: @hCtl;
}
QComboBox:hover   { background: @raisedHover; border-color: @strong; }
QComboBox:focus   { border-color: @accent; }
QComboBox:disabled{ background: @pressed; color: @disabled; }
QComboBox::drop-down { border: none; width: 22px; }
QComboBox::down-arrow { image: url(:/icons/chevron-down.svg); width: 14px; height: 14px; }
QComboBox::down-arrow:disabled { image: none; }
QComboBox QAbstractItemView {
  background: @raised; border: 1px solid @strong; border-radius: @rCtl; padding: @spSnug;
  outline: none; selection-background-color: @accentTint; selection-color: @accentText;
}
QComboBox QAbstractItemView::item { min-height: @hRow; padding: 0 @spGap; border-radius: @rCtl; }
QComboBox QAbstractItemView::item:hover { background: @ovHover; }

QSpinBox, QDoubleSpinBox {
  background: @well; color: @text; border: 1px solid @border; border-radius: @rCtl;
  padding: 0 @spSnug 0 @spGap; min-height: @hCtl;
  selection-background-color: @accentFill; selection-color: @onAccent;
}
QSpinBox:hover, QDoubleSpinBox:hover { border-color: @strong; }
QSpinBox:focus, QDoubleSpinBox:focus { border-color: @accent; }
QSpinBox::up-button, QDoubleSpinBox::up-button,
QSpinBox::down-button, QDoubleSpinBox::down-button {
  background: transparent; border: none; width: 16px; subcontrol-origin: border;
}
QSpinBox::up-button, QDoubleSpinBox::up-button { subcontrol-position: top right; }
QSpinBox::down-button, QDoubleSpinBox::down-button { subcontrol-position: bottom right; }
QSpinBox::up-arrow, QDoubleSpinBox::up-arrow { image: url(:/icons/chevron-up.svg); width: 10px; height: 10px; }
QSpinBox::down-arrow, QDoubleSpinBox::down-arrow { image: url(:/icons/chevron-down.svg); width: 10px; height: 10px; }
QSpinBox::up-button:hover, QSpinBox::down-button:hover,
QDoubleSpinBox::up-button:hover, QDoubleSpinBox::down-button:hover { background: @ovHover; }

QCheckBox, QRadioButton { background: transparent; color: @soft; spacing: @spGap; min-height: @hIcon; }
QCheckBox:hover, QRadioButton:hover { color: @text; }
QCheckBox::indicator, QRadioButton::indicator {
  width: 15px; height: 15px; border: 1px solid @strong; background: @well;
}
QCheckBox::indicator  { border-radius: @rCtl; }
QRadioButton::indicator { border-radius: 8px; }
QCheckBox::indicator:hover, QRadioButton::indicator:hover { border-color: @accent; }
QCheckBox::indicator:checked {
  background: @accentFill; border-color: @accentFill; image: url(:/icons/check.svg);
}
QRadioButton::indicator:checked { background: @accentFill; border-color: @accentFill; }
QCheckBox::indicator:disabled, QRadioButton::indicator:disabled { border-color: @border; background: @pressed; }

/* ---- lists and trees --------------------------------------------------------------------- */
QAbstractItemView {
  background: @well; alternate-background-color: @ovAlt; color: @soft;
  border: 1px solid @border; border-radius: @rCtl; outline: none;
  selection-background-color: transparent; selection-color: @text;
}
QAbstractItemView[role="flush"] { border: none; border-radius: 0; background: @panel; }
QAbstractItemView::item {
  min-height: @hRow; padding: 0 @spGap; border: none; border-radius: @rCtl; color: @soft;
}
QAbstractItemView::item:hover    { background: @ovHover; color: @text; }
QAbstractItemView::item:selected { background: @accentTint; color: @text; }
/* The branch column is deliberately NOT styled: naming ::branch at all hands the whole
   indicator to QStyleSheetStyle, which then draws Qt's default arrows and ignores
   MidnightStyle::drawPrimitive -- and the chevrons disappear. */
QHeaderView::section {
  background: @panel; color: @muted; border: none; border-bottom: 1px solid @hair;
  padding: 0 @spGap; font-size: @fsCaption; font-weight: 500; min-height: @hRow;
}

/* ---- scrollbars ------------------------------------------------------------------------- */
/* All four sub-controls have to be named, or Fusion draws the steppers as native arrows on a
   grey trough beside the styled handle. */
QScrollBar:vertical   { background: transparent; width: @scrollW; margin: 0; }
QScrollBar:horizontal { background: transparent; height: @scrollW; margin: 0; }
QScrollBar::handle:vertical   { background: rgba(255,255,255,0.14); min-height: 28px; border-radius: 4px; margin: 2px; }
QScrollBar::handle:horizontal { background: rgba(255,255,255,0.14); min-width: 28px;  border-radius: 4px; margin: 2px; }
QScrollBar::handle:hover      { background: rgba(255,255,255,0.24); }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; background: none; border: none; }
QScrollBar::add-page, QScrollBar::sub-page { background: none; }
QScrollArea { background: transparent; border: none; }
QScrollArea > QWidget > QWidget { background: transparent; }

/* ---- menus ------------------------------------------------------------------------------- */
QMenuBar { background: transparent; color: @soft; }
QMenuBar::item { background: transparent; padding: @spSnug @spPad; border-radius: @rCtl; margin: 0 1px; }
QMenuBar::item:selected { background: @ovHover; color: @text; }
QMenuBar::item:pressed  { background: @raised; color: @text; }
/* Radius 4 and opaque, deliberately: a rounded popup needs a translucent window, and that
   loses the system's drop shadow -- the two cannot be had at once on Windows with Qt 5.13. */
QMenu {
  background: @raised; border: 1px solid @strong; border-radius: @rCtl; padding: @spSnug;
  color: @soft;
}
QMenu::item { padding: @spSnug @spEdge @spSnug @spPad; border-radius: @rCtl; min-height: @hRow; }
QMenu::item:selected { background: @ovHover; color: @text; }
QMenu::item:disabled { color: @disabled; }
QMenu::separator { height: 1px; background: @hair; margin: @spSnug @spGap; }
QMenu::icon { padding-left: @spGap; }

QToolTip {
  background: @raised; color: @text; border: 1px solid @strong; border-radius: @rCtl;
  padding: @spSnug @spGap; font-size: @fsSmall;
}

/* ---- sliders and progress ------------------------------------------------------------------ */
QSlider::groove:horizontal { background: @well; height: 4px; border-radius: 2px; }
QSlider::sub-page:horizontal { background: @accentFill; height: 4px; border-radius: 2px; }
QSlider::handle:horizontal {
  background: @text; width: 12px; height: 12px; margin: -5px 0; border-radius: 6px;
}
QSlider::handle:horizontal:hover { background: @onAccent; }
/* The scrubber: a groove, an accent fill up to the playhead, and a handle you can hit.
   The old one was a 24px accent-filled bar across the whole window -- the loudest object
   in the frame -- with a 3px handle nobody could grab. */
QSlider[role="scrub"]::groove:horizontal { background: @well; height: 4px; border-radius: 2px; }
QSlider[role="scrub"]::sub-page:horizontal { background: @accent; height: 4px; border-radius: 2px; }
QSlider[role="scrub"]::handle:horizontal {
  background: @accent; width: 4px; height: 16px; margin: -6px 0; border-radius: 2px;
}
QSlider[role="scrub"]::handle:horizontal:hover { background: @accentText; }
QProgressBar { background: @well; border: none; border-radius: 2px; height: 4px; text-align: center; }
QProgressBar::chunk { background: @accentFill; border-radius: 2px; }

/* ---- splitter --------------------------------------------------------------------------- */
QSplitter::handle { background: @void; }
QSplitter::handle:horizontal { width: 4px; }
QSplitter::handle:vertical   { height: 4px; }
QSplitter::handle:hover { background: @accentFill; }

/* ---- tabs (the few real QTabWidgets, e.g. in dialogs) ------------------------------------ */
QTabWidget::pane { border: 1px solid @border; border-radius: @rCtl; }
QTabBar::tab {
  background: transparent; color: @muted; padding: @spGap @spPad; border: none;
  border-bottom: 2px solid transparent;
}
QTabBar::tab:hover    { color: @text; }
QTabBar::tab:selected { color: @text; border-bottom-color: @accent; }
)QSS";

  return expand(QString::fromUtf8(kSheet));
}

void Theme::repolish(QWidget* w)
{
  if (!w)
    return;
  // A dynamic property changes which rules match, but Qt does not re-evaluate the stylesheet on
  // its own -- without this the property is set and nothing happens, which is the classic
  // "my [variant] selector does nothing" bug.
  w->style()->unpolish(w);
  w->style()->polish(w);
  w->update();
}
