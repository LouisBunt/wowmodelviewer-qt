#include "UiKit.h"

#include <QAction>
#include <QButtonGroup>
#include <QComboBox>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QListView>
#include <QPainter>
#include <QTimer>
#include <QVBoxLayout>

#include "Theme.h"

// =================================================================================================
// ElidedLabel
// =================================================================================================
ElidedLabel::ElidedLabel(const QString& text, QWidget* parent) : QLabel(parent)
{
  setFullText(text);
}

void ElidedLabel::setFullText(const QString& text)
{
  full_ = text;
  refresh();
}

void ElidedLabel::setElideMode(Qt::TextElideMode m)
{
  mode_ = m;
  refresh();
}

void ElidedLabel::setTooltipSuffix(const QString& s)
{
  suffix_ = s;
  refresh();
}

QSize ElidedLabel::minimumSizeHint() const
{
  // Whatever it holds, this label will never be the reason a layout cannot shrink -- that is the
  // whole point of it. Height still comes from the font.
  return QSize(0, QLabel::minimumSizeHint().height());
}

void ElidedLabel::refresh() const
{
  const QFontMetrics fm(font());
  const bool now = fm.horizontalAdvance(full_) > contentsRect().width();
  // A tooltip only where something is hidden: on every label it would be noise, and the user
  // could not tell which ones are actually cut.
  const QString want = now ? (suffix_.isEmpty() ? full_ : full_ + "\n" + suffix_) : suffix_;
  if (now == elided_ && toolTip() == want)
    return;
  auto* self = const_cast<ElidedLabel*>(this);
  elided_ = now;
  self->setToolTip(want);
  self->update();
}

bool ElidedLabel::isElided() const
{
  refresh();
  return elided_;
}

void ElidedLabel::changeEvent(QEvent* e)
{
  QLabel::changeEvent(e);
  // A font change alters what fits, and Qt does not resize the widget for it.
  if (e->type() == QEvent::FontChange || e->type() == QEvent::StyleChange)
    refresh();
}

void ElidedLabel::resizeEvent(QResizeEvent* e)
{
  QLabel::resizeEvent(e);
  refresh();
}

void ElidedLabel::paintEvent(QPaintEvent*)
{
  refresh();
  QPainter p(this);
  p.setFont(font());
  p.setPen(palette().color(foregroundRole()));
  const QRect r = contentsRect();
  const QFontMetrics fm(font());

  QString shown = full_;
  if (fm.horizontalAdvance(full_) > r.width()) {
    shown = fm.elidedText(full_, mode_, r.width());
    // Four characters is the floor: below that the ellipsis is longer than the message.
    // Rather than a lone "…", show as much of the head as fits, which at least identifies
    // the row.
    if (shown.length() < 4 && full_.length() >= 4)
      shown = fm.elidedText(full_, Qt::ElideRight, r.width());
  }
  p.drawText(r, (int)alignment() | Qt::TextSingleLine, shown);
}

// =================================================================================================
// SegmentedBar
// =================================================================================================
SegmentedBar::SegmentedBar(Mode mode, QWidget* parent) : QWidget(parent), mode_(mode)
{
  setAttribute(Qt::WA_StyledBackground, true);
  setProperty("role", "panel");
  row_ = new QHBoxLayout(this);
  row_->setContentsMargins(0, 0, 0, 0);
  row_->setSpacing(0);   // the segments share their outline; a gap would break the group
}

void SegmentedBar::addSegment(const QString& label, const QString& icon, const QString& tooltip)
{
  auto* b = new QToolButton(this);
  b->setCheckable(true);
  b->setCursor(Qt::PointingHandCursor);
  b->setFocusPolicy(Qt::StrongFocus);
  b->setToolButtonStyle(icon.isEmpty() ? Qt::ToolButtonTextOnly : Qt::ToolButtonTextBesideIcon);
  b->setText(label);
  if (!icon.isEmpty()) {
    b->setIcon(Theme::icon(icon, QColor(tok::fgSoft)));
    b->setIconSize(QSize(ui::px(17), ui::px(17)));
  }
  b->setToolTip(tooltip.isEmpty() ? label : tooltip);
  b->setMinimumHeight(met::hRow());

  const int index = buttons_.size();
  connect(b, &QToolButton::clicked, this, [this, index]() {
    if (mode_ == Radio)
      setCurrent(index);
    else
      current_ = index;
    emit activated(index);
  });

  buttons_.push_back(b);
  labels_ << label;
  icons_ << icon;
  row_->addWidget(b);

  // Rounded ends, square middles: one control, not several.
  for (int i = 0; i < buttons_.size(); ++i) {
    const char* seg = buttons_.size() == 1 ? "only"
                      : i == 0 ? "first"
                      : i == buttons_.size() - 1 ? "last" : "mid";
    buttons_[i]->setProperty("segment", seg);
    Theme::repolish(buttons_[i]);
  }
  if (mode_ == Radio && current_ < 0)
    setCurrent(0);
}

void SegmentedBar::setCurrent(int index)
{
  current_ = index;
  for (int i = 0; i < buttons_.size(); ++i)
    buttons_[i]->setChecked(mode_ == Radio ? (i == index) : buttons_[i]->isChecked());
}

void SegmentedBar::setSegmentEnabled(int index, bool on)
{
  if (index >= 0 && index < buttons_.size())
    buttons_[index]->setEnabled(on);
}

void SegmentedBar::resizeEvent(QResizeEvent* e)
{
  QWidget::resizeEvent(e);
  applyLabels();
}

void SegmentedBar::applyLabels()
{
  if (icons_.isEmpty() || labels_.isEmpty())
    return;
  bool anyIcon = false;
  for (const QString& i : icons_)
    if (!i.isEmpty()) anyIcon = true;
  if (!anyIcon)
    return;   // nothing to fall back to; the labels stay and the bar scrolls with its parent

  int need = 0;
  const QFontMetrics fm(buttons_.isEmpty() ? font() : buttons_[0]->font());
  for (const QString& l : labels_)
    need += fm.horizontalAdvance(l) + met::sp(met::Edge) + met::hIcon();
  const int threshold = breakpoint_ > 0 ? breakpoint_ : need;

  const bool showLabels = width() >= threshold;
  if (showLabels == labelsVisible_)
    return;
  labelsVisible_ = showLabels;
  // The label is dropped, never truncated: "arakte" told the user nothing, an icon with a
  // tooltip tells them the same thing the full word did.
  for (int i = 0; i < buttons_.size(); ++i) {
    if (icons_[i].isEmpty())
      continue;
    buttons_[i]->setToolButtonStyle(showLabels ? Qt::ToolButtonTextBesideIcon
                                               : Qt::ToolButtonIconOnly);
  }
}

// =================================================================================================
// SearchField
// =================================================================================================
SearchField::SearchField(const QString& placeholder, QWidget* parent) : QLineEdit(parent)
{
  setPlaceholderText(placeholder);
  setClearButtonEnabled(true);
  setMinimumHeight(met::hCtl());
  addAction(Theme::icon("search", QColor(tok::fgMuted)), QLineEdit::LeadingPosition);

  debounce_ = new QTimer(this);
  debounce_->setSingleShot(true);
  // 200 ms: long enough that holding backspace does not re-filter per keystroke, short enough
  // that the list feels live. wow.export settled on the same number for the same reason.
  debounce_->setInterval(200);
  connect(debounce_, &QTimer::timeout, this, [this]() { emit searchChanged(text()); });
  connect(this, &QLineEdit::textChanged, this, [this]() { debounce_->start(); });
}

void SearchField::keyPressEvent(QKeyEvent* e)
{
  if (e->key() == Qt::Key_Escape && !text().isEmpty()) {
    clear();
    emit searchChanged(QString());
    e->accept();
    return;
  }
  QLineEdit::keyPressEvent(e);
}

// =================================================================================================
// PropertyRow
// =================================================================================================
int PropertyRow::measureColumn(const QStringList& labels, int minPx, int maxPx)
{
  // Measured against the font that will actually be used, over the strings that will actually
  // be shown. The old fixed 88 px was measured against a font the machine does not have.
  const QFontMetrics fm(typo::font(typo::Body));
  int w = 0;
  for (const QString& l : labels)
    w = qMax(w, fm.horizontalAdvance(l));
  w += met::sp(met::Gap);
  return qBound(ui::px(minPx), w, ui::px(maxPx));
}

PropertyRow::PropertyRow(const QString& label, QWidget* control, int columnPx, QWidget* parent)
  : QWidget(parent), control_(control), column_(columnPx)
{
  setAttribute(Qt::WA_StyledBackground, true);
  setProperty("role", "panel");

  label_ = new ElidedLabel(label, this);
  label_->setFont(typo::font(typo::Body));
  label_->setProperty("role", "caption");
  label_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  label_->setElideMode(Qt::ElideRight);

  suffix_ = new QLabel(this);
  suffix_->setFont(typo::font(typo::Caption));
  suffix_->setProperty("role", "caption");
  suffix_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  suffix_->setFixedWidth(ui::px(32));   // fixed, so a two-digit count never shifts the control
  suffix_->hide();

  if (control_)
    control_->setParent(this);
  relayout(false);
}

void PropertyRow::setSuffix(const QString& text)
{
  suffix_->setText(text);
  suffix_->setVisible(!text.isEmpty());
}

void PropertyRow::relayout(bool stacked)
{
  delete layout();
  stacked_ = stacked;
  if (stacked) {
    // Below the threshold the label goes above its control, full width. A stacked row cannot
    // clip, whatever the language does to the string.
    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(met::sp(met::Tight));
    auto* head = new QHBoxLayout;
    head->setContentsMargins(0, 0, 0, 0);
    head->setSpacing(met::sp(met::Snug));
    head->addWidget(label_, 1);
    head->addWidget(suffix_);
    col->addLayout(head);
    if (control_)
      col->addWidget(control_);
    label_->setFixedWidth(QWIDGETSIZE_MAX);
    label_->setMinimumWidth(0);
  } else {
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(met::sp(met::Gap));
    label_->setFixedWidth(column_);
    row->addWidget(label_);
    if (control_)
      row->addWidget(control_, 1);
    row->addWidget(suffix_);
  }
}

void PropertyRow::resizeEvent(QResizeEvent* e)
{
  QWidget::resizeEvent(e);
  // The control needs room of its own; when the row gets narrower than the label column plus a
  // usable control, stacking is the only honest answer.
  const bool wantStacked = width() < column_ + ui::px(120);
  if (wantStacked != stacked_)
    relayout(wantStacked);
}

// =================================================================================================
// helpers
// =================================================================================================
QLabel* uikit::sectionLabel(const QString& text)
{
  auto* l = new QLabel(text);
  l->setFont(typo::font(typo::Caption));
  l->setProperty("role", "section");
  return l;
}

QWidget* uikit::hairline()
{
  auto* w = new QWidget;
  w->setAttribute(Qt::WA_StyledBackground, true);
  w->setProperty("role", "hline");
  w->setFixedHeight(1);
  return w;
}

namespace {

// A combo whose popup is as wide as its longest entry.
class WideCombo : public QComboBox
{
public:
  explicit WideCombo(QWidget* parent = nullptr) : QComboBox(parent)
  {
    setView(new QListView(this));
    view()->setTextElideMode(Qt::ElideNone);
  }

  void showPopup() override
  {
    const QFontMetrics fm(view()->font());
    int w = 0;
    for (int i = 0; i < count(); ++i)
      w = qMax(w, fm.horizontalAdvance(itemText(i)));
    // Padding for the item's own margins plus the scrollbar, which appears for long lists.
    w += met::sp(met::Edge) * 2 + ui::px(12);
    view()->setMinimumWidth(qMax(w, width()));
    QComboBox::showPopup();
  }
};

}  // namespace

QComboBox* uikit::wideCombo()
{
  auto* c = new WideCombo;
  c->setMinimumHeight(met::hCtl());
  c->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  return c;
}

QToolButton* uikit::iconButton(const QString& icon, const QString& tooltip)
{
  auto* b = new QToolButton;
  // fgSoft, not fgMuted: a 2px stroke at 16 px on a near-black ground needs the brighter of
  // the two greys to read as a control at all. Muted is for text, which has more mass.
  b->setIcon(Theme::icon(icon, QColor(tok::fgSoft)));
  b->setToolTip(tooltip);
  b->setCursor(Qt::PointingHandCursor);
  b->setAutoRaise(true);
  // The sheet gives every tool button horizontal padding for its label; an icon-only button
  // has none, and that padding would eat the glyph inside a fixed box.
  b->setProperty("role", "icon");
  b->setFixedSize(ui::px(28), ui::px(28));
  b->setIconSize(QSize(ui::px(18), ui::px(18)));
  return b;
}
