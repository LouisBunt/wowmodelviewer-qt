#include "MainWindow.h"

#include <QFontDatabase>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCloseEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QSizeGrip>
#include <QLineEdit>
#include <QMenuBar>
#include <QPushButton>
#include <QSplitter>
#include <QToolButton>
#include <QTreeView>
#include <QResizeEvent>
#include <QScrollArea>
#include <QShortcut>
#include <QSettings>
#include <QStackedWidget>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

#include "CharacterPanel.h"
#include "FileTreeModel.h"
#include "GLHost.h"
#include "InspectorTabs.h"
#include "ItemBrowser.h"
#include "NpcBrowser.h"
#include "LightPanel.h"
#include "Theme.h"
#include "UiKit.h"
#include "TimelinePanel.h"



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

// The title bar's surface.
//
// Painted rather than styled: a stylesheet can do a gradient but not a texture, and the
// other way round -- a PNG in a .qrc -- would add a binary asset, a resource file and a
// build step for something that is twenty lines of arithmetic. The grain is fixed-seed
// value noise, so the pattern is identical on every run and screenshots stay comparable.
//
// To the rest of the window this has to remain an ordinary QWidget: MainWindow installs an
// event filter on it and reads the "isTitleBar" property to drag and to maximise. Handling
// mouse events here would take them away from that filter.
class TitleBarSurface : public QWidget
{
public:
  explicit TitleBarSurface(QWidget* parent = nullptr) : QWidget(parent) {}

protected:
  void paintEvent(QPaintEvent*) override
  {
    QPainter p(this);
    QLinearGradient g(0, 0, 0, height());
    g.setColorAt(0.0, QColor(tok::bgChrome));
    g.setColorAt(1.0, QColor(tok::bgVoid));
    p.fillRect(rect(), g);
    p.fillRect(rect(), QBrush(grain()));
    // The underline the stylesheet used to carry. Drawn last so the grain cannot mottle it.
    p.fillRect(0, height() - 1, width(), 1, QColor(tok::lineHair));
  }

private:
  // Cached as a QImage rather than a QPixmap on purpose: a function-local static QPixmap is
  // destroyed after QApplication has gone, and tearing down a pixmap without a platform
  // plugin is a well-known way to crash on exit. QBrush takes a QImage directly, so there
  // is no reason to hold a pixmap at all.
  static const QImage& grain()
  {
    static const QImage tile = [] {
      const int n = 64;
      QImage img(n, n, QImage::Format_ARGB32_Premultiplied);
      img.fill(Qt::transparent);
      quint32 s = 0x9e3779b9u;                  // fixed seed -- same grain every run
      for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
          s ^= s << 13; s ^= s >> 17; s ^= s << 5;
          const int v = int(s % 16u);
          if (v < 10)
            continue;                           // most pixels stay clean
          // Both directions, and barely: across 38 px the bar should read as having a
          // surface, not as being dirty. Lighter specks are rarer than darker ones.
          const bool light = (s & 0x30000u) == 0;
          img.setPixelColor(x, y, light ? QColor(255, 255, 255, v - 8)
                                        : QColor(0, 0, 0, v + 6));
        }
      }
      return img;
    }();
    return tile;
  }
};

// Every inspector page scrolls; they are all longer than the column.
static QWidget* wrapScroll(QWidget* body)
{
  auto* s = new QScrollArea;
  s->setWidget(body);
  s->setWidgetResizable(true);
  s->setFrameShape(QFrame::NoFrame);
  s->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  return s;
}

// A HUD element floating over the GL canvas. GLHost is a native child window, so
// ordinary sibling widgets are painted UNDER it by the compositor. Making the HUD
// native too gives it its own HWND, which sits above the GL surface in z-order.
static void asOverlay(QWidget* w)
{
  w->setAttribute(Qt::WA_NativeWindow, true);
  // A native child has its own backing store and inherits nothing from its parent's paint, so
  // it has to fill itself. Without this the pill shows uninitialised memory -- the previous
  // frame's pixels smeared over each other.
  w->setAttribute(Qt::WA_StyledBackground, true);
  w->setProperty("role", "overlay");
  w->raise();
}

// ElidedLabel used to be declared here and never instantiated -- a working answer to the
// project's most visible defect, sitting unused. It is a real class now, in UiKit.h.

static QLabel* mk(const QString& text, const QString& family, int pt, const char* colour,
                  bool bold = false, qreal spacing = 0)
{
  auto* l = new QLabel(text);
  QFont f(family, pt, bold ? QFont::DemiBold : QFont::Normal);
  if (spacing > 0)
    f.setLetterSpacing(QFont::AbsoluteSpacing, spacing);
  l->setFont(f);
  return l;
}

static QLabel* chip(const QString& text, bool active, const QString& family, int pt)
{
  auto* l = new QLabel(text);
  l->setFont(QFont(family, pt));
  l->setAlignment(Qt::AlignCenter);
  return l;
}

static QLabel* icon(const QString& glyph, int size, const char* colour, const char* background)
{
  auto* l = new QLabel(glyph);
  l->setFixedSize(size, size);
  l->setAlignment(Qt::AlignCenter);
  l->setFont(QFont(iconF(), size / 3 + 3));
  return l;
}

// ---------------------------------------------------------------------------

MainWindow::MainWindow()
{
  styled(this);
  // No window-wide stylesheet. It set the ground for every widget and then spent fifteen
  // lines undoing its own cascade, because a stylesheet on a window also applies to every
  // QDialog parented to it -- which is how an armory import ended up as black text on a
  // near-black field. Theme::sheet() styles dialogs directly, so there is nothing to undo.
  setProperty("role", "panel");
  // The version belongs in the title: a screenshot in a bug report then carries it
  // without the reporter having to look it up.
  setWindowTitle(QString("%1 %2").arg(WMV_APP_NAME).arg(WMV_QT_VERSION));
  resize(ui::px(1480), ui::px(900));
  // A floor, so the three columns and the timeline stay legible. Below this the layout is
  // not cramped, it is broken -- the inspector's controls stop fitting at all.
  setMinimumSize(ui::px(1120), ui::px(700));

  // The design has its own title bar, so the native frame goes away. On Windows this
  // costs Aero Snap and edge resizing, which the frame provided for free -- dragging
  // is reimplemented on the title bar below and a size grip sits in the corner.
  setWindowFlags(Qt::Window | Qt::FramelessWindowHint);

  auto* col = new QVBoxLayout(this);
  col->setContentsMargins(0, 0, 0, 0);
  col->setSpacing(0);
  col->addWidget(buildTitleBar());
  col->addWidget(buildToolBar());

  // Three columns in a splitter, not three fixed widths.
  //
  // The browser was pinned at 288 px and the inspector at 324, so 612 px of every window --
  // more than a third of the default width -- was spent on side columns the user could not
  // change. When a name did not fit there was no remedy at all: no wider column, no scroll,
  // no tooltip. Now the columns have a range, the viewport keeps a floor, and where the user
  // puts the handles is remembered between runs.
  columns_ = new QSplitter(Qt::Horizontal);
  columns_->setChildrenCollapsible(false);
  columns_->setHandleWidth(met::sp(met::Snug));

  QWidget* browser = buildBrowser();
  browser->setMinimumWidth(met::browserMin());
  browser->setMaximumWidth(met::browserMax());
  columns_->addWidget(browser);

  // The viewport and the timeline share a vertical splitter, so the timeline can be pulled
  // down to nothing when the model is what matters.
  centre_ = new QSplitter(Qt::Vertical);
  centre_->setChildrenCollapsible(false);
  centre_->setHandleWidth(met::sp(met::Snug));
  QWidget* viewport = buildViewport();
  viewport->setMinimumWidth(met::canvasMin());
  centre_->addWidget(viewport);
  QWidget* timeline = buildTimeline();
  centre_->addWidget(timeline);
  centre_->setStretchFactor(0, 1);
  centre_->setStretchFactor(1, 0);
  columns_->addWidget(centre_);

  QWidget* inspector = buildInspector();
  inspector->setMinimumWidth(met::inspectorMin());
  inspector->setMaximumWidth(met::inspectorMax());
  columns_->addWidget(inspector);

  columns_->setStretchFactor(0, 0);
  columns_->setStretchFactor(1, 1);
  columns_->setStretchFactor(2, 0);
  columns_->setSizes({met::browserDef(), met::canvasMin(), met::inspectorDef()});

  QWidget* mid = columns_;

  col->addWidget(mid, 1);
  col->addWidget(buildStatusBar());

  restoreLayout();
  installShortcuts();
}

// Keyboard access to the four inspector tabs and the two columns.
//
// Nothing in the chrome could be reached from the keyboard before: every control was a QLabel
// with an event filter, so Tab skipped all of them and there were no accelerators at all. The
// tool bar used to PRINT "Alt+1 Modell" next to buttons that had no such binding.
void MainWindow::installShortcuts()
{
  for (int i = 0; i < 4; ++i) {
    auto* sc = new QShortcut(QKeySequence(Qt::ALT + (Qt::Key_1 + i)), this);
    connect(sc, &QShortcut::activated, this, [this, i]() { setInspectorTab(i); });
  }
  // Fold a column away when the model is what matters. A splitter can do it, but only by
  // dragging; these give it a key.
  auto* toggleBrowser = new QShortcut(QKeySequence(Qt::Key_T), this);
  connect(toggleBrowser, &QShortcut::activated, this, [this]() {
    if (!columns_) return;
    QList<int> s = columns_->sizes();
    const bool hidden = s[0] == 0;
    s[1] += hidden ? -met::browserDef() : s[0];
    s[0] = hidden ? met::browserDef() : 0;
    columns_->setSizes(s);
  });
  auto* toggleInspector = new QShortcut(QKeySequence(Qt::Key_N), this);
  connect(toggleInspector, &QShortcut::activated, this, [this]() {
    if (!columns_) return;
    QList<int> s = columns_->sizes();
    const bool hidden = s[2] == 0;
    s[1] += hidden ? -met::inspectorDef() : s[2];
    s[2] = hidden ? met::inspectorDef() : 0;
    columns_->setSizes(s);
  });
}

// Where the window and the handles were last left.
//
// Nothing was remembered before: every start reopened at 1480x900 with columns the user could
// not have moved anyway. QSettings, the same ini the WoW folder lives in.
void MainWindow::saveLayout() const
{
  QSettings s(QStringLiteral("userSettings/qt-frontend.ini"), QSettings::IniFormat);
  s.setValue("ui/geometry", saveGeometry());
  if (columns_) s.setValue("ui/columns", columns_->saveState());
  if (centre_)  s.setValue("ui/centre", centre_->saveState());
}

void MainWindow::restoreLayout()
{
  QSettings s(QStringLiteral("userSettings/qt-frontend.ini"), QSettings::IniFormat);
  const QByteArray g = s.value("ui/geometry").toByteArray();
  if (!g.isEmpty())
    restoreGeometry(g);
  if (columns_) columns_->restoreState(s.value("ui/columns").toByteArray());
  if (centre_)  centre_->restoreState(s.value("ui/centre").toByteArray());
}

void MainWindow::resetLayout()
{
  resize(ui::px(1480), ui::px(900));
  if (columns_)
    columns_->setSizes({met::browserDef(), met::canvasMin(), met::inspectorDef()});
  if (centre_)
    centre_->setSizes({height(), ui::px(96)});
}

void MainWindow::closeEvent(QCloseEvent* e)
{
  saveLayout();
  QWidget::closeEvent(e);
}

void MainWindow::setDisplayFont(const QString& family, int pointSize)
{
  if (!brandLabel_ || family.isEmpty())
    return;
  QFont f = brandLabel_->font();
  f.setFamily(family);
  if (pointSize > 0)
    f.setPointSize(pointSize);
  // mk() sets the brand line DemiBold, which suits Cinzel. A display face out of the game
  // archives ships one weight, and Qt would fake the bold by smearing the outlines -- so
  // ask for the weight the file actually has.
  f.setBold(false);
  f.setWeight(QFont::Normal);
  // The 1.3 px of tracking is tuned for Cinzel's caps; a face with its own wide
  // sidebearings only reads as gappy at that value.
  f.setLetterSpacing(QFont::AbsoluteSpacing, 0.6);
  brandLabel_->setFont(f);
}

void MainWindow::setBuildLabel(const QString& text)
{
  if (buildLabel_)
    buildLabel_->setText(text);
}

void MainWindow::setPathLabel(const QString& text)
{
  // One place, not two. The same string used to be written into the tool bar AND the status
  // bar, in the same mono face, so a screenshot showed the path twice while neither strip had
  // the room for it. It goes into the title bar's context line, middle-elided, with the full
  // string in the tooltip; the status bar keeps its own short summary.
  if (contextLabel_)
    contextLabel_->setFullText(text);
}

QWidget* MainWindow::buildTitleBar()
{
  // No stylesheet and no WA_StyledBackground here: TitleBarSurface::paintEvent draws the
  // ground, the grain and the underline itself, and a stylesheet background would sit on
  // top of all three.
  auto* w = new TitleBarSurface;
  w->setFixedHeight(met::hBar());
  auto* row = new QHBoxLayout(w);
  row->setContentsMargins(met::sp(met::Edge), 0, 0, 0);
  row->setSpacing(met::sp(met::Pad));

  // The wordmark. Three labels, not one string, because they have to give way in order.
  //
  // What was here: the whole product name, uppercased, in the game's ornamental serif at the
  // same optical size as the menu -- 21 wide glyphs whose sizeHint became the layout's hard
  // minimum, so the title could only crowd the menu bar or clip. It could not shorten, because
  // a QLabel has no elide mode.
  //
  // Now only "MIDNIGHT" is ornamental. Eight characters, never elided, always the same width.
  // "ModelViewer" and the version are ordinary text that drop out as the window narrows: the
  // name of the product survives at any size, which is the opposite of what happened before.
  auto* brand = new QHBoxLayout;
  brand->setSpacing(met::sp(met::Gap));
  {
    auto* mark = new QLabel;
    mark->setPixmap(Theme::icon("layers", QColor(tok::accentText))
                      .pixmap(ui::px(16), ui::px(16)));
    mark->setFixedSize(ui::px(18), ui::px(18));
    mark->setAlignment(Qt::AlignCenter);
    brand->addWidget(mark);
  }
  brandLabel_ = new QLabel(QStringLiteral("MIDNIGHT"));
  brandLabel_->setFont(typo::font(typo::Wordmark));
  brand->addWidget(brandLabel_);

  brandSub_ = new QLabel(QStringLiteral("ModelViewer"));
  brandSub_->setFont(typo::font(typo::Small));
  brandSub_->setProperty("role", "caption");
  brand->addWidget(brandSub_);

  brandVersion_ = new QLabel(WMV_QT_VERSION);
  brandVersion_->setFont(typo::font(typo::Mono));
  brandVersion_->setProperty("role", "mono");
  brand->addWidget(brandVersion_);
  row->addLayout(brand);

  // A real QMenuBar rather than the mock-up's five labels: it brings hover states,
  // Alt mnemonics, keyboard navigation and window-wide shortcuts with it. The menus
  // themselves are filled in by MenuController, which needs objects main() only has
  // once the game data and the plugins are loaded.
  //
  // setNativeMenuBar(false) keeps it inside our own title bar on platforms that would
  // otherwise lift it into a system menu bar.
  // A real QMenuBar rather than the mock-up's five labels: it brings hover states, Alt
  // mnemonics, keyboard navigation and window-wide shortcuts with it. Flat now -- the tiles
  // with their own borders competed with the wordmark for the same corner of the window.
  menuBar_ = new QMenuBar;
  menuBar_->setNativeMenuBar(false);
  menuBar_->setFont(typo::font(typo::Body));
  row->addWidget(menuBar_);
  row->addSpacing(met::sp(met::Edge));

  // The context line: what is loaded, once, in the middle of the bar.
  //
  // The same path used to be printed twice -- in the tool bar and in the status bar -- in two
  // 30px strips that had no room for it, and the long German sentences main() writes there
  // ("Kein Modell geladen — links im Baum eines wählen") pushed their neighbours aside. One
  // line, middle-elided so the file name survives, with the full path in the tooltip.
  contextLabel_ = new ElidedLabel;
  contextLabel_->setElideMode(Qt::ElideMiddle);
  contextLabel_->setFont(typo::font(typo::Small));
  contextLabel_->setProperty("role", "hint");
  contextLabel_->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
  row->addWidget(contextLabel_, 1);
  row->addSpacing(met::sp(met::Edge));

  // The CASC indicator: a dot and a build number, not a bordered pill. It is a passive status,
  // and as a pill it was the highest-contrast object in the top third of the window.
  {
    auto* status = new QWidget;
    status->setAttribute(Qt::WA_StyledBackground, true);
    auto* sr = new QHBoxLayout(status);
    sr->setContentsMargins(0, 0, 0, 0);
    sr->setSpacing(met::sp(met::Snug) + 2);
    auto* dot = new QLabel;
    dot->setFixedSize(ui::px(6), ui::px(6));
    sr->addWidget(dot);
    buildLabel_ = new QLabel("CASC");
    buildLabel_->setFont(typo::font(typo::Mono));
    buildLabel_->setProperty("role", "mono");
    sr->addWidget(buildLabel_);
    status->setToolTip(QString::fromUtf8("Spielarchiv eingebunden"));
    row->addWidget(status);
  }
  row->addSpacing(met::sp(met::Pad));

  // Window buttons. Real QToolButtons now: they were QLabels with an event filter, so they had
  // no pressed state, could not be reached with the keyboard and were invisible to any
  // accessibility tool. Full bar height, because the corner of a window is a target people
  // throw the pointer at.
  {
    auto* group = new QWidget;
    group->setAttribute(Qt::WA_StyledBackground, true);
    auto* gr = new QHBoxLayout(group);
    gr->setContentsMargins(0, 0, 0, 0);
    gr->setSpacing(0);
    const struct { const char* icon; const char* tip; int action; } buttons[] = {
      { "minus",  "Minimieren", 0 },
      { "square", "Maximieren", 1 },
      { "x",      "Schließen",  2 }
    };
    for (const auto& b : buttons) {
      auto* t = new QToolButton;
      t->setIcon(Theme::icon(b.icon, QColor(tok::fgMuted)));
      t->setIconSize(QSize(ui::px(12), ui::px(12)));
      t->setFixedSize(ui::px(44), ui::px(38));
      t->setToolTip(QString::fromUtf8(b.tip));
      t->setProperty("role", b.action == 2 ? "close" : "window");
      t->setFocusPolicy(Qt::NoFocus);   // the corner buttons are not part of the tab order
      const int action = b.action;
      connect(t, &QToolButton::clicked, this, [this, action]() {
        if (action == 0) showMinimized();
        else if (action == 1) isMaximized() ? showNormal() : showMaximized();
        else close();
      });
      gr->addWidget(t);
    }
    row->addWidget(group);
  }

  // Dragging the bar moves the window, since there is no native caption to grab.
  w->installEventFilter(this);
  w->setProperty("isTitleBar", true);
  return w;
}

QWidget* MainWindow::buildToolBar()
{
  // One strip, three groups, each a real control.
  //
  // What was here mixed three unrelated kinds of thing in thirty pixels: three buttons that
  // only jumped to an inspector tab visible on the right anyway, two that opened viewport
  // menus, a view-mode pair, and the model path. All of them were QLabels with an event
  // filter -- no hover, no pressed state, no keyboard, no disabled look -- which is most of
  // why the window felt like a picture of an application rather than one.
  //
  // The tab shortcuts are gone (Alt+1..4 do it, and the tabs are two centimetres away). The
  // camera presets and the two viewport actions move up from the HUD, so the primary action
  // is always at the same pixel and two native overlays over the GL canvas disappear.
  auto* w = new QWidget;
  styled(w);
  w->setProperty("role", "chrome");
  w->setFixedHeight(met::hCta());
  auto* row = new QHBoxLayout(w);
  row->setContentsMargins(met::sp(met::Pad), 0, met::sp(met::Pad), 0);
  row->setSpacing(met::sp(met::Pad));

  // Group 1: what is shown -- the whole character, or one worn piece.
  viewBar_ = new SegmentedBar(SegmentedBar::Radio, w);
  viewBar_->addSegment(QString::fromUtf8("Charakter"), "user",
                       QString::fromUtf8("Die ganze Figur zeigen"));
  viewBar_->addSegment(QString::fromUtf8("Nur Teil"), "eye",
                       QString::fromUtf8("Nur ein getragenes Teil zeigen"));
  connect(viewBar_, &SegmentedBar::activated, this,
          [this](int i) { emit itemViewRequested(i == 1); });
  row->addWidget(viewBar_);

  // Group 2: where it is seen from.
  camBar_ = new SegmentedBar(SegmentedBar::Radio, w);
  const char* kCam[] = {"Vorn", "3/4", "Seite", "Oben"};
  for (const char* c : kCam)
    camBar_->addSegment(QString::fromUtf8(c));
  connect(camBar_, &SegmentedBar::activated, this,
          [this](int i) { emit cameraPresetRequested(i); });
  row->addWidget(camBar_);

  // Group 3: the viewport's own switches.
  {
    auto* fit = uikit::iconButton("maximize-2", QString::fromUtf8("Kamera einpassen (R)"));
    connect(fit, &QToolButton::clicked, this, &MainWindow::fitCameraRequested);
    row->addWidget(fit);

    gridButton_ = uikit::iconButton("grid-3x3", QString::fromUtf8("Gitter (Strg+G)"));
    gridButton_->setCheckable(true);
    connect(gridButton_, &QToolButton::clicked, this, &MainWindow::gridToggleRequested);
    row->addWidget(gridButton_);

    auto* bg = uikit::iconButton("palette", QString::fromUtf8("Hintergrund"));
    connect(bg, &QToolButton::clicked, this, &MainWindow::backgroundRequested);
    row->addWidget(bg);
  }

  row->addStretch(1);

  screenshotButton_ = uikit::iconButton("camera", QString::fromUtf8("Screenshot (F12)"));
  connect(screenshotButton_, &QToolButton::clicked, this, &MainWindow::screenshotRequested);
  row->addWidget(screenshotButton_);

  exportButton2_ = new QPushButton(QString::fromUtf8("Exportieren"));
  exportButton2_->setProperty("variant", "primary");
  exportButton2_->setCursor(Qt::PointingHandCursor);
  exportButton2_->setIcon(Theme::icon("package", QColor(tok::onAccent)));
  connect(exportButton2_, &QPushButton::clicked, this, &MainWindow::exportRequested);
  row->addWidget(exportButton2_);

  setItemFocusIndicator(-1);
  return w;
}

QWidget* MainWindow::buildBrowser()
{
  // One left edge for everything in this column.
  //
  // The search field, the category row, the section label and the tree used to start at four
  // different x positions within seventeen pixels -- not aligned enough to read as indentation,
  // not far enough apart to read as intent, which is exactly the kind of thing that registers
  // as "sloppy" without being nameable. One margin constant now, used by all four.
  auto* w = new QWidget;
  styled(w);
  w->setProperty("role", "panel");
  auto* col = new QVBoxLayout(w);
  col->setContentsMargins(0, 0, 0, 0);
  col->setSpacing(0);

  const int edge = met::sp(met::Pad);

  auto* searchWrap = new QWidget;
  searchWrap_ = searchWrap;
  searchWrap->setAttribute(Qt::WA_StyledBackground, true);
  searchWrap->setProperty("role", "panel");
  auto* sw = new QHBoxLayout(searchWrap);
  sw->setContentsMargins(edge, edge, edge, met::sp(met::Gap));
  auto* search = new SearchField(QString::fromUtf8("Name oder FileDataID …"));
  search_ = search;
  connect(search, &SearchField::searchChanged, this, [this](const QString&) { populateTree(); });
  connect(search, &QLineEdit::returnPressed, this, &MainWindow::populateTree);
  sw->addWidget(search);
  col->addWidget(searchWrap);

  // The categories. They were five text chips squeezed into 288 px, where "Charaktere" and
  // "Kreaturen" came out as "arakte" and "eature" -- the clearest single example of the
  // owner's complaint, and visible in every screenshot the project has. A segmented control
  // drops the LABEL when the column is narrow and keeps the icon and the tooltip, so the
  // control still says what it is instead of showing a word fragment.
  auto* cats = new QWidget;
  cats->setAttribute(Qt::WA_StyledBackground, true);
  cats->setProperty("role", "panel");
  auto* cr = new QHBoxLayout(cats);
  cr->setContentsMargins(edge, 0, edge, met::sp(met::Gap));
  cr->setSpacing(0);
  catBar_ = new SegmentedBar(SegmentedBar::Radio, cats);
  const struct { const char* label; const char* icon; } kCats[] = {
    { "Alle",       "layers"    },
    { "Charaktere", "user"      },
    { "Kreaturen",  "paw-print" },
    { "Items",      "swords"    },
    { "NPCs",       "users"     },
  };
  for (const auto& c : kCats)
    catBar_->addSegment(QString::fromUtf8(c.label), c.icon);
  connect(catBar_, &SegmentedBar::activated, this, [this](int i) { setCategory(i); });
  cr->addWidget(catBar_);
  col->addWidget(cats);

  auto* listHost = new QWidget;
  listHost->setAttribute(Qt::WA_StyledBackground, true);
  listHost->setProperty("role", "panel");
  auto* lr = new QVBoxLayout(listHost);
  lr->setContentsMargins(edge, 0, edge, met::sp(met::Gap));
  lr->setSpacing(met::sp(met::Snug));
  resultLabel_ = uikit::sectionLabel(QString::fromUtf8("ERGEBNISSE"));
  lr->addWidget(resultLabel_);

  // The real tree. No lazy expansion, no freeze/thaw: the view only asks the model
  // about rows it is about to paint.
  treeModel_ = new FileTreeModel(this);
  tree_ = new QTreeView;
  tree_->setModel(treeModel_);
  tree_->setHeaderHidden(true);
  tree_->setUniformRowHeights(true);      // lets the view skip per-row size queries
  tree_->setProperty("role", "flush");
  tree_->setFont(typo::font(typo::Body));
  // Middle elision, because the tail of a model path ("...male_hd.m2") is the half that
  // identifies it; end elision would cut off exactly the informative part. Per-pixel
  // scrolling because a 130 000-row list jumps a whole row at a time otherwise.
  tree_->setTextElideMode(Qt::ElideMiddle);
  tree_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  tree_->setAlternatingRowColors(false);
  connect(tree_, &QTreeView::activated, this, &MainWindow::onTreeActivated);
  lr->addWidget(tree_, 1);

  // Items get their own browser rather than a page of the file tree. The tree lists model
  // FILES, and most armour has none of its own -- it is a texture layer on a character --
  // so the only way to find a chest piece is through the item database.
  browserStack_ = new QStackedWidget;
  browserStack_->setAttribute(Qt::WA_StyledBackground, true);
  browserStack_->setProperty("role", "panel");
  browserStack_->addWidget(listHost);
  itemBrowser_ = new ItemBrowser;
  browserStack_->addWidget(itemBrowser_);
  // NPCs are the same situation as items, one layer up: the tree lists model files, but
  // one creature .m2 serves dozens of differently named and skinned NPCs. Only the
  // creature database can answer "show me Hogger".
  npcBrowser_ = new NpcBrowser;
  browserStack_->addWidget(npcBrowser_);
  col->addWidget(browserStack_, 1);
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
  if (!treeModel_ || index < 0 || index > 4)
    return;

  // "Items" and "NPCs" swap the whole column over to their database browsers, which bring
  // their own search boxes -- the tree's would filter nothing there. The tree model only
  // learns about categories it actually renders.
  const bool itemMode = (index == 3);
  const bool npcMode = (index == 4);
  if (!itemMode && !npcMode) {
    static const FileTreeModel::Category kMap[] = {
      FileTreeModel::All, FileTreeModel::Characters, FileTreeModel::Creatures
    };
    treeModel_->setCategory(kMap[index]);
  }
  if (browserStack_)
    browserStack_->setCurrentIndex(npcMode ? 2 : itemMode ? 1 : 0);
  if (searchWrap_)
    searchWrap_->setVisible(!itemMode && !npcMode);

  // One line where there were eight: the segmented control holds the checked state, and
  // the stylesheet decides what checked looks like.
  if (catBar_)
    catBar_->setCurrent(index);

  if (!itemMode && !npcMode)      // the database browsers run their own queries
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
  if (gridButton_)
    gridButton_->setChecked(on);
}

void MainWindow::setModelActionsEnabled(bool on)
{
  // setEnabled, not a rebuilt stylesheet: the sheet already says what a disabled button
  // looks like, and the button stops accepting clicks instead of only looking as if it had.
  if (exportButton2_)
    exportButton2_->setEnabled(on);
  if (exportButton2_)
    exportButton2_->setToolTip(on ? QString() : tr("Erst ein Modell laden"));
  if (screenshotButton_)
    screenshotButton_->setEnabled(on);
  if (viewBar_)
    viewBar_->setEnabled(on);
  // The hint owns the middle of an empty viewport; it would be in the way of a model.
  if (emptyHint_)
    emptyHint_->setVisible(!on);
}

void MainWindow::setItemFocusIndicator(int slot)
{
  if (viewBar_)
    viewBar_->setCurrent(slot >= 0 ? 1 : 0);
}

void MainWindow::setActiveCameraPreset(int index)
{
  if (camBar_)
    camBar_->setCurrent(index);
}

bool MainWindow::eventFilter(QObject* obj, QEvent* e)
{
  // All that is left of what used to be a hundred-line chain of property lookups: the
  // window drag. Everything else in the chrome is a real button with a real signal now,
  // which is what gives it hover, pressed, disabled and keyboard states.
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

  auto* g = new QGridLayout(host);
  g->setContentsMargins(14, 14, 14, 14);

  // The GL canvas fills the whole cell; the HUD sits on top of it in the same cells.
  canvas_ = new GLHost(host);
  g->addWidget(canvas_, 0, 0, 3, 3);

  // An empty viewport with nothing in it reads as "broken", not as "waiting". One quiet
  // line in the middle says which of the two it is. Hidden as soon as a model arrives
  // (setModelActionsEnabled), and an overlay like the rest of the HUD because GLHost is a
  // native child window that would otherwise paint straight over it.
  emptyHint_ = new QLabel(QString::fromUtf8(
                            "Kein Modell geladen\n\n"
                            "Links im Baum eines auswählen —\n"
                            "unter „Charaktere“ nach Rasse, "
                            "unter „Items“ nach Ausrüstung."), host);
  emptyHint_->setAlignment(Qt::AlignCenter);
  emptyHint_->setFont(typo::font(typo::Body));
  // Not transparent: asOverlay() gives this its own native window, which cannot inherit the
  // parent's paint and would show up as a black box. Matching the GL clear colour (GLHost's
  // bg_) makes it sit invisibly on the viewport instead.
  g->addWidget(emptyHint_, 0, 0, 3, 3, Qt::AlignCenter);
  asOverlay(emptyHint_);

  // Three floating clusters are gone from the viewport.
  //
  // The rail's three glyphs -- fit, grid, light -- were unlabelled symbols at three different
  // optical weights, in a card pushed hard into the corner of the render. Screenshot and
  // Exportieren floated top-right on a different margin. The camera presets sat bottom-right.
  // All of them are named buttons in the tool bar now, which puts the primary action at the
  // same pixel whatever the viewport is doing, and removes three native child windows whose
  // opaque backing showed as dark boxes the moment the user changed the background colour.
  setActiveCameraPreset(0);

  auto* stats = new QFrame(host);
  auto* sr = new QHBoxLayout(stats);
  sr->setContentsMargins(12, 6, 12, 6);
  sr->setSpacing(16);
  // "60 FPS" was a hardcoded string that happened to look plausible. It is measured now;
  // updateStats() is driven from the same timer that drives the timeline.
  fpsLabel_ = mk(QString::fromUtf8("– FPS"), typo::monoFamily(), 8, tok::fgMuted);
  sr->addWidget(fpsLabel_);
  for (const char* s : {"M2", "GL 4.6"})
    sr->addWidget(mk(QString::fromUtf8(s), typo::monoFamily(), 8, tok::fgMuted));
  g->addWidget(stats, 2, 0, Qt::AlignBottom | Qt::AlignLeft);
  asOverlay(stats);

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
  auto* col = new QVBoxLayout(w);
  col->setContentsMargins(0, 0, 0, 0);
  col->setSpacing(0);

  // Tab strip. The labels drive a QStackedWidget below.
  auto* tabs = new QWidget;
  auto* tr = new QHBoxLayout(tabs);
  tr->setContentsMargins(0, 0, 0, 0);
  tr->setSpacing(0);
  // "Import", not "Charakter": this tab is where a look comes IN -- MVLink, Armory, .chr --
  // and "Charakter" already named the tool-bar button and the menu, so the one word pointed
  // at three different places and at none of them helpfully. Somebody looking for the MVLink
  // field had no reason to open it. The order is unchanged; --tab <0..3> indexes this.
  const struct { const char* label; } kTabs[] = {
    { "Anpassen" }, { "Import" }, { "Licht" }, { "Export" }
  };
  for (int i = 0; i < 4; ++i) {
    auto* t = new QToolButton;
    t->setText(QString::fromUtf8(kTabs[i].label));
    t->setCheckable(true);
    t->setCursor(Qt::PointingHandCursor);
    t->setProperty("role", "tab");
    t->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    // The same weight checked and unchecked. The old strip switched the active tab to
    // DemiBold inside a fixed-width cell, so selecting a tab made its own text wider than
    // the cell it had to fit into -- the label could clip on selection alone.
    t->setFont(typo::font(typo::Strong));
    t->setToolTip(QString::fromUtf8("%1 (Alt+%2)")
                    .arg(QString::fromUtf8(kTabs[i].label)).arg(i + 1));
    connect(t, &QToolButton::clicked, this, [this, i]() { setInspectorTab(i); });
    inspectorTabs_.push_back(t);
    tr->addWidget(t, 1);
  }
  col->addWidget(tabs);

  inspectorStack_ = new QStackedWidget;

  // Page 0 -- character
  auto* charBody = new QWidget;
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
  auto* ib = new QVBoxLayout(ioBody);
  ib->setContentsMargins(16, 16, 16, 24);
  characterIoHost_ = new QWidget;
  auto* ih = new QVBoxLayout(characterIoHost_);
  ih->setContentsMargins(0, 0, 0, 0);
  ib->addWidget(characterIoHost_);
  ib->addStretch(1);
  inspectorStack_->addWidget(wrapScroll(ioBody));

  // Page 2 -- lighting. The four scene lights were configurable in principle since
  // Phase 0 (SceneLighting is widget-free) but had no UI at all.
  auto* lightBody = new QWidget;
  auto* lb = new QVBoxLayout(lightBody);
  lb->setContentsMargins(16, 16, 16, 24);
  lightPanel_ = new LightPanel(canvas_);
  lb->addWidget(lightPanel_);
  lb->addStretch(1);
  inspectorStack_->addWidget(wrapScroll(lightBody));

  // Page 3 -- export (filled in by main once the exporters are loaded)
  auto* expBody = new QWidget;
  auto* eb = new QVBoxLayout(expBody);
  eb->setContentsMargins(16, 16, 16, 24);
  exportHost_ = new QWidget;
  auto* eh = new QVBoxLayout(exportHost_);
  eh->setContentsMargins(0, 0, 0, 0);
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
  for (int i = 0; i < (int)inspectorTabs_.size(); ++i)
    inspectorTabs_[i]->setChecked(i == index);
}

QWidget* MainWindow::buildStatusBar()
{
  auto* w = new QWidget;
  styled(w);
  w->setProperty("role", "chrome");
  w->setFixedHeight(ui::px(26));
  auto* r = new QHBoxLayout(w);
  r->setContentsMargins(met::sp(met::Edge), 0, met::sp(met::Gap), 0);
  r->setSpacing(met::sp(met::Edge));

  statusLabel_ = new QLabel(QString::fromUtf8("Bereit"));
  statusLabel_->setFont(typo::font(typo::Small));
  statusLabel_->setProperty("role", "hint");
  r->addWidget(statusLabel_);

  // What is loaded, in words rather than as a second copy of the path. The path itself is
  // in the title bar, once.
  statusPathLabel_ = new ElidedLabel;
  statusPathLabel_->setFont(typo::font(typo::Small));
  statusPathLabel_->setProperty("role", "hint");
  statusPathLabel_->setElideMode(Qt::ElideRight);
  r->addWidget(statusPathLabel_, 1);

  // Was the literal "FBX · OBJ · glTF". There is no glTF exporter, so the text promised a
  // format the build cannot produce. main() fills this in from the exporters that loaded.
  formatsLabel_ = new QLabel;
  formatsLabel_->setFont(typo::font(typo::Mono));
  formatsLabel_->setProperty("role", "mono");
  r->addWidget(formatsLabel_);

  // Without a native frame there is no resize edge, so give the status bar a grip. The
  // window also has an 8px resize band on every edge (nativeEvent), but a visible corner
  // is what people reach for.
  auto* grip = new QSizeGrip(w);
  grip->setFixedSize(ui::px(14), ui::px(14));
  r->addWidget(grip, 0, Qt::AlignBottom | Qt::AlignRight);
  return w;
}

void MainWindow::setStatus(const QString& text)
{
  if (statusLabel_)
    statusLabel_->setText(text.isEmpty() ? QString::fromUtf8("Bereit") : text);
}

void MainWindow::setSummary(const QString& text)
{
  if (statusPathLabel_)
    statusPathLabel_->setFullText(text);
}
