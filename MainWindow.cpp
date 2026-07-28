#include "MainWindow.h"

#include <QFontDatabase>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTreeView>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSlider>
#include <QVBoxLayout>

#include "FileTreeModel.h"
#include "GLHost.h"

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
static QString iconF() { static QString f = pick({"Segoe UI Symbol", "Segoe UI"}, "sans-serif"); return f; }

static void styled(QWidget* w) { w->setAttribute(Qt::WA_StyledBackground, true); }

// A HUD element floating over the GL canvas. GLHost is a native child window, so
// ordinary sibling widgets are painted UNDER it by the compositor. Making the HUD
// native too gives it its own HWND, which sits above the GL surface in z-order.
static void asOverlay(QWidget* w)
{
  w->setAttribute(Qt::WA_NativeWindow, true);
  w->raise();
}

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

// ---------------------------------------------------------------------------

MainWindow::MainWindow()
{
  styled(this);
  setStyleSheet(QString("QWidget { background:%1; } QLabel { border:none; }").arg(tok::kApp));
  setWindowTitle("WoW Model Viewer");
  resize(1480, 900);

  auto* col = new QVBoxLayout(this);
  col->setContentsMargins(0, 0, 0, 0);
  col->setSpacing(0);
  col->addWidget(buildTitleBar());
  col->addWidget(buildToolBar());

  auto* mid = new QWidget;
  mid->setStyleSheet("background:transparent;");
  auto* mr = new QHBoxLayout(mid);
  mr->setContentsMargins(0, 0, 0, 0);
  mr->setSpacing(0);
  mr->addWidget(buildBrowser());

  auto* centre = new QWidget;
  centre->setStyleSheet("background:transparent;");
  auto* cc = new QVBoxLayout(centre);
  cc->setContentsMargins(0, 0, 0, 0);
  cc->setSpacing(0);
  cc->addWidget(buildViewport(), 1);
  cc->addWidget(buildTimeline());
  mr->addWidget(centre, 1);
  mr->addWidget(buildInspector());

  col->addWidget(mid, 1);
  col->addWidget(buildStatusBar());
}

void MainWindow::setBuildLabel(const QString& text)
{
  if (buildLabel_)
    buildLabel_->setText(text);
}

void MainWindow::setPathLabel(const QString& text)
{
  if (pathLabel_)
    pathLabel_->setText(text);
  if (statusPathLabel_)
    statusPathLabel_->setText(text);
}

QWidget* MainWindow::buildTitleBar()
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
  buildLabel_ = mk("CASC", monoF(), 8, tok::kMuted);
  pr->addWidget(buildLabel_);
  row->addWidget(pill);

  for (const char* c : {"#2a3038", "#2a3038", "#3a2a2a"}) {
    auto* d = new QLabel;
    d->setFixedSize(11, 11);
    d->setStyleSheet(QString("background:%1; border:none; border-radius:5px;").arg(c));
    row->addWidget(d);
  }
  return w;
}

QWidget* MainWindow::buildToolBar()
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
  pathLabel_ = mk("", monoF(), 8, tok::kDim);
  row->addWidget(pathLabel_);
  return w;
}

QWidget* MainWindow::buildBrowser()
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
  search_ = search;
  connect(search, &QLineEdit::returnPressed, this, [this]() { populateTree(); });
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
  resultLabel_ = mk(QString::fromUtf8("ERGEBNISSE"), uiF(), 7, tok::kDim, false, 1.3);
  resultLabel_->setContentsMargins(6, 6, 6, 8);
  lr->addWidget(resultLabel_);

  // The real tree. No lazy expansion, no freeze/thaw: the view only asks the model
  // about rows it is about to paint.
  treeModel_ = new FileTreeModel(this);
  tree_ = new QTreeView;
  tree_->setModel(treeModel_);
  tree_->setHeaderHidden(true);
  tree_->setUniformRowHeights(true);      // lets the view skip per-row size queries
  tree_->setFont(QFont(uiF(), 8));
  tree_->setStyleSheet(QString(
    "QTreeView { background:transparent; border:none; outline:none; }"
    "QTreeView::item { padding:3px 2px; border-radius:4px; }"
    "QTreeView::item:hover { background:#181d23; }"
    "QTreeView::item:selected { background:#181510; color:%1; }"
    "QScrollBar:vertical { background:transparent; width:10px; }"
    "QScrollBar::handle:vertical { background:#262c35; border-radius:5px; min-height:30px; }"
    "QScrollBar::add-line, QScrollBar::sub-line { height:0; }").arg(tok::kAccent));
  connect(tree_, &QTreeView::activated, this, &MainWindow::onTreeActivated);
  lr->addWidget(tree_, 1);
  col->addWidget(listHost, 1);

  auto* foot0 = new QWidget;
  styled(foot0);
  foot0->setStyleSheet(QString("background:transparent; border-top:1px solid %1;").arg(tok::kBorder2));
  auto* fr0 = new QHBoxLayout(foot0);
  fr0->setContentsMargins(14, 10, 14, 10);
  fr0->addWidget(mk("Datenquelle", uiF(), 8, tok::kDim));
  fr0->addStretch(1);
  fr0->addWidget(mk("listfile", monoF(), 8, tok::kMuted));
  col->addWidget(foot0);
  return w;
}

void MainWindow::populateTree()
{
  if (!treeModel_)
    return;
  const int n = treeModel_->rebuild("m2", search_ ? search_->text() : QString());
  if (resultLabel_)
    resultLabel_->setText(QString::fromUtf8("ERGEBNISSE · %1").arg(n));
}

void MainWindow::onSearchChanged(const QString&)
{
  populateTree();
}

void MainWindow::onTreeActivated(const QModelIndex& index)
{
  if (GameFile* f = treeModel_->fileAt(index))
    emit fileActivated(f);
}

QWidget* MainWindow::buildViewport()
{
  auto* host = new QWidget;
  styled(host);
  host->setStyleSheet("background:#080a0d;");

  auto* g = new QGridLayout(host);
  g->setContentsMargins(14, 14, 14, 14);

  // The GL canvas fills the whole cell; the HUD sits on top of it in the same cells.
  canvas_ = new GLHost(host);
  g->addWidget(canvas_, 0, 0, 3, 3);

  auto* rail = new QFrame(host);
  rail->setStyleSheet(QString("QFrame { background: rgba(14,17,20,220); border:1px solid %1;"
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
  asOverlay(rail);

  auto* actions = new QWidget(host);
  actions->setStyleSheet("background:transparent;");
  auto* ar = new QHBoxLayout(actions);
  ar->setContentsMargins(0, 0, 0, 0);
  ar->setSpacing(8);
  auto* shot = new QFrame;
  shot->setStyleSheet(QString("QFrame { background: rgba(14,17,20,220); border:1px solid %1;"
                              " border-radius:8px; }").arg(tok::kBorder));
  auto* sl = new QHBoxLayout(shot);
  sl->setContentsMargins(12, 6, 12, 6);
  sl->addWidget(mk("Screenshot", uiF(), 9, "#cdd3dc"));
  ar->addWidget(shot);
  auto* exp = new QLabel("Exportieren");
  exp->setFont(QFont(uiF(), 9, QFont::DemiBold));
  exp->setAlignment(Qt::AlignCenter);
  exp->setStyleSheet(QString("background:%1; border:1px solid #d9b678; border-radius:7px;"
                             "color:%2; padding:8px 14px;").arg(tok::kAccent).arg(tok::kOnAccent));
  ar->addWidget(exp);
  g->addWidget(actions, 0, 2, Qt::AlignTop | Qt::AlignRight);
  asOverlay(actions);

  auto* stats = new QFrame(host);
  stats->setStyleSheet(QString("QFrame { background: rgba(14,17,20,220); border:1px solid %1;"
                               " border-radius:8px; }").arg(tok::kBorder));
  auto* sr = new QHBoxLayout(stats);
  sr->setContentsMargins(12, 6, 12, 6);
  sr->setSpacing(16);
  for (const char* s : {"60 FPS", "M2", "GL 4.6"})
    sr->addWidget(mk(QString::fromUtf8(s), monoF(), 8, "#7d8693"));
  g->addWidget(stats, 2, 0, Qt::AlignBottom | Qt::AlignLeft);
  asOverlay(stats);

  auto* cams = new QWidget(host);
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
      ? QString("background: rgba(40,32,14,235); border:1px solid #4a3f28; border-radius:6px;"
                "color:%1; padding:6px 11px;").arg(tok::kAccent)
      : QString("background: rgba(14,17,20,220); border:1px solid %1; border-radius:6px;"
                "color:#98a1ae; padding:6px 11px;").arg(tok::kBorder));
    cr->addWidget(c);
  }
  g->addWidget(cams, 2, 2, Qt::AlignBottom | Qt::AlignRight);
  asOverlay(cams);

  g->setColumnStretch(1, 1);
  g->setRowStretch(1, 1);
  return host;
}

QWidget* MainWindow::buildTimeline()
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
  top->addStretch(1);
  top->addWidget(mk("Tempo", uiF(), 8, tok::kDim));
  auto* speeds = new QHBoxLayout;
  speeds->setSpacing(3);
  const char* sp[] = {"0.25x", "0.5x", "1x", "2x"};
  for (int i = 0; i < 4; ++i)
    speeds->addWidget(chip(QString::fromLatin1(sp[i]), i == 2, monoF(), 8));
  top->addLayout(speeds);
  col->addLayout(top);

  auto* track = new QFrame;
  track->setFixedHeight(40);
  track->setStyleSheet(QString("QFrame { background:#0a0d10; border:1px solid %1; border-radius:6px; }")
                       .arg(tok::kBorder2));
  col->addWidget(track);
  return w;
}

QWidget* MainWindow::buildInspector()
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
  bc->addWidget(mk(QString::fromUtf8("ANPASSUNG"), uiF(), 7, tok::kDim, false, 1.4));

  // Placeholder sliders -- Phase 5 binds these to CharDetails.
  const struct { const char* l; const char* v; int p; } sl[] = {
    {"Hautfarbe", "04", 38}, {"Gesicht", "07", 62}, {"Frisur", "11", 84}
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
    bc->addWidget(rw);
  }
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

QWidget* MainWindow::buildStatusBar()
{
  auto* w = new QWidget;
  styled(w);
  w->setFixedHeight(24);
  w->setStyleSheet(QString("background:#0f1216; border-top:1px solid %1;").arg(tok::kBorder2));
  auto* r = new QHBoxLayout(w);
  r->setContentsMargins(14, 0, 14, 0);
  r->setSpacing(18);
  r->addWidget(mk("Bereit", monoF(), 7, tok::kDim));
  statusPathLabel_ = mk("", monoF(), 7, tok::kDim);
  r->addWidget(statusPathLabel_);
  r->addStretch(1);
  r->addWidget(mk(QString::fromUtf8("FBX · OBJ · glTF"), monoF(), 7, tok::kDim));
  return w;
}
