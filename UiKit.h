#ifndef UIKIT_H
#define UIKIT_H

// The parts every panel is built from.
//
// Before this existed, each panel hand-rolled its own version of the same five things — a
// search field, a chip row, a label/value row, a section heading, a clickable label pretending
// to be a button — with different paddings, radii, debounce times and even different escaping
// of apostrophes. Six copies of the combo style, five of the checkbox, two of the search field.
// That divergence is most of what "everything feels somehow off" was made of.
//
// Everything here takes its colours and measures from Theme.h and nothing else.

#include <QLabel>
#include <QLineEdit>
#include <QStringList>
#include <QToolButton>
#include <QVector>
#include <QWidget>

class QComboBox;
class QHBoxLayout;
class QTimer;
class QButtonGroup;

// --- ElidedLabel ------------------------------------------------------------------------------
// A label that shortens its text to fit instead of letting it run under the next widget.
//
// QLabel has no elide mode: give it a string longer than its width and it simply clips, mid
// glyph, with no ellipsis and no way to read the rest. That is the single most visible defect in
// the old interface — item names, paths and the window title all did it. This paints the elided
// form and puts the full string in the tooltip, but ONLY when something was actually hidden, so
// a tooltip means "there is more here" rather than being noise on every label.
class ElidedLabel : public QLabel
{
  Q_OBJECT
public:
  explicit ElidedLabel(const QString& text = QString(), QWidget* parent = nullptr);

  void setFullText(const QString& text);
  QString fullText() const { return full_; }
  void setElideMode(Qt::TextElideMode m);
  // Extra tooltip text appended after the full string, e.g. a file id. Empty by default.
  void setTooltipSuffix(const QString& s);
  // Recomputed on demand rather than cached from the last resize event: Qt only DELIVERS a
  // resize event to a widget that has been shown, so a cached flag was a lie for any label
  // that had been sized but not yet displayed -- and the tooltip that goes with it was
  // missing exactly then.
  bool isElided() const;

  QSize minimumSizeHint() const override;

protected:
  void paintEvent(QPaintEvent* e) override;
  void resizeEvent(QResizeEvent* e) override;
  void changeEvent(QEvent* e) override;

private:
  void refresh() const;
  QString full_;
  QString suffix_;
  Qt::TextElideMode mode_ = Qt::ElideRight;
  mutable bool elided_ = false;
};

// --- SegmentedBar -----------------------------------------------------------------------------
// One control made of several buttons: [Charakter | Nur Teil], [Vorn | ¾ | Seite | Oben].
//
// Replaces the "chips" — QLabels with an event filter — which had no hover, no pressed state, no
// keyboard access and no disabled look, and whose text was simply cut off when the column was
// too narrow ("Charaktere" became "arakte"). Here the label is dropped in favour of the icon
// when the space runs out, and the name survives in the tooltip.
class SegmentedBar : public QWidget
{
  Q_OBJECT
public:
  enum Mode { Radio, Toggle };
  explicit SegmentedBar(Mode mode = Radio, QWidget* parent = nullptr);

  // icon may be empty; then the label is never dropped.
  void addSegment(const QString& label, const QString& icon = QString(),
                  const QString& tooltip = QString());
  void setCurrent(int index);
  int current() const { return current_; }
  void setSegmentEnabled(int index, bool on);
  // Below this width the labels give way to the icons. -1 (default) computes it from the
  // segments' own text.
  void setLabelBreakpoint(int px) { breakpoint_ = px; }

signals:
  void activated(int index);

protected:
  void resizeEvent(QResizeEvent* e) override;

private:
  void applyLabels();
  Mode mode_;
  int current_ = -1;
  int breakpoint_ = -1;
  QHBoxLayout* row_ = nullptr;
  QVector<QToolButton*> buttons_;
  QStringList labels_;
  QStringList icons_;
  bool labelsVisible_ = true;
};

// --- SearchField ------------------------------------------------------------------------------
// The one search box: magnifier, clear button, 200 ms debounce, Escape clears.
//
// The three search fields in the left column used to be three different widgets: different
// heights, different focus colours, one filtering instantly, one after 250 ms, one after 350,
// and two of them disagreeing about apostrophes so that "O'ros" worked in one and silently
// found nothing in the other.
class SearchField : public QLineEdit
{
  Q_OBJECT
public:
  explicit SearchField(const QString& placeholder, QWidget* parent = nullptr);

signals:
  // Emitted after the user stops typing. Connect to this, not to textChanged.
  void searchChanged(const QString& text);

protected:
  void keyPressEvent(QKeyEvent* e) override;

private:
  QTimer* debounce_ = nullptr;
  QAction* clear_ = nullptr;
};

// --- PropertyRow ------------------------------------------------------------------------------
// A label and its control, side by side, where the label can never push the control out of the
// panel and can never be silently cut in half.
//
// The old code gave the label a hardcoded width — 88 px in two places, 74 in a third — measured
// against a font that is not installed on the machine. German compounds ("Schulterausrüstung",
// "Beleuchtungseinstellungen") do not fit in 88 px in any font. Here the column is MEASURED over
// the actual set of labels the panel will show, clamped to a sane range, and the label itself
// elides with a tooltip. Below a narrow threshold the row stacks instead, which is the only
// layout that cannot fail.
class PropertyRow : public QWidget
{
  Q_OBJECT
public:
  // Measures the column for one panel: the widest label in `labels`, clamped. Call once and
  // pass the result to every row of that panel, so the controls line up.
  static int measureColumn(const QStringList& labels, int minPx = 96, int maxPx = 140);

  PropertyRow(const QString& label, QWidget* control, int columnPx, QWidget* parent = nullptr);

  ElidedLabel* label() const { return label_; }
  QWidget* control() const { return control_; }
  // Right-aligned trailing text, e.g. a choice count. Fixed width, so it never moves.
  void setSuffix(const QString& text);

protected:
  void resizeEvent(QResizeEvent* e) override;

private:
  void relayout(bool stacked);
  ElidedLabel* label_ = nullptr;
  QWidget* control_ = nullptr;
  QLabel* suffix_ = nullptr;
  int column_ = 96;
  bool stacked_ = false;
};

// --- helpers ------------------------------------------------------------------------------------
namespace uikit {

// A section heading with the caption role, for the top of a group.
QLabel* sectionLabel(const QString& text);

// A horizontal hairline.
QWidget* hairline();

// A combo box that shows its entries in full. Qt sizes the popup to the closed box, so a long
// animation name or "Wrath of the Lich King" was elided in the list as well as in the box —
// unreadable either way. This measures the longest entry and widens the popup to fit.
QComboBox* wideCombo();

// An icon-only button with a guaranteed 24x24 hit area, whatever the glyph size.
QToolButton* iconButton(const QString& icon, const QString& tooltip);

}  // namespace uikit

#endif  // UIKIT_H
