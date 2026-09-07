// Tests for the interface's building blocks.
//
// These are the pieces the whole redesign rests on, and each of them replaces something that
// failed silently: a label that clipped instead of eliding, a label column measured against a
// font the machine does not have, a stylesheet with colours written into it by hand. A silent
// failure needs a test more than a loud one does.
//
// Runs offscreen (QT_QPA_PLATFORM=offscreen), so it needs no display, no game data and no GL.
//
//   uitest
//
// Exit code = number of failing checks, so a plain shell/CI check works.
#include <cstdio>

#include <QApplication>
#include <QFontMetrics>
#include <QLabel>
#include <QRegularExpression>

#include "Theme.h"
#include "UiKit.h"

namespace {

int failures = 0;

#define CHECK(cond)                                                        \
  do {                                                                     \
    if (!(cond)) {                                                         \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
      failures++;                                                          \
    }                                                                      \
  } while (0)

// --- ElidedLabel ---------------------------------------------------------------------------
void testElidedLabel()
{
  const QString longName = QStringLiteral("Schulterstücke des ehrfürchtigen Wächters");

  ElidedLabel wide(longName);
  wide.setFont(typo::font(typo::Body));
  const int full = QFontMetrics(wide.font()).horizontalAdvance(longName);
  wide.resize(full + 40, 20);
  // Room to spare: nothing is hidden, so there must be no tooltip. A tooltip on every label
  // would make the ones that DO hide something indistinguishable.
  CHECK(!wide.isElided());
  CHECK(wide.toolTip().isEmpty());

  ElidedLabel narrow(longName);
  narrow.setFont(typo::font(typo::Body));
  narrow.resize(full / 3, 20);
  CHECK(narrow.isElided());
  CHECK(narrow.toolTip() == longName);      // the full string is recoverable

  // The tooltip suffix rides along with the full text, and shows on its own when nothing
  // is elided -- that is where the item id lives in the equipment rows.
  narrow.setTooltipSuffix(QStringLiteral("Item 18817"));
  CHECK(narrow.toolTip().contains(longName));
  CHECK(narrow.toolTip().contains(QStringLiteral("Item 18817")));
  wide.setTooltipSuffix(QStringLiteral("Item 18817"));
  CHECK(wide.toolTip() == QStringLiteral("Item 18817"));

  // A label may never be the reason a layout cannot shrink.
  CHECK(narrow.minimumSizeHint().width() == 0);

  // Empty text is not elided and carries no tooltip.
  ElidedLabel empty;
  empty.resize(100, 20);
  CHECK(!empty.isElided());
  CHECK(empty.toolTip().isEmpty());
}

// --- PropertyRow's measured label column -----------------------------------------------------
void testMeasuredColumn()
{
  // The real German option names from the character panel, including the ones that did not
  // fit in the old hardcoded 88 px.
  const QStringList labels = {
    QStringLiteral("Gesicht"), QStringLiteral("Hautfarbe"), QStringLiteral("Frisur"),
    QStringLiteral("Haarfarbe"), QStringLiteral("Koteletten"), QStringLiteral("Augenfarbe"),
    QStringLiteral("Tätowierung"), QStringLiteral("Gesichtsbehaarung")
  };
  const int col = PropertyRow::measureColumn(labels);
  CHECK(col >= ui::px(96));
  CHECK(col <= ui::px(140));

  // It has to be at least as wide as the longest label it was measured over, or the whole
  // exercise is pointless -- unless the clamp caps it, which is the deliberate limit.
  const QFontMetrics fm(typo::font(typo::Body));
  int widest = 0;
  for (const QString& l : labels)
    widest = qMax(widest, fm.horizontalAdvance(l));
  CHECK(col >= widest || col == ui::px(140));

  // A single short label still gets the minimum, so panels line up with each other.
  CHECK(PropertyRow::measureColumn({QStringLiteral("Bart")}) == ui::px(96));
  // An absurd label is clamped rather than eating the panel.
  CHECK(PropertyRow::measureColumn({QString(200, QChar('M'))}) == ui::px(140));
}

// --- the scale ------------------------------------------------------------------------------
void testScale()
{
  const double before = ui::scale();

  ui::setScale(1.0);
  CHECK(ui::scale() == 1.0);
  CHECK(ui::px(16) == 16);
  CHECK(met::hCtl() == 32);

  ui::setScale(1.5);
  CHECK(ui::scale() == 1.5);
  CHECK(ui::px(16) == 24);
  CHECK(met::hCtl() == 48);

  // Quarter steps: anything finer produces half-pixel borders that the stylesheet (integers)
  // and the layout (floats) round differently, which shows as a 1px jitter along panel edges.
  ui::setScale(1.31);
  CHECK(ui::scale() == 1.25);

  // Clamped at both ends.
  ui::setScale(0.4);
  CHECK(ui::scale() == 1.0);
  ui::setScale(9.0);
  CHECK(ui::scale() == 3.0);

  ui::setScale(before);
}

// --- the stylesheet -------------------------------------------------------------------------
void testSheet()
{
  const QString sheet = Theme::sheet();
  CHECK(!sheet.isEmpty());

  // Every placeholder was substituted. A leftover "@name" is a token that does not exist,
  // and Qt would silently drop the whole rule containing it.
  const QRegularExpression leftover(QStringLiteral("@[a-zA-Z]"));
  CHECK(!leftover.match(sheet).hasMatch());

  // The families really made it in, so the sheet cannot fall back to a platform default
  // behind the application font's back.
  CHECK(sheet.contains(typo::uiFamily()));

  // Complete families, not single sub-controls: touching one sub-control of a complex widget
  // makes Qt stop drawing the whole widget natively, so the rest has to be named too. These
  // four are the ones that looked half-native before.
  for (const char* needed : {"QScrollBar::add-line", "QScrollBar::handle:vertical",
                             "QComboBox::down-arrow", "QComboBox QAbstractItemView",
                             "QSpinBox::up-arrow", "QSpinBox::down-button"})
    CHECK(sheet.contains(QLatin1String(needed)));

  // The branch indicator must NOT be styled: naming it hands the indicator to the stylesheet
  // and MidnightStyle's chevrons never get drawn. This is a regression that already happened
  // once, and it is invisible in code review.
  CHECK(!sheet.contains(QLatin1String("QTreeView::branch {")));

  // Colours come from the tokens. Any hex in the sheet has to be one of them.
  const QRegularExpression hex(QStringLiteral("#[0-9a-fA-F]{6}"));
  QStringList known;
  for (const char* c : {tok::bgVoid, tok::bgChrome, tok::bgWell, tok::bgPanel, tok::bgRaised,
                        tok::bgRaisedHover, tok::bgPressed, tok::lineBorder, tok::lineStrong,
                        tok::fgText, tok::fgSoft, tok::fgMuted, tok::fgDim, tok::accent,
                        tok::accentFill, tok::accentFillHover, tok::accentFillPress,
                        tok::accentText, tok::onAccent, tok::ok, tok::warn, tok::danger})
    known << QString::fromLatin1(c).toLower();
  auto it = hex.globalMatch(sheet);
  while (it.hasNext()) {
    const QString found = it.next().captured(0).toLower();
    if (!known.contains(found)) {
      std::printf("FAIL raw colour in stylesheet: %s\n", qPrintable(found));
      failures++;
    }
  }
}

// --- the palette ------------------------------------------------------------------------------
void testPalette()
{
  const QPalette p = Theme::palette();
  // Fusion takes disabled colours from the palette and ignores the stylesheet for them, which
  // is why "disabled" used to look like four different things.
  CHECK(p.color(QPalette::Disabled, QPalette::Text)
        != p.color(QPalette::Active, QPalette::Text));
  CHECK(p.color(QPalette::Highlight) == QColor(tok::accentFill));
  CHECK(p.color(QPalette::Base) == QColor(tok::bgWell));
}

// --- icons -------------------------------------------------------------------------------------
void testIcons()
{
  // Every icon the interface asks for by name has to exist in the resources; a missing one
  // is a blank button, which is not obviously wrong at a glance.
  for (const char* name : {"layers", "user", "paw-print", "swords", "users", "eye", "eye-off",
                           "x", "minus", "square", "maximize-2", "grid-3x3", "palette",
                           "camera", "package", "search", "chevron-down", "chevron-up",
                           "check", "play", "pause", "skip-back", "skip-forward",
                           "folder", "sun"})
    CHECK(!Theme::icon(QLatin1String(name)).isNull());

  // An unknown name gives a null icon rather than a crash or a stray placeholder.
  CHECK(Theme::icon(QStringLiteral("no-such-icon")).isNull());
}

}  // namespace

int main(int argc, char** argv)
{
  QApplication app(argc, argv);
  typo::loadFonts();

  testElidedLabel();
  testMeasuredColumn();
  testScale();
  testSheet();
  testPalette();
  testIcons();

  if (failures == 0)
    std::printf("uitest: all checks passed\n");
  else
    std::printf("uitest: %d check(s) FAILED\n", failures);
  return failures;
}
