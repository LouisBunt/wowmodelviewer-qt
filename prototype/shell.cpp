// Full-window prototype of the "WoW Model Viewer Redesign" mock-up in Qt Widgets.
// Companion to main.cpp (which prototypes the inspector column alone).
//
//   shell.exe                 -- show the window
//   shell.exe --shot out.png  -- render offscreen and exit
//
// Everything here is static mock data. The question this answers is "how close
// does Qt get to the design, and with how much code", not "does it work".

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>

namespace tok {
const char* kApp      = "#0b0d10";
const char* kPanel    = "#0e1114";
const char* kCard     = "#14181e";
const char* kBorder   = "#23282f";
const char* kBorder2  = "#1c2128";
const char* kText     = "#e8eaee";
const char* kTextSoft = "#b6bdc8";
const char* kMuted    = "#8a93a0";
const char* kDim      = "#5f6874";
const char* kAccent   = "#c8a15a";
const char* kAccentBg = "#191509";
const char* kAccentBr = "#3a3222";
const char* kOnAccent = "#17130a";
}

static QString pick(std::initializer_list<const char*> names, const char* fallback)
{
  const QStringList have = QFontDatabase().families();
  for (const char* n : names)
    if (have.contains(QString::fromLatin1(n)))
      return QString::fromLatin1(n);
  return QString::fromLatin1(fallback);
}
static QString uiF()   { static QString f = pick({"IBM Plex Sans", "Segoe UI"}, "sans-serif"); return f; }
static QString monoF() { static QString f = pick({"IBM Plex Mono", "Consolas"}, "monospace"); return f; }
static QString dispF() { static QString f = pick({"Cinzel", "Georgia"}, "serif"); return f; }
// The geometric symbols used as icons fall outside Segoe UI's coverage; they
// only render in the symbol font.
static QString iconF() { static QString f = pick({"Segoe UI Symbol", "Segoe UI"}, "sans-serif"); return f; }

// A plain QWidget ignores a stylesheet background unless told to honour one.
static void styled(QWidget* w) { w->setAttribute(Qt::WA_StyledBackground, true); }

class ElidedLabel : public QLabel
{
public:
  explicit ElidedLabel(const QString& t, QWidget* p = nullptr) : QLabel(p), full_(t)
  { setMinimumWidth(1); setText(t); }
protected:
  void resizeEvent(QResizeEvent* e) override
  { QLabel::resizeEvent(e); QLabel::setText(fontMetrics().elidedText(full_, Qt::ElideRight, width())); }
private:
  QString full_;
};

// The animation track: tick grid, elapsed fill and playhead. Drawn by hand
// because the mock-up's playhead glow has no widget equivalent.
class TrackWidget : public QWidget
{
public:
  explicit TrackWidget(double progress, QWidget* p = nullptr) : QWidget(p), progress_(progress)
  { setFixedHeight(40); }
protected:
  void paintEvent(QPaintEvent*) override
  {
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing);
    const QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);

    QPainterPath clip;
    clip.addRoundedRect(r, 6, 6);
    g.fillPath(clip, QColor("#0a0d10"));

    g.save();
    g.setClipPath(clip);
    const int cells = 12;
    g.setFont(QFont(monoF(), 7));
    for (int i = 0; i < cells; ++i) {
      const double x = r.width() * i / cells;
      if (i > 0) {
        g.setPen(QColor(tok::kCard));
        g.drawLine(QPointF(x, r.top()), QPointF(x, r.bottom()));
      }
      g.setPen(QColor("#3f4650"));
      g.drawText(QRectF(x + 5, r.top() + 2, 40, 14), Qt::AlignLeft | Qt::AlignTop,
                 QString::number(i * 0.2, 'f', 1));
    }

    const double px = r.width() * progress_;
    QLinearGradient fill(0, 0, px, 0);
    fill.setColorAt(0, QColor(200, 161, 90, 128));
    fill.setColorAt(1, QColor(200, 161, 90, 46));
    g.fillRect(QRectF(r.left(), r.top() + 22, px, 12), fill);

    g.setPen(QPen(QColor(tok::kAccent), 2));
    g.drawLine(QPointF(px, r.top()), QPointF(px, r.bottom()));
    g.fillRect(QRectF(px - 5, r.top(), 10, 8), QColor(tok::kAccent));
    g.restore();

    g.setPen(QColor(tok::kBorder2));
    g.drawPath(clip);
  }
private:
  double progress_;
};

static QLabel* mk(const QString& text, const QString& family, int pt, const char* colour,
                  bool bold = false, qreal spacing = 0)
{
  auto* l = new QLabel(text);
  QFont f(family, pt, bold ? QFont::DemiBold : QFont::Normal);
  if (spacing > 0)
    f.setLetterSpacing(QFont::AbsoluteSpacing, spacing);
  l->setFont(f);
  l->setStyleSheet(QString("color:%1; background:transparent; border:none;").arg(colour));
  return l;
}

static QLabel* chip(const QString& text, bool active, const QString& family, int pt)
{
  auto* l = new QLabel(text);
  l->setFont(QFont(family, pt));
  l->setAlignment(Qt::AlignCenter);
  l->setStyleSheet(active
    ? QString("color:%1; background:%2; border:1px solid %3; border-radius:9px; padding:3px 10px;")
        .arg(tok::kAccent).arg(tok::kAccentBg).arg(tok::kAccentBr)
    : QString("color:%1; background:#12161b; border:1px solid %2; border-radius:9px; padding:3px 10px;")
        .arg(tok::kMuted).arg(tok::kBorder));
  return l;
}

static QLabel* icon(const QString& glyph, int size, const char* colour, const char* background)
{
  auto* l = new QLabel(glyph);
  l->setFixedSize(size, size);
  l->setAlignment(Qt::AlignCenter);
  l->setFont(QFont(iconF(), size / 3 + 3));
  l->setStyleSheet(QString("color:%1; background:%2; border:none; border-radius:6px;")
                   .arg(colour).arg(background));
  return l;
}

// --- title bar -------------------------------------------------------------

static QWidget* titleBar()
{
  auto* w = new QWidget;
  styled(w);
  w->setFixedHeight(38);
  w->setStyleSheet(QString(
    "background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #14181e, stop:1 #0f1216);"
    "border-bottom:1px solid %1;").arg(tok::kBorder2));
  auto* row = new QHBoxLayout(w);
  row->setContentsMargins(14, 0, 12, 0);
  row->setSpacing(14);

  auto* brand = new QHBoxLayout;
  brand->setSpacing(8);
  brand->addWidget(icon(QString::fromUtf8("◆"), 16, tok::kAccent, "transparent"));
  brand->addWidget(mk("MODEL VIEWER", dispF(), 9, "#d7c39a", true, 1.3));
  row->addLayout(brand);

  auto* menu = new QHBoxLayout;
  menu->setSpacing(2);
  for (const char* m : {"Datei", "Ansicht", "Charakter", "Export", "Hilfe"}) {
    auto* l = mk(QString::fromLatin1(m), uiF(), 9, "#99a2af");
    l->setStyleSheet("color:#99a2af; background:transparent; border:none; padding:4px 9px;");
    menu->addWidget(l);
  }
  row->addLayout(menu);
  row->addStretch(1);

  auto* pill = new QFrame;
  pill->setStyleSheet(QString("background:#0f1216; border:1px solid %1; border-radius:10px;").arg(tok::kBorder));
  auto* pr = new QHBoxLayout(pill);
  pr->setContentsMargins(8, 3, 10, 3);
  pr->setSpacing(7);
  auto* dot = new QLabel;
  dot->setFixedSize(6, 6);
  dot->setStyleSheet("background:#5bbd7a; border:none; border-radius:3px;");
  pr->addWidget(dot);
  pr->addWidget(mk(QString::fromUtf8("CASC · 12.0.7.68887"), monoF(), 8, tok::kMuted));
  row->addWidget(pill);

  for (const char* c : {"#2a3038", "#2a3038", "#3a2a2a"}) {
    auto* d = new QLabel;
    d->setFixedSize(11, 11);
    d->setStyleSheet(QString("background:%1; border:none; border-radius:5px;").arg(c));
    row->addWidget(d);
  }
  return w;
}

// --- tool bar --------------------------------------------------------------

static QWidget* toolBar()
{
  auto* w = new QWidget;
  styled(w);
  w->setFixedHeight(30);
  w->setStyleSheet(QString("background:#0f1216; border-bottom:1px solid %1;").arg(tok::kBorder2));
  auto* row = new QHBoxLayout(w);
  row->setContentsMargins(12, 0, 12, 0);
  row->setSpacing(6);

  const char* labels[] = {"Modell", "Charakter", "Licht", "Kamera", "Hintergrund", "Effekte"};
  for (int i = 0; i < 6; ++i) {
    auto* item = new QWidget;
    item->setStyleSheet("background:transparent;");
    auto* ir = new QHBoxLayout(item);
    ir->setContentsMargins(9, 0, 9, 0);
    ir->setSpacing(6);
    ir->addWidget(mk(QString("Alt+%1").arg(i + 1), monoF(), 8, "#6d7683"));
    ir->addWidget(mk(QString::fromLatin1(labels[i]), uiF(), 8, "#9aa3b0"));
    row->addWidget(item);
  }
  row->addStretch(1);
  row->addWidget(mk("models/character/orc/male/orcmale_hd.m2", monoF(), 8, tok::kDim));
  return w;
}

// --- left: asset browser ---------------------------------------------------

static QWidget* browser()
{
  auto* w = new QWidget;
  styled(w);
  w->setFixedWidth(288);
  w->setStyleSheet(QString("background:%1; border-right:1px solid %2;").arg(tok::kPanel).arg(tok::kBorder2));
  auto* col = new QVBoxLayout(w);
  col->setContentsMargins(0, 0, 0, 0);
  col->setSpacing(0);

  auto* searchWrap = new QWidget;
  searchWrap->setStyleSheet("background:transparent;");
  auto* sw = new QHBoxLayout(searchWrap);
  sw->setContentsMargins(12, 12, 12, 8);
  auto* search = new QLineEdit;
  search->setPlaceholderText(QString::fromUtf8("Modelle, Items, NPCs …"));
  search->setFixedHeight(32);
  search->setFont(QFont(uiF(), 9));
  search->setStyleSheet(QString(
    "QLineEdit { background:%1; border:1px solid %2; border-radius:7px; padding:0 10px; color:%3; }"
    "QLineEdit:focus { border-color:#3a434f; }").arg(tok::kCard).arg(tok::kBorder).arg(tok::kText));
  sw->addWidget(search);
  col->addWidget(searchWrap);

  auto* cats = new QWidget;
  cats->setStyleSheet("background:transparent;");
  auto* cr = new QHBoxLayout(cats);
  cr->setContentsMargins(12, 0, 12, 10);
  cr->setSpacing(5);
  const char* catNames[] = {"Alle", "Charaktere", "Kreaturen", "Items"};
  for (int i = 0; i < 4; ++i)
    cr->addWidget(chip(QString::fromLatin1(catNames[i]), i == 0, uiF(), 8));
  cr->addStretch(1);
  col->addWidget(cats);

  auto* listHost = new QWidget;
  listHost->setStyleSheet("background:transparent;");
  auto* lr = new QVBoxLayout(listHost);
  lr->setContentsMargins(8, 0, 8, 8);
  lr->setSpacing(2);
  auto* hdr = mk(QString::fromUtf8("ERGEBNISSE · 10"), uiF(), 7, tok::kDim, false, 1.3);
  hdr->setContentsMargins(6, 6, 6, 8);
  lr->addWidget(hdr);

  const struct { const char* n; const char* m; const char* g; const char* q; } rows[] = {
    {"Orc Male HD", "1 000 001 · M2 · Charakter", "O", "#e8eaee"},
    {"Gruhlmok (Armory)", "6 798 783 · M2 · Charakter", "G", "#c8a15a"},
    {"Ashbringer", "133 800 · M2 · Waffe", "A", "#ff8000"},
    {"Helm der Grimmigen Nacht", "141 205 · M2 · Kopf", "H", "#a335ee"},
    {"Frostwyrm", "31 040 · M2 · Kreatur", "F", "#e8eaee"},
    {"Sturmgreif", "28 776 · M2 · Reittier", "S", "#0070dd"},
    {"Murloc Tidehunter", "12 004 · M2 · Kreatur", "M", "#e8eaee"},
    {"Netherdrache", "20 461 · M2 · Reittier", "N", "#a335ee"}
  };
  int idx = 0;
  for (const auto& r : rows) {
    const bool sel = (idx == 1);
    auto* row = new QFrame;
    row->setStyleSheet(sel
      ? QString("QFrame { background:#181510; border:1px solid %1; border-radius:7px; }").arg(tok::kAccentBr)
      : QString("QFrame { background:transparent; border:1px solid transparent; border-radius:7px; }"));
    auto* rr = new QHBoxLayout(row);
    rr->setContentsMargins(8, 8, 8, 8);
    rr->setSpacing(10);

    auto* thumb = new QLabel(QString::fromLatin1(r.g));
    thumb->setFixedSize(32, 32);
    thumb->setAlignment(Qt::AlignCenter);
    thumb->setFont(QFont(dispF(), 9));
    thumb->setStyleSheet(QString(
      "background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #232b36, stop:1 #131820);"
      "border:1px solid %1; border-radius:6px; color:#7b8494;")
      .arg(sel ? "#4a3f28" : "#252c36"));
    rr->addWidget(thumb);

    auto* tc = new QVBoxLayout;
    tc->setSpacing(2);
    auto* n = new ElidedLabel(QString::fromUtf8(r.n));
    n->setFont(QFont(uiF(), 9));
    n->setStyleSheet(QString("color:%1; background:transparent; border:none;").arg(r.q));
    auto* m = new ElidedLabel(QString::fromUtf8(r.m));
    m->setFont(QFont(monoF(), 7));
    m->setStyleSheet(QString("color:%1; background:transparent; border:none;").arg(tok::kDim));
    tc->addWidget(n);
    tc->addWidget(m);
    rr->addLayout(tc, 1);

    if (sel) {
      auto* d = new QLabel;
      d->setFixedSize(5, 5);
      d->setStyleSheet(QString("background:%1; border:none; border-radius:2px;").arg(tok::kAccent));
      rr->addWidget(d);
    }
    lr->addWidget(row);
    ++idx;
  }
  lr->addStretch(1);
  col->addWidget(listHost, 1);

  auto* foot = new QWidget;
  styled(foot);
  foot->setStyleSheet(QString("background:transparent; border-top:1px solid %1;").arg(tok::kBorder2));
  auto* fr = new QHBoxLayout(foot);
  fr->setContentsMargins(14, 10, 14, 10);
  fr->addWidget(mk("Datenquelle", uiF(), 8, tok::kDim));
  fr->addStretch(1);
  fr->addWidget(mk(QString::fromUtf8("listfile · 1.42 M"), monoF(), 8, tok::kMuted));
  col->addWidget(foot);
  return w;
}

// --- centre: viewport + timeline ------------------------------------------

static QFrame* hudBox(QWidget* inner)
{
  auto* f = new QFrame;
  f->setStyleSheet(QString(
    "QFrame { background: rgba(14,17,20,200); border:1px solid %1; border-radius:8px; }").arg(tok::kBorder));
  auto* l = new QHBoxLayout(f);
  l->setContentsMargins(12, 6, 12, 6);
  l->addWidget(inner);
  return f;
}

static QWidget* viewport()
{
  auto* w = new QWidget;
  styled(w);
  w->setStyleSheet(
    "background: qradialgradient(cx:0.5, cy:0.3, radius:0.95, fx:0.5, fy:0.3,"
    " stop:0 #202a35, stop:0.45 #10141a, stop:1 #080a0d);");
  auto* g = new QGridLayout(w);
  g->setContentsMargins(14, 14, 14, 14);

  auto* rail = new QFrame;
  rail->setStyleSheet(QString("QFrame { background: rgba(14,17,20,200); border:1px solid %1;"
                              " border-radius:9px; }").arg(tok::kBorder));
  auto* rl = new QVBoxLayout(rail);
  rl->setContentsMargins(6, 6, 6, 6);
  rl->setSpacing(6);
  const char* glyphs[] = {"◎", "✥", "⤢", "▦", "☀"};
  for (int i = 0; i < 5; ++i)
    rl->addWidget(icon(QString::fromUtf8(glyphs[i]), 30,
                       i == 0 ? tok::kAccent : tok::kMuted,
                       i == 0 ? "#22282f" : "transparent"));
  g->addWidget(rail, 0, 0, Qt::AlignTop | Qt::AlignLeft);

  auto* actions = new QWidget;
  actions->setStyleSheet("background:transparent;");
  auto* ar = new QHBoxLayout(actions);
  ar->setContentsMargins(0, 0, 0, 0);
  ar->setSpacing(8);
  ar->addWidget(hudBox(mk("Screenshot", uiF(), 9, "#cdd3dc")));
  auto* exp = new QLabel("Exportieren");
  exp->setFont(QFont(uiF(), 9, QFont::DemiBold));
  exp->setAlignment(Qt::AlignCenter);
  exp->setStyleSheet(QString("background:%1; border:1px solid #d9b678; border-radius:7px;"
                             "color:%2; padding:8px 14px;").arg(tok::kAccent).arg(tok::kOnAccent));
  ar->addWidget(exp);
  g->addWidget(actions, 0, 2, Qt::AlignTop | Qt::AlignRight);

  auto* drop = mk(QString::fromUtf8("Render hier ablegen"), uiF(), 10, "#3f4650");
  drop->setAlignment(Qt::AlignCenter);
  g->addWidget(drop, 1, 1, Qt::AlignCenter);

  auto* stats = new QWidget;
  stats->setStyleSheet("background:transparent;");
  auto* sr = new QHBoxLayout(stats);
  sr->setContentsMargins(0, 0, 0, 0);
  sr->setSpacing(16);
  for (const char* s : {"144 FPS", "38 214 TRIS", "6 MAT", "2048² TEX"})
    sr->addWidget(mk(QString::fromUtf8(s), monoF(), 8, "#7d8693"));
  g->addWidget(hudBox(stats), 2, 0, Qt::AlignBottom | Qt::AlignLeft);

  auto* cams = new QWidget;
  cams->setStyleSheet("background:transparent;");
  auto* cr = new QHBoxLayout(cams);
  cr->setContentsMargins(0, 0, 0, 0);
  cr->setSpacing(6);
  const char* presets[] = {"Vorn", "3/4", "Seite", "Oben"};
  for (int i = 0; i < 4; ++i) {
    auto* c = new QLabel(QString::fromLatin1(presets[i]));
    c->setAlignment(Qt::AlignCenter);
    c->setFont(QFont(uiF(), 8));
    c->setStyleSheet(i == 1
      ? QString("background: rgba(40,32,14,220); border:1px solid #4a3f28; border-radius:6px;"
                "color:%1; padding:6px 11px;").arg(tok::kAccent)
      : QString("background: rgba(14,17,20,200); border:1px solid %1; border-radius:6px;"
                "color:#98a1ae; padding:6px 11px;").arg(tok::kBorder));
    cr->addWidget(c);
  }
  g->addWidget(cams, 2, 2, Qt::AlignBottom | Qt::AlignRight);

  g->setColumnStretch(1, 1);
  g->setRowStretch(1, 1);
  return w;
}

static QWidget* timeline()
{
  auto* w = new QWidget;
  styled(w);
  w->setFixedHeight(104);
  w->setStyleSheet(QString("background:%1; border-top:1px solid %2;").arg(tok::kPanel).arg(tok::kBorder2));
  auto* col = new QVBoxLayout(w);
  col->setContentsMargins(16, 12, 16, 14);
  col->setSpacing(11);

  auto* top = new QHBoxLayout;
  top->setSpacing(14);

  auto* transport = new QWidget;
  transport->setStyleSheet("background:transparent;");
  auto* tr = new QHBoxLayout(transport);
  tr->setContentsMargins(0, 0, 0, 0);
  tr->setSpacing(4);
  tr->addWidget(icon(QString::fromUtf8("⏮"), 28, "#98a1ae", "transparent"));
  auto* play = icon(QString::fromUtf8("⏸"), 34, tok::kOnAccent, tok::kAccent);
  play->setStyleSheet(QString("color:%1; background:%2; border:none; border-radius:8px;")
                      .arg(tok::kOnAccent).arg(tok::kAccent));
  tr->addWidget(play);
  tr->addWidget(icon(QString::fromUtf8("⏭"), 28, "#98a1ae", "transparent"));
  tr->addWidget(icon(QString::fromUtf8("↻"), 28, tok::kAccent, "transparent"));
  top->addWidget(transport);

  auto* anim = new QFrame;
  anim->setFixedHeight(28);
  anim->setMinimumWidth(190);
  anim->setStyleSheet(QString("QFrame { background:%1; border:1px solid %2; border-radius:6px; }")
                      .arg(tok::kCard).arg(tok::kBorder));
  auto* anr = new QHBoxLayout(anim);
  anr->setContentsMargins(10, 0, 10, 0);
  anr->setSpacing(8);
  anr->addWidget(mk("ANIM", uiF(), 7, tok::kDim, false, 1.2));
  anr->addWidget(mk("Stand [0]", uiF(), 9, tok::kText, true));
  anr->addStretch(1);
  anr->addWidget(mk(QString::fromUtf8("▾"), uiF(), 8, "#6f7885"));
  top->addWidget(anim);

  top->addWidget(mk(QString::fromUtf8("00:00.816 / 00:02.400"), monoF(), 9, "#98a1ae"));
  top->addStretch(1);
  top->addWidget(mk("Tempo", uiF(), 8, tok::kDim));
  auto* speeds = new QHBoxLayout;
  speeds->setSpacing(3);
  const char* sp[] = {"0.25x", "0.5x", "1x", "2x"};
  for (int i = 0; i < 4; ++i)
    speeds->addWidget(chip(QString::fromLatin1(sp[i]), i == 2, monoF(), 8));
  top->addLayout(speeds);
  col->addLayout(top);

  col->addWidget(new TrackWidget(0.34));
  return w;
}

// --- right: inspector ------------------------------------------------------

static QWidget* inspector()
{
  auto* w = new QWidget;
  styled(w);
  w->setFixedWidth(324);
  w->setStyleSheet(QString(
    "QWidget { background:%1; }"
    "QLabel { background:transparent; border:none; }"
    "QFrame#card { background:%2; border:1px solid %3; border-radius:8px; }")
    .arg(tok::kPanel).arg(tok::kCard).arg(tok::kBorder));
  auto* col = new QVBoxLayout(w);
  col->setContentsMargins(0, 0, 0, 0);
  col->setSpacing(0);

  auto* tabs = new QWidget;
  tabs->setStyleSheet("background:transparent;");
  auto* tr = new QHBoxLayout(tabs);
  tr->setContentsMargins(0, 0, 0, 0);
  tr->setSpacing(0);
  const char* names[] = {"Charakter", "Material", "Export"};
  for (int i = 0; i < 3; ++i) {
    auto* t = new QLabel(QString::fromLatin1(names[i]));
    t->setFixedHeight(38);
    t->setAlignment(Qt::AlignCenter);
    t->setFont(QFont(uiF(), 9, i == 0 ? QFont::DemiBold : QFont::Normal));
    t->setStyleSheet(i == 0
      ? QString("color:%1; background:transparent; border:none; border-bottom:2px solid %2;")
          .arg(tok::kText).arg(tok::kAccent)
      : QString("color:#7d8693; background:transparent; border:none; border-bottom:1px solid %1;")
          .arg(tok::kBorder2));
    tr->addWidget(t, 1);
  }
  col->addWidget(tabs);

  auto* body = new QWidget;
  body->setStyleSheet("background:transparent;");
  auto* bc = new QVBoxLayout(body);
  bc->setContentsMargins(16, 16, 16, 24);
  bc->setSpacing(20);

  auto* card = new QFrame;
  card->setObjectName("card");
  auto* cr = new QHBoxLayout(card);
  cr->setContentsMargins(12, 12, 12, 12);
  cr->setSpacing(12);
  auto* av = new QLabel("G");
  av->setFixedSize(46, 46);
  av->setAlignment(Qt::AlignCenter);
  av->setFont(QFont(dispF(), 13, QFont::DemiBold));
  av->setStyleSheet(QString("background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
    "stop:0 #2a3340, stop:1 #151a21); border:1px solid #2c333d; border-radius:8px;"
    "color:%1;").arg(tok::kAccent));
  cr->addWidget(av);
  auto* nc = new QVBoxLayout;
  nc->setSpacing(2);
  auto* nm = new ElidedLabel("Gruhlmok");
  nm->setFont(QFont(uiF(), 10, QFont::DemiBold));
  nm->setStyleSheet(QString("color:%1; background:transparent; border:none;").arg(tok::kText));
  auto* sb = new ElidedLabel(QString::fromUtf8("Mag'har Orc · Todesritter"));
  sb->setFont(QFont(uiF(), 8));
  sb->setStyleSheet("color:#7d8693; background:transparent; border:none;");
  nc->addWidget(nm);
  nc->addWidget(sb);
  cr->addLayout(nc, 1);
  auto* arm = new QLabel("Armory");
  arm->setFont(QFont(uiF(), 8));
  arm->setAlignment(Qt::AlignCenter);
  arm->setStyleSheet(QString("color:%1; background:%2; border:1px solid %3; border-radius:5px;"
                             "padding:3px 8px;").arg(tok::kAccent).arg(tok::kAccentBg).arg(tok::kAccentBr));
  cr->addWidget(arm);
  bc->addWidget(card);

  auto* cust = new QVBoxLayout;
  cust->setSpacing(12);
  cust->addWidget(mk(QString::fromUtf8("ANPASSUNG"), uiF(), 7, tok::kDim, false, 1.4));
  const struct { const char* l; const char* v; int p; } sl[] = {
    {"Hautfarbe", "04", 38}, {"Gesicht", "07", 62}, {"Frisur", "11", 84},
    {"Haarfarbe", "02", 22}, {"Hauer", "05", 45}
  };
  for (const auto& s : sl) {
    auto* rw = new QWidget;
    rw->setStyleSheet("background:transparent;");
    auto* rc = new QVBoxLayout(rw);
    rc->setContentsMargins(0, 0, 0, 0);
    rc->setSpacing(6);
    auto* hd = new QHBoxLayout;
    hd->addWidget(mk(QString::fromLatin1(s.l), uiF(), 9, tok::kTextSoft));
    hd->addStretch(1);
    hd->addWidget(mk(QString::fromLatin1(s.v), monoF(), 8, "#7d8693"));
    rc->addLayout(hd);
    auto* q = new QSlider(Qt::Horizontal);
    q->setRange(0, 100);
    q->setValue(s.p);
    q->setFixedHeight(12);
    q->setStyleSheet(
      "QSlider::groove:horizontal { height:4px; border-radius:2px; background:#1c222a; }"
      "QSlider::sub-page:horizontal { height:4px; border-radius:2px; background:#6f7f92; }"
      "QSlider::handle:horizontal { width:12px; height:12px; margin:-4px 0; border-radius:6px;"
      "background:#cdd3dc; border:2px solid #0e1114; }");
    rc->addWidget(q);
    cust->addWidget(rw);
  }
  bc->addLayout(cust);

  auto* eq = new QVBoxLayout;
  eq->setSpacing(10);
  eq->addWidget(mk(QString::fromUtf8("AUSRÜSTUNG"), uiF(), 7, tok::kDim, false, 1.4));
  auto* grid = new QGridLayout;
  grid->setSpacing(7);
  const struct { const char* s; const char* i; const char* q; } eqs[] = {
    {"Kopf", "Grimmige Nacht", "#a335ee"},
    {"Schulter", "Ewige Wacht", "#0070dd"},
    {"Brust", "Kriegsplatte", "#a335ee"},
    {"Hände", "Schlächterfäuste", "#0070dd"},
    {"Gürtel", "Gurt des Zorns", "#1eff00"},
    {"Beine", "Beinschienen", "#a335ee"},
    {"Waffe", "Ashbringer", "#ff8000"},
    {"Schild", "Bollwerk", "#a335ee"}
  };
  int n = 0;
  for (const auto& e : eqs) {
    auto* f = new QFrame;
    f->setObjectName("card");
    auto* fr = new QHBoxLayout(f);
    fr->setContentsMargins(7, 7, 7, 7);
    fr->setSpacing(8);
    auto* sw = new QLabel;
    sw->setFixedSize(26, 26);
    sw->setStyleSheet(QString("background:#10141a; border:1px solid %1; border-radius:5px;").arg(e.q));
    fr->addWidget(sw);
    auto* tc = new QVBoxLayout;
    tc->setSpacing(1);
    tc->addWidget(mk(QString::fromUtf8(e.s).toUpper(), uiF(), 7, tok::kDim, false, 0.7));
    auto* it = new ElidedLabel(QString::fromUtf8(e.i));
    it->setFont(QFont(uiF(), 8));
    it->setStyleSheet(QString("color:%1; background:transparent; border:none;").arg(e.q));
    tc->addWidget(it);
    fr->addLayout(tc, 1);
    grid->addWidget(f, n / 2, n % 2);
    ++n;
  }
  eq->addLayout(grid);
  bc->addLayout(eq);
  bc->addStretch(1);

  auto* scroll = new QScrollArea;
  scroll->setWidget(body);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scroll->setStyleSheet(
    "QScrollArea { background:transparent; border:none; }"
    "QScrollBar:vertical { background:#0e1114; width:10px; }"
    "QScrollBar::handle:vertical { background:#262c35; border-radius:5px; min-height:30px; }"
    "QScrollBar::add-line, QScrollBar::sub-line { height:0; }");
  col->addWidget(scroll, 1);
  return w;
}

static QWidget* statusBar()
{
  auto* w = new QWidget;
  styled(w);
  w->setFixedHeight(24);
  w->setStyleSheet(QString("background:#0f1216; border-top:1px solid %1;").arg(tok::kBorder2));
  auto* r = new QHBoxLayout(w);
  r->setContentsMargins(14, 0, 14, 0);
  r->setSpacing(18);
  r->addWidget(mk("Bereit", monoF(), 7, tok::kDim));
  r->addWidget(mk("deDE", monoF(), 7, tok::kDim));
  r->addWidget(mk("models/character/orc/male/orcmale_hd.m2", monoF(), 7, tok::kDim));
  r->addStretch(1);
  r->addWidget(mk(QString::fromUtf8("FBX · OBJ · glTF"), monoF(), 7, tok::kDim));
  r->addWidget(mk("v0.11.0", monoF(), 7, "#7d8693"));
  return w;
}

int main(int argc, char** argv)
{
  QApplication app(argc, argv);

  QString shot;
  for (int i = 1; i < argc - 1; ++i)
    if (QString(argv[i]) == "--shot")
      shot = QString::fromLocal8Bit(argv[i + 1]);

  auto* win = new QWidget;
  styled(win);
  win->setStyleSheet(QString("QWidget { background:%1; } QLabel { border:none; }").arg(tok::kApp));
  win->resize(1480, 900);

  auto* col = new QVBoxLayout(win);
  col->setContentsMargins(0, 0, 0, 0);
  col->setSpacing(0);
  col->addWidget(titleBar());
  col->addWidget(toolBar());

  auto* mid = new QWidget;
  mid->setStyleSheet("background:transparent;");
  auto* mr = new QHBoxLayout(mid);
  mr->setContentsMargins(0, 0, 0, 0);
  mr->setSpacing(0);
  mr->addWidget(browser());

  auto* centre = new QWidget;
  centre->setStyleSheet("background:transparent;");
  auto* cc = new QVBoxLayout(centre);
  cc->setContentsMargins(0, 0, 0, 0);
  cc->setSpacing(0);
  cc->addWidget(viewport(), 1);
  cc->addWidget(timeline());
  mr->addWidget(centre, 1);
  mr->addWidget(inspector());
  col->addWidget(mid, 1);
  col->addWidget(statusBar());

  if (!shot.isEmpty()) {
    win->show();
    app.processEvents();
    win->grab().save(shot);
    return 0;
  }
  win->setWindowTitle("WMV Shell - Qt Prototype");
  win->show();
  return app.exec();
}
