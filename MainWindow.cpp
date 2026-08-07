#include "MainWindow.h"

#include <QFontDatabase>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QSizeGrip>
#include <QLineEdit>
#include <QMenuBar>
#include <QTreeView>
#include <QResizeEvent>
#include <QScrollArea>
#include <QStackedWidget>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

#include "CharacterPanel.h"
#include "FileTreeModel.h"
#include "GLHost.h"
#include "InspectorTabs.h"
#include "ItemBrowser.h"
#include "LightPanel.h"
#include "TimelinePanel.h"

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

// Every inspector page scrolls; they are all longer than the column.
static QWidget* wrapScroll(QWidget* body)
{
  auto* s = new QScrollArea;
  s->setWidget(body);
  s->setWidgetResizable(true);
  s->setFrameShape(QFrame::NoFrame);
  s->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  s->setStyleSheet(
    "QScrollArea { background:transparent; border:none; }"
    "QScrollBar:vertical { background:#0e1114; width:10px; }"
    "QScrollBar::handle:vertical { background:#262c35; border-radius:5px; min-height:30px; }"
    "QScrollBar::add-line, QScrollBar::sub-line { height:0; }");
  return s;
}

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
  // The universal QWidget rule is what gives the whole window its background -- but a
  // stylesheet set on a window also applies to every DIALOG parented to it, and a
  // stylesheet background beats the palette. Without the QDialog rules below, the input
  // field of an armory import came up with a near-black background from this very rule and
  // the platform's default black text on top of it.
  setStyleSheet(QString(
    "QWidget { background:%1; } QLabel { border:none; }"
    "QDialog { background:%2; }"
    "QDialog QLabel { color:%3; background:transparent; }"
    "QDialog QLineEdit, QDialog QComboBox, QDialog QAbstractSpinBox,"
    " QDialog QPlainTextEdit, QDialog QTextEdit {"
    " background:%4; color:%3; border:1px solid %5; border-radius:5px; padding:4px 7px;"
    " selection-background-color:%6; selection-color:%7; }"
    "QDialog QAbstractItemView { background:%4; color:%3; border:1px solid %5;"
    " selection-background-color:%6; selection-color:%7; }"
    "QDialog QPushButton { background:#1c2229; color:%3; border:1px solid %5;"
    " border-radius:5px; padding:5px 15px; min-width:74px; }"
    "QDialog QPushButton:hover { background:#232a33; }"
    "QDialog QPushButton:default { border-color:%6; }")
    .arg(tok::kApp).arg(tok::kPanel).arg(tok::kText).arg("#0f1216")
    .arg(tok::kBorder).arg(tok::kAccent).arg(tok::kOnAccent));
  // The version belongs in the title: a screenshot in a bug report then carries it
  // without the reporter having to look it up.
  setWindowTitle(QString("better Model Viewer %1").arg(WMV_QT_VERSION));
  resize(1480, 900);

  // The design has its own title bar, so the native frame goes away. On Windows this
  // costs Aero Snap and edge resizing, which the frame provided for free -- dragging
  // is reimplemented on the title bar below and a size grip sits in the corner.
  setWindowFlags(Qt::Window | Qt::FramelessWindowHint);

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

  // A real QMenuBar rather than the mock-up's five labels: it brings hover states,
  // Alt mnemonics, keyboard navigation and window-wide shortcuts with it. The menus
  // themselves are filled in by MenuController, which needs objects main() only has
  // once the game data and the plugins are loaded.
  //
  // setNativeMenuBar(false) keeps it inside our own title bar on platforms that would
  // otherwise lift it into a system menu bar.
  menuBar_ = new QMenuBar;
  menuBar_->setNativeMenuBar(false);
  menuBar_->setFont(QFont(uiF(), 9));
  menuBar_->setStyleSheet(QString(
    "QMenuBar { background:transparent; border:none; color:#99a2af; }"
    "QMenuBar::item { background:transparent; padding:5px 9px; margin:0 1px;"
    " border-radius:5px; }"
    "QMenuBar::item:selected { background:#1c2229; color:%1; }"
    "QMenuBar::item:pressed { background:%2; color:%3; }"
    "QMenu { background:%4; border:1px solid %5; padding:5px; color:#cdd3dc; }"
    "QMenu::item { padding:5px 30px 5px 24px; border-radius:5px; }"
    "QMenu::item:selected { background:%2; color:%3; }"
    "QMenu::item:disabled { color:#4c545e; background:transparent; }"
    "QMenu::separator { height:1px; background:%5; margin:5px 8px; }"
    "QMenu::indicator { width:12px; height:12px; left:7px; }")
    .arg(tok::kText).arg(tok::kAccentBg).arg(tok::kAccent)
    .arg(tok::kCard).arg(tok::kBorder));
  row->addWidget(menuBar_);
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

  // Window buttons. The dots from the mock-up, now actually wired up.
  const struct { const char* colour; int action; } buttons[] = {
    { "#2a3038", 0 },   // minimise
    { "#2a3038", 1 },   // maximise / restore
    { "#3a2a2a", 2 }    // close
  };
  for (const auto& b : buttons) {
    auto* d = new QLabel;
    d->setFixedSize(11, 11);
    d->setCursor(Qt::PointingHandCursor);
    d->setStyleSheet(QString("background:%1; border:none; border-radius:5px;").arg(b.colour));
    d->setProperty("windowAction", b.action);
    d->installEventFilter(this);
    row->addWidget(d);
  }

  // Dragging the bar moves the window, since there is no native caption to grab.
  w->installEventFilter(this);
  w->setProperty("isTitleBar", true);
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
  row->setSpacing(4);

  // The mock-up showed six "Alt+n Label" pairs. The shortcut hints were invented -- no
  // Alt+n binding ever existed -- so they are gone, and the labels are now buttons that
  // actually go somewhere. "Effekte" is dropped: this front-end has no effects panel to
  // send anyone to, and a button to nowhere is worse than no button.
  const struct { const char* label; int action; } kItems[] = {
    // Same words as the inspector tabs they open -- a button called "Modell" that
    // lands on a tab called "Anpassen" reads like a misclick.
    { "Anpassen",    ToolModel      },
    { "Charakter",   ToolCharacter  },
    { "Licht",       ToolLight      },
    { "Kamera",      ToolCamera     },
    { "Hintergrund", ToolBackground }
  };
  for (const auto& it : kItems) {
    auto* b = new QLabel(QString::fromUtf8(it.label));
    b->setFont(QFont(uiF(), 8));
    b->setCursor(Qt::PointingHandCursor);
    b->setStyleSheet("color:#9aa3b0; background:transparent; border:none;"
                     " padding:5px 10px; border-radius:5px;");
    b->setProperty("toolButton", it.action);
    b->installEventFilter(this);
    row->addWidget(b);
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
  searchWrap_ = searchWrap;
  searchWrap->setStyleSheet("background:transparent;");
  auto* sw = new QHBoxLayout(searchWrap);
  sw->setContentsMargins(12, 12, 12, 8);
  auto* search = new QLineEdit;
  search_ = search;
  // Typing filters as you go. A rebuild walks the whole listfile, so it waits for a
  // pause in the typing rather than running per keystroke; Enter skips the wait.
  auto* searchDelay = new QTimer(this);
  searchDelay->setSingleShot(true);
  searchDelay->setInterval(350);
  connect(searchDelay, &QTimer::timeout, this, &MainWindow::populateTree);
  connect(search, &QLineEdit::textChanged, this, [searchDelay](const QString&) {
    searchDelay->start();
  });
  connect(search, &QLineEdit::returnPressed, this, [this, searchDelay]() {
    searchDelay->stop();
    populateTree();
  });
  search->setPlaceholderText(QString::fromUtf8("Name oder FileDataID …"));
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
  for (int i = 0; i < 4; ++i) {
    QLabel* c = chip(QString::fromLatin1(catNames[i]), i == 0, uiF(), 8);
    c->setCursor(Qt::PointingHandCursor);
    c->installEventFilter(this);
    c->setProperty("categoryIndex", i);
    catChips_.push_back(c);
    cr->addWidget(c);
  }
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

  // Items get their own browser rather than a page of the file tree. The tree lists model
  // FILES, and most armour has none of its own -- it is a texture layer on a character --
  // so the only way to find a chest piece is through the item database.
  browserStack_ = new QStackedWidget;
  browserStack_->setStyleSheet("background:transparent;");
  browserStack_->addWidget(listHost);
  itemBrowser_ = new ItemBrowser;
  browserStack_->addWidget(itemBrowser_);
  col->addWidget(browserStack_, 1);

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

  const QString needle = search_ ? search_->text() : QString();
  const int n = (treeModel_->category() == FileTreeModel::Characters)
                  ? treeModel_->buildRaceBrowser(needle)
                  : treeModel_->rebuild("m2", needle);

  if (resultLabel_)
    resultLabel_->setText(QString::fromUtf8("ERGEBNISSE · %1").arg(n));

  // The race browser is small enough to show open; the full tree is not.
  if (tree_) {
    if (treeModel_->category() == FileTreeModel::Characters)
      tree_->expandToDepth(0);
    else
      tree_->collapseAll();
  }
}

void MainWindow::setCategory(int index)
{
  if (!treeModel_ || index < 0 || index >= (int)catChips_.size())
    return;

  static const FileTreeModel::Category kMap[] = {
    FileTreeModel::All, FileTreeModel::Characters,
    FileTreeModel::Creatures, FileTreeModel::Items
  };
  treeModel_->setCategory(kMap[index]);

  // "Items" swaps the whole column over to the database browser, which brings its own
  // search box -- the tree's would filter nothing there.
  const bool itemMode = (index == 3);
  if (browserStack_)
    browserStack_->setCurrentIndex(itemMode ? 1 : 0);
  if (searchWrap_)
    searchWrap_->setVisible(!itemMode);

  for (int i = 0; i < (int)catChips_.size(); ++i) {
    const bool active = (i == index);
    catChips_[i]->setStyleSheet(active
      ? QString("color:%1; background:%2; border:1px solid %3; border-radius:9px; padding:3px 10px;")
          .arg(tok::kAccent).arg(tok::kAccentBg).arg(tok::kAccentBr)
      : QString("color:%1; background:#12161b; border:1px solid %2; border-radius:9px; padding:3px 10px;")
          .arg(tok::kMuted).arg(tok::kBorder));
  }

  if (!itemMode)      // the item browser runs its own query
    populateTree();
}

void MainWindow::updateStats()
{
  if (!fpsLabel_ || !canvas_)
    return;
  const float f = canvas_->fps();
  fpsLabel_->setText(f > 0.0f ? QString("%1 FPS").arg(qRound(f))
                              : QString::fromUtf8("– FPS"));
}

void MainWindow::setExportFormats(const QStringList& labels)
{
  if (formatsLabel_)
    formatsLabel_->setText(labels.isEmpty() ? QString::fromUtf8("kein Exporter")
                                            : labels.join(QString::fromUtf8(" · ")));
}

void MainWindow::setGridIndicator(bool on)
{
  if (railButtons_.size() <= RailGrid)
    return;
  railButtons_[RailGrid]->setStyleSheet(
    QString("color:%1; background:%2; border:none; border-radius:6px;")
      .arg(on ? tok::kAccent : tok::kMuted).arg(on ? "#22282f" : "transparent"));
}

void MainWindow::setActiveCameraPreset(int index)
{
  for (int i = 0; i < (int)camPresets_.size(); ++i) {
    const bool active = (i == index);
    camPresets_[i]->setStyleSheet(active
      ? QString("background: rgba(40,32,14,235); border:1px solid #4a3f28; border-radius:6px;"
                "color:%1; padding:6px 11px;").arg(tok::kAccent)
      : QString("background: rgba(14,17,20,220); border:1px solid %1; border-radius:6px;"
                "color:#98a1ae; padding:6px 11px;").arg(tok::kBorder));
  }
}

bool MainWindow::eventFilter(QObject* obj, QEvent* e)
{
  if (e->type() == QEvent::MouseButtonRelease) {
    const QVariant idx = obj->property("categoryIndex");
    if (idx.isValid()) {
      setCategory(idx.toInt());
      return true;
    }
    if (obj->property("exportButton").isValid()) {
      emit exportRequested();
      return true;
    }
    if (obj->property("screenshotButton").isValid()) {
      emit screenshotRequested();
      return true;
    }
    const QVariant tool = obj->property("toolButton");
    if (tool.isValid()) {
      switch (tool.toInt()) {
        case ToolModel:      setInspectorTab(TabCharacter); break;
        case ToolCharacter:  setInspectorTab(TabCharacterIo); break;
        case ToolLight:      setInspectorTab(TabLight); break;
        case ToolCamera:     emit cameraMenuRequested(); break;
        case ToolBackground: emit backgroundRequested(); break;
      }
      return true;
    }
    const QVariant rail = obj->property("railButton");
    if (rail.isValid()) {
      switch (rail.toInt()) {
        case RailFit:   emit fitCameraRequested(); break;
        case RailGrid:  emit gridToggleRequested(); break;
        case RailLight: setInspectorTab(TabLight); break;
      }
      return true;
    }
    const QVariant preset = obj->property("cameraPreset");
    if (preset.isValid()) {
      setActiveCameraPreset(preset.toInt());
      emit cameraPresetRequested(preset.toInt());
      return true;
    }
    const QVariant tab = obj->property("inspectorTab");
    if (tab.isValid()) {
      setInspectorTab(tab.toInt());
      return true;
    }
    const QVariant act = obj->property("windowAction");
    if (act.isValid()) {
      switch (act.toInt()) {
        case 0: showMinimized(); break;
        case 1: isMaximized() ? showNormal() : showMaximized(); break;
        case 2: close(); break;
      }
      return true;
    }
  }

  // Frameless drag: remember the grab offset on press, move the window on drag.
  if (obj->property("isTitleBar").isValid()) {
    if (e->type() == QEvent::MouseButtonPress) {
      dragOffset_ = static_cast<QMouseEvent*>(e)->globalPos() - frameGeometry().topLeft();
      dragging_ = !isMaximized();
      return false;
    }
    if (e->type() == QEvent::MouseMove && dragging_) {
      move(static_cast<QMouseEvent*>(e)->globalPos() - dragOffset_);
      return false;
    }
    if (e->type() == QEvent::MouseButtonRelease) {
      dragging_ = false;
      return false;
    }
    if (e->type() == QEvent::MouseButtonDblClick) {
      isMaximized() ? showNormal() : showMaximized();
      return true;
    }
  }

  return QWidget::eventFilter(obj, e);
}

void MainWindow::onTreeActivated(const QModelIndex& index)
{
  if (GameFile* f = treeModel_->fileAt(index)) {
    emit fileActivated(f);
    return;
  }
  // Race-browser leaves carry a FileDataID instead of a GameFile.
  if (const int id = treeModel_->fileIdAt(index))
    emit fileIdActivated(id);
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
  // Three tools, not five decorative glyphs. The mock-up's pan and zoom icons are gone:
  // panning is the right mouse button and zooming is the wheel, so a button that only
  // says "you can drag" earns nothing.
  const struct { const char* glyph; int action; const char* tip; } kTools[] = {
    { "◎", RailFit,   "Auf das Modell einpassen" },
    { "▦", RailGrid,  "Gitter ein/aus" },
    { "☀", RailLight, "Licht" }
  };
  for (const auto& t : kTools) {
    QLabel* b = icon(QString::fromUtf8(t.glyph), 30, tok::kMuted, "transparent");
    b->setCursor(Qt::PointingHandCursor);
    b->setToolTip(QString::fromUtf8(t.tip));
    b->setProperty("railButton", t.action);
    b->installEventFilter(this);
    railButtons_.push_back(b);
    rl->addWidget(b);
  }
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
  shot->setCursor(Qt::PointingHandCursor);
  shot->setProperty("screenshotButton", true);
  shot->installEventFilter(this);
  ar->addWidget(shot);
  auto* exp = new QLabel("Exportieren");
  exp->setFont(QFont(uiF(), 9, QFont::DemiBold));
  exp->setAlignment(Qt::AlignCenter);
  exp->setCursor(Qt::PointingHandCursor);
  exp->setStyleSheet(QString("background:%1; border:1px solid #d9b678; border-radius:7px;"
                             "color:%2; padding:8px 14px;").arg(tok::kAccent).arg(tok::kOnAccent));
  exp->setProperty("exportButton", true);
  exp->installEventFilter(this);
  ar->addWidget(exp);
  g->addWidget(actions, 0, 2, Qt::AlignTop | Qt::AlignRight);
  asOverlay(actions);

  auto* stats = new QFrame(host);
  stats->setStyleSheet(QString("QFrame { background: rgba(14,17,20,220); border:1px solid %1;"
                               " border-radius:8px; }").arg(tok::kBorder));
  auto* sr = new QHBoxLayout(stats);
  sr->setContentsMargins(12, 6, 12, 6);
  sr->setSpacing(16);
  // "60 FPS" was a hardcoded string that happened to look plausible. It is measured now;
  // updateStats() is driven from the same timer that drives the timeline.
  fpsLabel_ = mk(QString::fromUtf8("– FPS"), monoF(), 8, "#7d8693");
  sr->addWidget(fpsLabel_);
  for (const char* s : {"M2", "GL 4.6"})
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
    c->setCursor(Qt::PointingHandCursor);
    c->setProperty("cameraPreset", i);
    c->installEventFilter(this);
    camPresets_.push_back(c);
    cr->addWidget(c);
  }
  // The mock-up highlighted "3/4" for looks. The camera actually starts where
  // OrbitCamera::reset() puts it, which is the front view -- so highlight that.
  setActiveCameraPreset(0);
  g->addWidget(cams, 2, 2, Qt::AlignBottom | Qt::AlignRight);
  asOverlay(cams);

  g->setColumnStretch(1, 1);
  g->setRowStretch(1, 1);
  return host;
}

QWidget* MainWindow::buildTimeline()
{
  timeline_ = new TimelinePanel;
  return timeline_;
}

QWidget* MainWindow::buildInspector()
{
  auto* w = new QWidget;
  styled(w);
  w->setFixedWidth(324);
  w->setStyleSheet(QString(
    "QWidget { background:%1; }"
    "QLabel { background:transparent; border:none; }")
    .arg(tok::kPanel));
  auto* col = new QVBoxLayout(w);
  col->setContentsMargins(0, 0, 0, 0);
  col->setSpacing(0);

  // Tab strip. The labels drive a QStackedWidget below.
  auto* tabs = new QWidget;
  tabs->setStyleSheet("background:transparent;");
  auto* tr = new QHBoxLayout(tabs);
  tr->setContentsMargins(0, 0, 0, 0);
  tr->setSpacing(0);
  const char* names[] = {"Anpassen", "Charakter", "Licht", "Export"};
  for (int i = 0; i < (int)(sizeof(names) / sizeof(names[0])); ++i) {
    auto* t = new QLabel(QString::fromLatin1(names[i]));
    t->setFixedHeight(38);
    t->setAlignment(Qt::AlignCenter);
    t->setCursor(Qt::PointingHandCursor);
    t->setProperty("inspectorTab", i);
    t->installEventFilter(this);
    inspectorTabs_.push_back(t);
    tr->addWidget(t, 1);
  }
  col->addWidget(tabs);

  inspectorStack_ = new QStackedWidget;
  inspectorStack_->setStyleSheet("background:transparent;");

  // Page 0 -- character
  auto* charBody = new QWidget;
  charBody->setStyleSheet("background:transparent;");
  auto* cb = new QVBoxLayout(charBody);
  cb->setContentsMargins(16, 16, 16, 24);
  charPanel_ = new CharacterPanel;
  cb->addWidget(charPanel_);
  cb->addStretch(1);
  inspectorStack_->addWidget(wrapScroll(charBody));

  // Page 1 -- the character himself: where he comes from and where he goes.
  // Filled by main(), which is the first place that has both the loaded exporters and
  // the menu controller the buttons delegate to.
  auto* ioBody = new QWidget;
  ioBody->setStyleSheet("background:transparent;");
  auto* ib = new QVBoxLayout(ioBody);
  ib->setContentsMargins(16, 16, 16, 24);
  characterIoHost_ = new QWidget;
  auto* ih = new QVBoxLayout(characterIoHost_);
  ih->setContentsMargins(0, 0, 0, 0);
  characterIoHost_->setStyleSheet("background:transparent;");
  ib->addWidget(characterIoHost_);
  ib->addStretch(1);
  inspectorStack_->addWidget(wrapScroll(ioBody));

  // Page 2 -- lighting. The four scene lights were configurable in principle since
  // Phase 0 (SceneLighting is widget-free) but had no UI at all.
  auto* lightBody = new QWidget;
  lightBody->setStyleSheet("background:transparent;");
  auto* lb = new QVBoxLayout(lightBody);
  lb->setContentsMargins(16, 16, 16, 24);
  lightPanel_ = new LightPanel(canvas_);
  lb->addWidget(lightPanel_);
  lb->addStretch(1);
  inspectorStack_->addWidget(wrapScroll(lightBody));

  // Page 3 -- export (filled in by main once the exporters are loaded)
  auto* expBody = new QWidget;
  expBody->setStyleSheet("background:transparent;");
  auto* eb = new QVBoxLayout(expBody);
  eb->setContentsMargins(16, 16, 16, 24);
  exportHost_ = new QWidget;
  auto* eh = new QVBoxLayout(exportHost_);
  eh->setContentsMargins(0, 0, 0, 0);
  exportHost_->setStyleSheet("background:transparent;");
  eb->addWidget(exportHost_);
  eb->addStretch(1);
  inspectorStack_->addWidget(wrapScroll(expBody));

  col->addWidget(inspectorStack_, 1);
  setInspectorTab(0);
  return w;
}

void MainWindow::setInspectorTab(int index)
{
  if (!inspectorStack_ || index < 0 || index >= (int)inspectorTabs_.size())
    return;
  inspectorStack_->setCurrentIndex(index);
  for (int i = 0; i < (int)inspectorTabs_.size(); ++i) {
    const bool active = (i == index);
    inspectorTabs_[i]->setFont(QFont(uiF(), 9, active ? QFont::DemiBold : QFont::Normal));
    inspectorTabs_[i]->setStyleSheet(active
      ? QString("color:%1; background:transparent; border:none; border-bottom:2px solid %2;")
          .arg(tok::kText).arg(tok::kAccent)
      : QString("color:#7d8693; background:transparent; border:none; border-bottom:1px solid %1;")
          .arg(tok::kBorder2));
  }
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
  // Was the literal "FBX · OBJ · glTF". There is no glTF exporter -- only the fbx and obj
  // plugins exist -- so the text promised a format the build cannot produce. main() fills
  // this in from the exporters that actually loaded.
  formatsLabel_ = mk(QString(), monoF(), 7, tok::kDim);
  r->addWidget(formatsLabel_);
  // Without a native frame there is no resize edge, so give the status bar a grip.
  auto* grip = new QSizeGrip(w);
  grip->setFixedSize(14, 14);
  r->addWidget(grip, 0, Qt::AlignBottom | Qt::AlignRight);
  return w;
}
