// Prototype: the WMV character inspector panel rebuilt in Qt Widgets, following
// the "WoW Model Viewer Redesign" mock-up. The point is to find out how close Qt
// gets to that design and what it costs, before touching the real front-end.
//
// Run with --shot <file.png> to render the panel offscreen instead of showing it.

#include <QApplication>
#include <QFont>
#include <QResizeEvent>
#include <QFontDatabase>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>

namespace tok {
// Design tokens straight from the mock-up.
const char* kPanel     = "#0e1114";
const char* kCard      = "#14181e";
const char* kBorder    = "#23282f";
const char* kText      = "#e8eaee";
const char* kTextSoft  = "#b6bdc8";
const char* kMuted     = "#7d8693";
const char* kMutedDim  = "#5f6874";
const char* kAccent    = "#c8a15a";
const char* kAccentBg  = "#191509";
const char* kAccentBr  = "#3a3222";
}

// IBM Plex / Cinzel are not installed on this machine, so fall back gracefully.
// Qt picks the first family that exists from a comma list only via stylesheet,
// not via QFont, so build the stack explicitly.
static QString uiFamily()
{
  for (const QString& f : {QStringLiteral("IBM Plex Sans"), QStringLiteral("Segoe UI")})
    if (QFontDatabase().families().contains(f))
      return f;
  return QStringLiteral("sans-serif");
}

static QString monoFamily()
{
  for (const QString& f : {QStringLiteral("IBM Plex Mono"), QStringLiteral("Consolas")})
    if (QFontDatabase().families().contains(f))
      return f;
  return QStringLiteral("monospace");
}

static QString displayFamily()
{
  for (const QString& f : {QStringLiteral("Cinzel"), QStringLiteral("Georgia")})
    if (QFontDatabase().families().contains(f))
      return f;
  return QStringLiteral("serif");
}

// A label that shortens its text with an ellipsis instead of forcing the whole
// panel wider. The mock-up's inspector is a fixed 324 px column, so every label
// in it has to be able to shrink.
class ElidedLabel : public QLabel
{
public:
  explicit ElidedLabel(const QString& text, QWidget* parent = nullptr)
    : QLabel(parent), full_(text)
  {
    setMinimumWidth(1);
    setText(text);
  }

protected:
  void resizeEvent(QResizeEvent* e) override
  {
    QLabel::resizeEvent(e);
    QLabel::setText(fontMetrics().elidedText(full_, Qt::ElideRight, width()));
  }

private:
  QString full_;
};

static QLabel* sectionLabel(const QString& text)
{
  auto* l = new QLabel(text.toUpper());
  QFont f(uiFamily(), 8);
  f.setLetterSpacing(QFont::AbsoluteSpacing, 1.4);
  l->setFont(f);
  l->setStyleSheet(QString("color:%1;").arg(tok::kMutedDim));
  return l;
}

// --- character header card -------------------------------------------------

static QFrame* characterCard()
{
  auto* card = new QFrame;
  card->setObjectName("card");
  auto* row = new QHBoxLayout(card);
  row->setContentsMargins(12, 12, 12, 12);
  row->setSpacing(12);

  auto* avatar = new QLabel("G");
  avatar->setFixedSize(46, 46);
  avatar->setAlignment(Qt::AlignCenter);
  avatar->setFont(QFont(displayFamily(), 13, QFont::DemiBold));
  avatar->setStyleSheet(QString(
    "background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #2a3340, stop:1 #151a21);"
    "border:1px solid #2c333d; border-radius:8px; color:%1;").arg(tok::kAccent));
  row->addWidget(avatar);

  auto* names = new QVBoxLayout;
  names->setSpacing(2);
  auto* name = new ElidedLabel("Gruhlmok");
  name->setFont(QFont(uiFamily(), 10, QFont::DemiBold));
  name->setStyleSheet(QString("color:%1;").arg(tok::kText));
  auto* sub = new ElidedLabel(QString::fromUtf8("Mag'har Orc · Männlich · Todesritter"));
  sub->setFont(QFont(uiFamily(), 8));
  sub->setStyleSheet(QString("color:%1;").arg(tok::kMuted));
  names->addWidget(name);
  names->addWidget(sub);
  row->addLayout(names, 1);

  auto* armory = new QPushButton("Armory");
  armory->setCursor(Qt::PointingHandCursor);
  armory->setFont(QFont(uiFamily(), 8));
  armory->setStyleSheet(QString(
    "QPushButton { color:%1; background:%2; border:1px solid %3;"
    "border-radius:5px; padding:3px 8px; }"
    "QPushButton:hover { background:#221c0d; }")
    .arg(tok::kAccent).arg(tok::kAccentBg).arg(tok::kAccentBr));
  row->addWidget(armory, 0, Qt::AlignVCenter);

  return card;
}

// --- customization sliders -------------------------------------------------

static QWidget* sliderRow(const QString& label, const QString& value, int pct)
{
  auto* w = new QWidget;
  auto* col = new QVBoxLayout(w);
  col->setContentsMargins(0, 0, 0, 0);
  col->setSpacing(6);

  auto* head = new QHBoxLayout;
  auto* l = new QLabel(label);
  l->setFont(QFont(uiFamily(), 9));
  l->setStyleSheet(QString("color:%1;").arg(tok::kTextSoft));
  auto* v = new QLabel(value);
  v->setFont(QFont(monoFamily(), 8));
  v->setStyleSheet(QString("color:%1;").arg(tok::kMuted));
  head->addWidget(l);
  head->addStretch(1);
  head->addWidget(v);
  col->addLayout(head);

  auto* s = new QSlider(Qt::Horizontal);
  s->setRange(0, 100);
  s->setValue(pct);
  s->setFixedHeight(12);
  s->setStyleSheet(
    "QSlider::groove:horizontal { height:4px; border-radius:2px; background:#1c222a; }"
    "QSlider::sub-page:horizontal { height:4px; border-radius:2px; background:#6f7f92; }"
    "QSlider::handle:horizontal { width:12px; height:12px; margin:-4px 0;"
    "border-radius:6px; background:#cdd3dc; border:2px solid #0e1114; }");
  col->addWidget(s);
  return w;
}

// --- equipment grid --------------------------------------------------------

static QFrame* slotCard(const QString& slot, const QString& item, const QString& quality)
{
  auto* f = new QFrame;
  f->setObjectName("card");
  f->setCursor(Qt::PointingHandCursor);
  auto* row = new QHBoxLayout(f);
  row->setContentsMargins(7, 7, 7, 7);
  row->setSpacing(8);

  auto* swatch = new QLabel;
  swatch->setFixedSize(26, 26);
  swatch->setStyleSheet(QString("background:#10141a; border:1px solid %1; border-radius:5px;").arg(quality));
  row->addWidget(swatch);

  auto* col = new QVBoxLayout;
  col->setSpacing(1);
  auto* s = new QLabel(slot.toUpper());
  QFont sf(uiFamily(), 7);
  sf.setLetterSpacing(QFont::AbsoluteSpacing, 0.7);
  s->setFont(sf);
  s->setStyleSheet(QString("color:%1;").arg(tok::kMutedDim));
  auto* i = new ElidedLabel(item);
  i->setFont(QFont(uiFamily(), 8));
  i->setStyleSheet(QString("color:%1;").arg(quality));
  col->addWidget(s);
  col->addWidget(i);
  row->addLayout(col, 1);
  return f;
}

// --- tab strip -------------------------------------------------------------

static QWidget* tabStrip()
{
  auto* w = new QWidget;
  auto* row = new QHBoxLayout(w);
  row->setContentsMargins(0, 0, 0, 0);
  row->setSpacing(0);

  const QStringList tabs = {"Charakter", "Material", "Export"};
  for (int i = 0; i < tabs.size(); ++i) {
    auto* b = new QPushButton(tabs[i]);
    b->setFixedHeight(38);
    b->setCursor(Qt::PointingHandCursor);
    b->setFont(QFont(uiFamily(), 9, i == 0 ? QFont::DemiBold : QFont::Normal));
    b->setStyleSheet(i == 0
      ? QString("QPushButton { border:0; border-bottom:2px solid %1; background:transparent; color:%2; }")
          .arg(tok::kAccent).arg(tok::kText)
      : QString("QPushButton { border:0; background:transparent; color:%1; }"
                "QPushButton:hover { color:%2; }").arg(tok::kMuted).arg(tok::kText));
    row->addWidget(b, 1);
  }
  return w;
}

// ---------------------------------------------------------------------------

static QWidget* buildInspector()
{
  auto* root = new QWidget;
  root->setFixedWidth(324);
  root->setStyleSheet(QString(
    "QWidget { background:%1; }"
    // Without this every label paints the panel colour over the card it sits on.
    "QLabel { background:transparent; }"
    "QFrame#card { background:%2; border:1px solid %3; border-radius:8px; }"
    "QFrame#sep { background:%3; }")
    .arg(tok::kPanel).arg(tok::kCard).arg(tok::kBorder));

  auto* outer = new QVBoxLayout(root);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->setSpacing(0);

  outer->addWidget(tabStrip());
  auto* sep = new QFrame;
  sep->setObjectName("sep");
  sep->setFixedHeight(1);
  outer->addWidget(sep);

  auto* body = new QWidget;
  auto* col = new QVBoxLayout(body);
  col->setContentsMargins(16, 16, 16, 24);
  col->setSpacing(20);

  col->addWidget(characterCard());

  auto* custom = new QVBoxLayout;
  custom->setSpacing(12);
  custom->addWidget(sectionLabel("Anpassung"));
  const struct { const char* l; const char* v; int p; } sliders[] = {
    {"Hautfarbe", "04", 38}, {"Gesicht", "07", 62}, {"Frisur", "11", 84},
    {"Haarfarbe", "02", 22}, {"Hauer", "05", 45}
  };
  for (const auto& s : sliders)
    custom->addWidget(sliderRow(s.l, s.v, s.p));
  col->addLayout(custom);

  auto* equip = new QVBoxLayout;
  equip->setSpacing(10);
  equip->addWidget(sectionLabel(QString::fromUtf8("Ausrüstung")));
  auto* grid = new QGridLayout;
  grid->setSpacing(7);
  // NB: not "slots" -- Qt #defines that as a keyword macro.
  const struct { const char* s; const char* i; const char* q; } equipSlots[] = {
    {"Kopf", "Grimmige Nacht", "#a335ee"},   {"Schulter", "Ewige Wacht", "#0070dd"},
    {"Brust", "Kriegsplatte", "#a335ee"},    {"Hände", "Schlächterfäuste", "#0070dd"},
    {"Gürtel", "Gurt des Zorns", "#1eff00"}, {"Beine", "Beinschienen", "#a335ee"},
    {"Füße", "Stiefel der Wut", "#1eff00"}, {"Umhang", "Mantel der Horde", "#0070dd"},
    {"Waffe", "Ashbringer", "#ff8000"},      {"Schild", "Bollwerk", "#a335ee"}
  };
  int n = 0;
  for (const auto& s : equipSlots) {
    grid->addWidget(slotCard(QString::fromUtf8(s.s), QString::fromUtf8(s.i), s.q), n / 2, n % 2);
    ++n;
  }
  equip->addLayout(grid);
  col->addLayout(equip);
  col->addStretch(1);

  auto* scroll = new QScrollArea;
  scroll->setWidget(body);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scroll->setStyleSheet(
    "QScrollBar:vertical { background:#0e1114; width:10px; margin:0; }"
    "QScrollBar::handle:vertical { background:#262c35; border-radius:5px; min-height:30px; }"
    "QScrollBar::handle:vertical:hover { background:#38414d; }"
    "QScrollBar::add-line, QScrollBar::sub-line { height:0; }"
    "QScrollBar::add-page, QScrollBar::sub-page { background:transparent; }");
  outer->addWidget(scroll, 1);

  return root;
}

int main(int argc, char** argv)
{
  QApplication app(argc, argv);

  QString shot;
  for (int i = 1; i < argc - 1; ++i)
    if (QString(argv[i]) == "--shot")
      shot = QString::fromLocal8Bit(argv[i + 1]);

  QWidget* panel = buildInspector();
  panel->setFixedHeight(860);

  if (!shot.isEmpty()) {
    panel->show();          // needed so the layout settles before grabbing
    app.processEvents();
    panel->grab().save(shot);
    return 0;
  }

  panel->setWindowTitle("WMV Inspector - Qt Prototype");
  panel->show();
  return app.exec();
}
