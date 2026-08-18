// The visible installer: a frameless 760x520 wizard in the application's own design.
//
// This program does not install anything itself. It collects the three decisions the
// setup actually offers (target folder, desktop icon, wx cleanup), then re-runs the
// Inno executable it was extracted from in /VERYSILENT mode and tails Inno's /LOG file
// for the progress bar and the protocol pane. Everything that must be correct across
// upgrades -- registry entries, the uninstaller, stale-file deletion -- stays in the
// battle-tested engine; everything the user sees is here.
//
// The design is the mock-up "ModelViewer Midnight Setup.html", which in turn lifted
// every colour verbatim from Theme.h. Including Theme.h closes the circle: the setup
// is provably in the application's palette, not in a copy of it.
//
// Command line (all optional, with development fallbacks):
//   --inner <path>     the Inno setup exe to re-run silently; without it the run page
//                      simulates, which is how the UI is developed and screenshotted
//   --totalfiles <n>   staged file count, denominator of the progress bar
//   --sizemb <n>       payload size on disk, shown on the welcome and folder pages
//   --version <v>      display version

#include "Theme.h"

#include <QApplication>
#include <QDesktopServices>
#include <QElapsedTimer>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QTextEdit>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWindow>

#include <functional>
#include <set>

// Tokens the mock-up uses beyond Theme.h. Same names as its CSS variables.
namespace tok {
const char* const kBorderFocus = "#3a434f";
const char* const kScrubTrack  = "#0a0d10";
const char* const kMeta        = "#7d8693";
const char* const kTextQuiet   = "#cdd3dc";
const char* const kBrandInk    = "#c9b6e8";
const char* const kScrollThumb = "#262c35";
}

// ---------------------------------------------------------------------------------
// Small helpers in the style of MainWindow.cpp
// ---------------------------------------------------------------------------------

static QLabel* mk(const QString& text, const QString& family, int pt, const char* colour,
                  bool demi = false, qreal spacing = 0)
{
  auto* l = new QLabel(text);
  QFont f(family, pt, demi ? QFont::DemiBold : QFont::Normal);
  if (spacing > 0)
    f.setLetterSpacing(QFont::AbsoluteSpacing, spacing);
  l->setFont(f);
  l->setStyleSheet(QString("color:%1; background:transparent; border:none;").arg(colour));
  return l;
}

static QLabel* kicker(const QString& text)
{
  // The small uppercase line above every page title.
  return mk(text.toUpper(), "Segoe UI", 7, tok::kDim, false, 1.3);
}

static QLabel* pageTitle(const QString& text)
{
  return mk(text, "Georgia", 14, tok::kText, true, 0.6);
}

static QLabel* bodyText(const QString& text)
{
  auto* l = mk(text, "Segoe UI", 9, tok::kTextSoft);
  l->setWordWrap(true);
  return l;
}

static QFrame* card()
{
  auto* f = new QFrame;
  f->setStyleSheet(QString("QFrame { background:%1; border:1px solid %2; border-radius:8px; }")
                     .arg(tok::kCard).arg(tok::kBorder2));
  return f;
}

static QFrame* hairline()
{
  auto* f = new QFrame;
  f->setFixedHeight(1);
  f->setStyleSheet(QString("background:%1; border:none;").arg(tok::kBorder2));
  return f;
}

static QPushButton* flatButton(const QString& text)
{
  auto* b = new QPushButton(text);
  b->setCursor(Qt::PointingHandCursor);
  b->setFont(QFont("Segoe UI", 9));
  b->setStyleSheet(QString(
    "QPushButton { color:%1; background:%2; border:1px solid %3; border-radius:7px;"
    "              padding:8px 14px; }"
    "QPushButton:hover { background:%4; }")
    .arg(tok::kTextSoft).arg(tok::kRaised).arg(tok::kBorder).arg(tok::kRaised2));
  return b;
}

// ---------------------------------------------------------------------------------
// Title bar: the application's own painted surface, minus the maximise button.
// ---------------------------------------------------------------------------------

class TitleBar : public QWidget
{
public:
  explicit TitleBar(QWidget* window) : QWidget(window), window_(window)
  {
    setFixedHeight(38);
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(12, 0, 4, 0);
    row->setSpacing(8);
    row->addWidget(mk(QString::fromUtf8("◆"), "Segoe UI Symbol", 8, tok::kAccent));
    row->addWidget(mk(QString::fromUtf8("ModelViewer: Midnight — Setup"),
                      "Segoe UI", 8, "#99a2af", false, 0.3));
    row->addStretch(1);

    auto btn = [](const QString& glyph, const char* hoverBg, const char* hoverFg) {
      auto* b = new QPushButton(glyph);
      b->setFixedSize(30, 22);
      b->setCursor(Qt::PointingHandCursor);
      b->setFocusPolicy(Qt::NoFocus);
      b->setFont(QFont("Segoe UI Symbol", 8));
      b->setStyleSheet(QString(
        "QPushButton { color:%1; background:transparent; border:none; border-radius:5px; }"
        "QPushButton:hover { background:%2; color:%3; }")
        .arg(tok::kDim).arg(hoverBg).arg(hoverFg));
      return b;
    };
    auto* mini = btn(QString::fromUtf8("—"), tok::kRaised, tok::kTextSoft);
    auto* close = btn(QString::fromUtf8("✕"), tok::kDanger, tok::kOnAccent);
    QObject::connect(mini, &QPushButton::clicked, window, &QWidget::showMinimized);
    QObject::connect(close, &QPushButton::clicked, window, &QWidget::close);
    row->addWidget(mini);
    row->addWidget(close);
  }

protected:
  void paintEvent(QPaintEvent*) override
  {
    QPainter p(this);
    QLinearGradient g(0, 0, 0, height());
    g.setColorAt(0.0, QColor(tok::kTitleTop));
    g.setColorAt(1.0, QColor(tok::kTitleBot));
    p.fillRect(rect(), g);
    p.fillRect(rect(), QBrush(grain()));
    p.fillRect(0, height() - 1, width(), 1, QColor(tok::kBorder2));
  }

  // Qt 5.13 has no QWindow::startSystemMove yet; classic offset dragging instead.
  void mousePressEvent(QMouseEvent* e) override
  {
    if (e->button() == Qt::LeftButton)
      dragOffset_ = e->globalPos() - window_->frameGeometry().topLeft();
  }

  void mouseMoveEvent(QMouseEvent* e) override
  {
    if (e->buttons() & Qt::LeftButton)
      window_->move(e->globalPos() - dragOffset_);
  }

private:
  // Verbatim from MainWindow.cpp: fixed-seed value noise, cached as a QImage.
  static const QImage& grain()
  {
    static const QImage tile = [] {
      const int n = 64;
      QImage img(n, n, QImage::Format_ARGB32_Premultiplied);
      img.fill(Qt::transparent);
      quint32 s = 0x9e3779b9u;
      for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
          s ^= s << 13; s ^= s >> 17; s ^= s << 5;
          const int v = int(s % 16u);
          if (v < 10)
            continue;
          const bool light = (s & 0x30000u) == 0;
          img.setPixelColor(x, y, light ? QColor(255, 255, 255, v - 8)
                                        : QColor(0, 0, 0, v + 6));
        }
      }
      return img;
    }();
    return tile;
  }

  QWidget* window_;
  QPoint dragOffset_;
};

// ---------------------------------------------------------------------------------
// Sidebar: wordmark, step spine, violet glow.
// ---------------------------------------------------------------------------------

struct SpineRow
{
  QLabel* dot;
  QLabel* label;
};

class Sidebar : public QWidget
{
public:
  Sidebar(const QStringList& steps, const QString& version)
  {
    setFixedWidth(248);
    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(18, 20, 18, 16);
    col->setSpacing(0);

    auto* markRow = new QHBoxLayout;
    markRow->setSpacing(7);
    markRow->addWidget(mk(QString::fromUtf8("◆"), "Segoe UI Symbol", 9, tok::kAccent), 0,
                       Qt::AlignBaseline);
    markRow->addWidget(mk("MODELVIEWER", "Georgia", 14, tok::kBrandInk, true, 1.3), 0,
                       Qt::AlignBaseline);
    markRow->addStretch(1);
    col->addLayout(markRow);

    auto indent = [](QWidget* w) {
      auto* r = new QHBoxLayout;
      r->setContentsMargins(19, 0, 0, 0);
      r->addWidget(w);
      r->addStretch(1);
      return r;
    };
    auto* mid = mk("MIDNIGHT", "Georgia", 10, tok::kMuted, false, 3.0);
    auto* midRow = indent(mid);
    midRow->setContentsMargins(19, 3, 0, 0);
    col->addLayout(midRow);
    auto* ver = mk(version + QString::fromUtf8(" · Early Access"), "Consolas", 7, tok::kDim);
    auto* verRow = indent(ver);
    verRow->setContentsMargins(19, 10, 0, 0);
    col->addLayout(verRow);

    col->addSpacing(26);
    for (const QString& s : steps) {
      auto* row = new QHBoxLayout;
      row->setContentsMargins(0, 5, 0, 5);
      row->setSpacing(10);
      SpineRow r;
      r.dot = new QLabel;
      r.dot->setFixedSize(17, 17);
      r.dot->setAlignment(Qt::AlignCenter);
      r.dot->setFont(QFont("Consolas", 7));
      r.label = mk(s, "Segoe UI", 8, tok::kFaint, false, 0.2);
      row->addWidget(r.dot);
      row->addWidget(r.label);
      row->addStretch(1);
      col->addLayout(row);
      rows_.push_back(r);
    }
    col->addStretch(1);

    auto* sys = mk(QString::fromUtf8("x64 · Windows 10 / 11"), "Consolas", 7, tok::kFaint);
    auto* bld = mk("Setup-Build " + version, "Consolas", 7, tok::kFaint);
    col->addWidget(sys);
    col->addSpacing(4);
    col->addWidget(bld);

    setCurrent(0);
  }

  void setCurrent(int idx)
  {
    for (int i = 0; i < int(rows_.size()); ++i) {
      const bool done = i < idx;
      const bool cur = i == idx;
      const SpineRow& r = rows_[size_t(i)];
      r.dot->setText(done ? QString::fromUtf8("✓") : QString::number(i + 1));
      r.dot->setStyleSheet(QString(
        "background:%1; color:%2; border:1px solid %3; border-radius:8px;")
        .arg(cur ? tok::kAccentBg : "transparent")
        .arg(cur ? tok::kAccentHi : done ? tok::kAccent : tok::kFaint)
        .arg(cur ? tok::kAccentHi : done ? tok::kAccentBr : tok::kBorder2));
      r.label->setStyleSheet(QString("color:%1; background:transparent;")
        .arg(cur ? tok::kText : done ? tok::kMeta : tok::kFaint));
    }
  }

protected:
  void paintEvent(QPaintEvent*) override
  {
    QPainter p(this);
    QLinearGradient g(0, 0, 0, height());
    g.setColorAt(0.0, QColor(tok::kTitleTop));
    g.setColorAt(1.0, QColor(tok::kVoid));
    p.fillRect(rect(), g);
    // The violet bloom behind the wordmark.
    QRadialGradient r(QPointF(100, 80), 170);
    QColor glow(tok::kAccent);
    glow.setAlphaF(0.14);
    r.setColorAt(0.0, glow);
    glow.setAlphaF(0.0);
    r.setColorAt(0.62, glow);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QBrush(r));
    p.fillRect(width() - 1, 0, 1, height(), QColor(tok::kBorder2));
  }

private:
  std::vector<SpineRow> rows_;
};

// ---------------------------------------------------------------------------------
// Check row: the mock-up's 14px box with a tick, clickable along its whole width.
// ---------------------------------------------------------------------------------

class CheckRow : public QWidget
{
public:
  CheckRow(const QString& text, bool on, bool locked = false, const QString& hint = QString())
    : on_(on), locked_(locked)
  {
    setCursor(locked ? Qt::ArrowCursor : Qt::PointingHandCursor);
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(10);
    box_ = new QLabel;
    box_->setFixedSize(14, 14);
    box_->setAlignment(Qt::AlignCenter);
    box_->setFont(QFont("Segoe UI", 7));
    label_ = mk(text, "Segoe UI", 9, tok::kTextSoft);
    label_->setWordWrap(true);
    row->addWidget(box_, 0, Qt::AlignTop);
    row->addWidget(label_, 1);
    if (!hint.isEmpty())
      row->addWidget(mk(hint, "Consolas", 7, tok::kDim), 0, Qt::AlignTop);
    paint();
  }

  bool isChecked() const { return on_; }
  std::function<void(bool)> onToggle;

protected:
  void mousePressEvent(QMouseEvent*) override
  {
    if (locked_)
      return;
    on_ = !on_;
    paint();
    if (onToggle)
      onToggle(on_);
  }

private:
  void paint()
  {
    if (locked_) {
      // Part of the installation, not a choice: a grey tick that cannot be removed.
      box_->setText(QString::fromUtf8("✓"));
      box_->setStyleSheet(QString("background:%1; color:%2; border:1px solid %3; border-radius:3px;")
                            .arg(tok::kRaised).arg(tok::kTextQuiet).arg(tok::kBorderFocus));
      return;
    }
    box_->setText(on_ ? QString::fromUtf8("✓") : QString());
    box_->setStyleSheet(QString("background:%1; color:%2; border:1px solid %3; border-radius:3px;")
                          .arg(on_ ? tok::kAccent : tok::kCardAlt)
                          .arg(tok::kOnAccent)
                          .arg(on_ ? tok::kAccentHi : tok::kBorderFocus));
  }

  QLabel* box_;
  QLabel* label_;
  bool on_;
  bool locked_;
};

// ---------------------------------------------------------------------------------
// Progress bar and the folder page's disk bar.
// ---------------------------------------------------------------------------------

class Bar : public QWidget
{
public:
  explicit Bar(int h) { setFixedHeight(h); }

  // Segments painted left to right; the remainder stays groove-dark.
  void set(double aFrac, const char* aColour, double bFrac = 0, const char* bColour = nullptr)
  {
    a_ = aFrac; aC_ = aColour; b_ = bFrac; bC_ = bColour;
    update();
  }

protected:
  void paintEvent(QPaintEvent*) override
  {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip;
    clip.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 2, 2);
    p.setClipPath(clip);
    p.fillRect(rect(), QColor(tok::kScrubTrack));
    int x = 0;
    const int wA = int(width() * qBound(0.0, a_, 1.0));
    p.fillRect(0, 0, wA, height(), QColor(aC_));
    x += wA;
    if (bC_) {
      const int wB = int(width() * qBound(0.0, b_, 1.0));
      p.fillRect(x, 0, wB, height(), QColor(bC_));
    }
    p.setClipping(false);
    p.setPen(QColor(tok::kBorder2));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 2, 2);
  }

private:
  double a_ = 0, b_ = 0;
  const char* aC_ = tok::kAccent;
  const char* bC_ = nullptr;
};

// ---------------------------------------------------------------------------------
// The wizard.
// ---------------------------------------------------------------------------------

struct Options
{
  QString innerExe;      // the Inno setup to run silently; empty = simulate
  int totalFiles = 1300;
  int sizeMb = 450;
  QString version = "1.8.0";
  QString shotsDir;      // autopilot: step through, grab each page as PNG, quit
};

class SetupWindow : public QWidget
{
public:
  explicit SetupWindow(const Options& opt) : opt_(opt)
  {
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setFixedSize(760, 520);
    setStyleSheet(QString("SetupWindow { background:%1; }").arg(tok::kApp));
    setAttribute(Qt::WA_StyledBackground, true);

    readExisting();
    dir_ = existingDir_.isEmpty()
             ? QDir::toNativeSeparators(
                 QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                   .replace("/AppData/Roaming", "/AppData/Local") + "/Programs/ModelViewer Midnight")
             : existingDir_;

    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(1, 1, 1, 1);   // the 1px window border painted below
    col->setSpacing(0);
    col->addWidget(new TitleBar(this));

    auto* body = new QHBoxLayout;
    body->setSpacing(0);
    sidebar_ = new Sidebar({ "Willkommen", "Lizenz", "Zielordner",
                             QString::fromUtf8("Verknüpfungen"), "Installation", "Fertig" },
                           opt_.version);
    body->addWidget(sidebar_);

    auto* right = new QVBoxLayout;
    right->setSpacing(0);
    pages_ = new QStackedWidget;
    pages_->addWidget(wrapPage(buildWelcome()));
    pages_->addWidget(wrapPage(buildLicense()));
    pages_->addWidget(wrapPage(buildFolder()));
    pages_->addWidget(wrapPage(buildShortcuts()));
    pages_->addWidget(wrapPage(buildRun()));
    pages_->addWidget(wrapPage(buildFinish()));
    right->addWidget(pages_, 1);
    right->addWidget(buildFooter());
    body->addLayout(right, 1);
    col->addLayout(body, 1);

    refresh();
    if (!opt_.shotsDir.isEmpty())
      startAutopilot();
  }

protected:
  void paintEvent(QPaintEvent*) override
  {
    QPainter p(this);
    p.fillRect(rect(), QColor(tok::kApp));
    p.setPen(QColor(tok::kBorder));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
  }

  void closeEvent(QCloseEvent* e) override
  {
    // While Inno is writing files there is nothing sane a hard close can do.
    if (running_)
      e->ignore();
  }

  void keyPressEvent(QKeyEvent* e) override
  {
    // Scripted walkthrough for screenshots and smoke tests; not documented in the UI.
    if (e->key() == Qt::Key_F8) {
      if (step_ == 1 && !eula_->isChecked()) {
        QMouseEvent click(QEvent::MouseButtonPress, QPointF(1, 1), Qt::LeftButton,
                          Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(eula_, &click);
      }
      onNext();
      return;
    }
    QWidget::keyPressEvent(e);
  }

private:
  // --- pages ---------------------------------------------------------------

  static QWidget* wrapPage(QWidget* page)
  {
    auto* s = new QScrollArea;
    s->setWidget(page);
    s->setWidgetResizable(true);
    s->setFrameShape(QFrame::NoFrame);
    s->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    s->setStyleSheet(QString(
      "QScrollArea { background:transparent; border:none; }"
      "QScrollBar:vertical { background:%1; width:10px; }"
      "QScrollBar::handle:vertical { background:%2; border-radius:5px; min-height:30px; }"
      "QScrollBar::add-line, QScrollBar::sub-line { height:0; }")
      .arg(tok::kApp).arg(tok::kScrollThumb));
    return s;
  }

  static QVBoxLayout* pageLayout(QWidget* page)
  {
    page->setStyleSheet("background:transparent;");
    auto* l = new QVBoxLayout(page);
    l->setContentsMargins(24, 22, 24, 18);
    l->setSpacing(0);
    return l;
  }

  static void addCardRow(QVBoxLayout* cardCol, const QString& name, const QString& sub,
                         const QString& right, bool last)
  {
    auto* row = new QHBoxLayout;
    row->setContentsMargins(0, 9, 0, 9);
    QString text = name;
    if (!sub.isEmpty())
      text += QString(" <span style='color:%1'>· %2</span>").arg(tok::kDim).arg(sub);
    auto* l = mk(QString(), "Segoe UI", 8, tok::kTextSoft);
    l->setText(text);
    l->setTextFormat(Qt::RichText);
    row->addWidget(l);
    row->addStretch(1);
    row->addWidget(mk(right, "Consolas", 8, tok::kMeta));
    cardCol->addLayout(row);
    if (!last)
      cardCol->addWidget(hairline());
  }

  QWidget* buildWelcome()
  {
    auto* page = new QWidget;
    auto* l = pageLayout(page);
    l->addWidget(kicker("Willkommen"));
    l->addSpacing(7);
    l->addWidget(pageTitle("ModelViewer: Midnight installieren"));
    l->addSpacing(8);
    l->addWidget(bodyText(QString::fromUtf8(
      "Dieses Setup installiert das Qt-Frontend samt Engine, das MVLink-Addon für "
      "World of Warcraft und das Blender-Add-on für den FBX-Import mit "
      ".wmvmat.json-Sidecar. Schließe eine laufende ModelViewer-Instanz, bevor du "
      "fortfährst.")));

    l->addSpacing(16);
    auto* c = card();
    auto* cc = new QVBoxLayout(c);
    cc->setContentsMargins(14, 4, 14, 4);
    cc->setSpacing(0);
    addCardRow(cc, "ModelViewer: Midnight " + opt_.version, "Programm, Engine, Spieldaten",
               QString::number(opt_.sizeMb) + " MB", false);
    addCardRow(cc, "MVLink", "WoW-Addon, Installation aus dem Programm", "< 1 MB", false);
    addCardRow(cc, "Blender-Add-on", "io_import_wmv_fbx", "< 1 MB", true);
    l->addWidget(c);

    l->addSpacing(14);
    auto* status = new QHBoxLayout;
    status->setSpacing(8);
    auto* dot = new QLabel;
    dot->setFixedSize(6, 6);
    dot->setStyleSheet(QString("background:%1; border-radius:3px;")
                         .arg(existingDir_.isEmpty() ? tok::kOk : tok::kAccent));
    status->addWidget(dot);
    const QString found = existingDir_.isEmpty()
      ? "Keine bestehende Installation gefunden."
      : QString::fromUtf8("Version %1 gefunden — wird aktualisiert.").arg(existingVersion_);
    status->addWidget(mk(found, "Segoe UI", 8, tok::kMeta));
    status->addStretch(1);
    l->addLayout(status);
    l->addStretch(1);
    return page;
  }

  QWidget* buildLicense()
  {
    auto* page = new QWidget;
    auto* l = pageLayout(page);
    l->addWidget(kicker("Lizenz"));
    l->addSpacing(7);
    l->addWidget(pageTitle("Lizenzvereinbarung"));
    l->addSpacing(10);

    auto* box = new QTextEdit;
    box->setReadOnly(true);
    box->setFrameShape(QFrame::NoFrame);
    box->setFixedHeight(252);
    box->setFont(QFont("Segoe UI", 8));
    box->setStyleSheet(QString(
      "QTextEdit { background:%1; border:1px solid %2; border-radius:6px; padding:10px;"
      "            color:%3; }"
      "QScrollBar:vertical { background:transparent; width:10px; }"
      "QScrollBar::handle:vertical { background:%4; border-radius:5px; min-height:30px; }"
      "QScrollBar::add-line, QScrollBar::sub-line { height:0; }")
      .arg(tok::kCardAlt).arg(tok::kBorder2).arg(tok::kTextSoft).arg(tok::kScrollThumb));
    box->setPlainText(licenseText());
    l->addWidget(box);

    l->addSpacing(14);
    eula_ = new CheckRow("Ich akzeptiere die Bedingungen der Lizenzvereinbarung.", false);
    eula_->onToggle = [this](bool) { refresh(); };
    l->addWidget(eula_);
    l->addStretch(1);
    return page;
  }

  QWidget* buildFolder()
  {
    auto* page = new QWidget;
    auto* l = pageLayout(page);
    l->addWidget(kicker("Zielordner"));
    l->addSpacing(7);
    l->addWidget(pageTitle(QString::fromUtf8("Installationsort wählen")));
    l->addSpacing(8);
    l->addWidget(bodyText(QString::fromUtf8(
      "Der Ordner wird angelegt, falls er nicht existiert. Das Programm legt seine "
      "Einstellungen, das Protokoll und den Datenbank-Cache neben sich ab und braucht "
      "deshalb einen beschreibbaren Ordner — ohne Administratorrechte ist das der "
      "Benutzerordner, nicht „C:\\Programme“.")));

    l->addSpacing(16);
    auto* row = new QHBoxLayout;
    row->setSpacing(8);
    pathField_ = new QLineEdit(dir_);
    pathField_->setReadOnly(true);
    pathField_->setCursorPosition(0);
    pathField_->setFixedHeight(28);
    pathField_->setFont(QFont("Consolas", 8));
    pathField_->setStyleSheet(QString(
      "QLineEdit { background:%1; border:1px solid %2; border-radius:6px; padding:4px 8px;"
      "            color:%3; }").arg(tok::kCard).arg(tok::kBorder).arg(tok::kTextQuiet));
    auto* browse = flatButton(QString::fromUtf8("Durchsuchen …"));
    QObject::connect(browse, &QPushButton::clicked, [this] {
      const QString d = QFileDialog::getExistingDirectory(this,
        QString::fromUtf8("Installationsort wählen"), dir_);
      if (!d.isEmpty()) {
        dir_ = QDir::toNativeSeparators(d);
        if (!dir_.endsWith("ModelViewer Midnight", Qt::CaseInsensitive))
          dir_ = dir_ + "\\ModelViewer Midnight";
        pathField_->setText(dir_);
        pathField_->setCursorPosition(0);
        updateDisk();
      }
    });
    row->addWidget(pathField_, 1);
    row->addWidget(browse);
    l->addLayout(row);

    l->addSpacing(18);
    auto* c = card();
    auto* cc = new QVBoxLayout(c);
    cc->setContentsMargins(14, 14, 14, 14);
    cc->setSpacing(0);
    auto* head = new QHBoxLayout;
    driveLabel_ = mk("LAUFWERK", "Segoe UI", 8, tok::kDim, false, 1.3);
    driveSize_ = mk("", "Consolas", 8, tok::kMeta);
    head->addWidget(driveLabel_);
    head->addStretch(1);
    head->addWidget(driveSize_);
    cc->addLayout(head);
    cc->addSpacing(12);
    diskBar_ = new Bar(8);
    cc->addWidget(diskBar_);
    cc->addSpacing(12);
    auto statRow = [&](const QString& k, QLabel*& v, const char* colour) {
      auto* r = new QHBoxLayout;
      r->setContentsMargins(0, 0, 0, 7);
      r->addWidget(mk(k, "Segoe UI", 8, tok::kTextSoft));
      r->addStretch(1);
      v = mk("", "Consolas", 8, colour);
      r->addWidget(v);
      cc->addLayout(r);
    };
    statRow(QString::fromUtf8("Benötigt"), needLabel_, tok::kAccentHi);
    statRow(QString::fromUtf8("Verfügbar"), availLabel_, tok::kMeta);
    statRow("Frei nach Installation", afterLabel_, tok::kMeta);
    l->addWidget(c);
    l->addStretch(1);
    updateDisk();
    return page;
  }

  QWidget* buildShortcuts()
  {
    auto* page = new QWidget;
    auto* l = pageLayout(page);
    l->addWidget(kicker(QString::fromUtf8("Verknüpfungen")));
    l->addSpacing(7);
    l->addWidget(pageTitle(QString::fromUtf8("Verknüpfungen & Aufräumen")));
    l->addSpacing(12);

    auto addRow = [&](CheckRow*& slot, const QString& text, bool on, bool locked,
                      const QString& hint) {
      l->addWidget(hairline());
      auto* wrap = new QWidget;
      auto* wl = new QVBoxLayout(wrap);
      wl->setContentsMargins(2, 11, 2, 11);
      slot = new CheckRow(text, on, locked, hint);
      wl->addWidget(slot);
      l->addWidget(wrap);
    };
    CheckRow* startmenu = nullptr;
    addRow(startmenu, QString::fromUtf8("Startmenü-Eintrag anlegen"), true, true,
           QString::fromUtf8("Startmenü"));
    addRow(desktop_, QString::fromUtf8("Desktop-Verknüpfung anlegen"), false, false,
           "Desktop");
    addRow(cleanwx_, QString::fromUtf8(
             "Dateien einer vorhandenen WoW-Model-Viewer-Installation im Zielordner "
             "entfernen"), false, false, QString::fromUtf8("Aufräumen"));
    l->addWidget(hairline());

    l->addSpacing(16);
    l->addWidget(bodyText(QString::fromUtf8(
      "Der Startmenü-Eintrag gehört zur Installation und wird bei der "
      "Deinstallation wieder entfernt. Das Aufräumen betrifft nur den alten "
      "wxWidgets-Viewer und nur, wenn er im selben Ordner liegt.")));
    l->addStretch(1);
    return page;
  }

  QWidget* buildRun()
  {
    auto* page = new QWidget;
    auto* l = pageLayout(page);
    runKicker_ = kicker("Installation");
    l->addWidget(runKicker_);
    l->addSpacing(7);
    runTitle_ = pageTitle("Dateien werden geschrieben");
    l->addWidget(runTitle_);
    l->addSpacing(14);

    auto* head = new QHBoxLayout;
    currentLine_ = mk(QString::fromUtf8("Vorbereitung …"), "Consolas", 8, tok::kTextQuiet);
    pctLabel_ = mk("0 %", "Consolas", 9, tok::kAccentHi);
    head->addWidget(currentLine_, 1);
    head->addWidget(pctLabel_);
    l->addLayout(head);
    l->addSpacing(8);
    progress_ = new Bar(6);
    l->addWidget(progress_);

    l->addSpacing(18);
    auto* logHead = new QHBoxLayout;
    logHead->addWidget(kicker("Protokoll"));
    logHead->addStretch(1);
    logCount_ = mk("0 Zeilen", "Consolas", 7, tok::kFaint);
    logHead->addWidget(logCount_);
    l->addLayout(logHead);
    l->addSpacing(7);

    log_ = new QTextEdit;
    log_->setReadOnly(true);
    log_->setFrameShape(QFrame::NoFrame);
    log_->setFixedHeight(206);
    log_->setLineWrapMode(QTextEdit::NoWrap);
    log_->setFont(QFont("Consolas", 8));
    log_->setStyleSheet(QString(
      "QTextEdit { background:%1; border:1px solid %2; border-radius:6px; padding:8px;"
      "            color:%3; }"
      "QScrollBar:vertical { background:transparent; width:10px; }"
      "QScrollBar::handle:vertical { background:%4; border-radius:5px; min-height:30px; }"
      "QScrollBar:horizontal { background:transparent; height:10px; }"
      "QScrollBar::handle:horizontal { background:%4; border-radius:5px; min-width:30px; }"
      "QScrollBar::add-line, QScrollBar::sub-line { width:0; height:0; }")
      .arg(tok::kCardAlt).arg(tok::kBorder2).arg(tok::kMuted).arg(tok::kScrollThumb));
    l->addWidget(log_);
    l->addStretch(1);
    return page;
  }

  QWidget* buildFinish()
  {
    auto* page = new QWidget;
    auto* l = pageLayout(page);
    l->addWidget(kicker("Fertig"));
    l->addSpacing(7);
    finishTitle_ = pageTitle("Installation abgeschlossen");
    l->addWidget(finishTitle_);
    l->addSpacing(8);
    l->addWidget(bodyText(QString::fromUtf8(
      "ModelViewer: Midnight ist eingerichtet. Beim ersten Start fragt das Programm nach "
      "deinem World-of-Warcraft-Ordner und baut den Datenbank-Cache auf; das dauert einen "
      "Moment.")));

    l->addSpacing(16);
    auto* c = card();
    summaryCol_ = new QVBoxLayout(c);
    summaryCol_->setContentsMargins(14, 4, 14, 4);
    summaryCol_->setSpacing(0);
    l->addWidget(c);

    l->addSpacing(16);
    launch_ = new CheckRow("ModelViewer: Midnight jetzt starten", true);
    l->addWidget(launch_);
    l->addSpacing(11);
    readme_ = new CheckRow("LIESMICH mit den Neuerungen dieser Version anzeigen", false);
    l->addWidget(readme_);
    l->addStretch(1);
    return page;
  }

  QWidget* buildFooter()
  {
    auto* bar = new QWidget;
    bar->setFixedHeight(54);
    bar->setStyleSheet(QString("background:%1; border-top:1px solid %2;")
                         .arg(tok::kBar).arg(tok::kBorder2));
    auto* row = new QHBoxLayout(bar);
    row->setContentsMargins(20, 0, 20, 0);
    row->setSpacing(8);

    cancel_ = new QPushButton("Abbrechen");
    cancel_->setCursor(Qt::PointingHandCursor);
    cancel_->setFlat(true);
    cancel_->setFont(QFont("Segoe UI", 9));
    cancel_->setStyleSheet(QString(
      "QPushButton { color:%1; background:transparent; border:none; }"
      "QPushButton:hover { color:%2; }"
      "QPushButton:disabled { color:%3; }")
      .arg(tok::kDim).arg(tok::kTextSoft).arg(tok::kFaint));
    QObject::connect(cancel_, &QPushButton::clicked, [this] {
      if (!running_)
        close();
    });
    row->addWidget(cancel_);
    row->addStretch(1);

    back_ = flatButton(QString::fromUtf8("Zurück"));
    QObject::connect(back_, &QPushButton::clicked, [this] {
      if (step_ > 0 && step_ < 4) {
        --step_;
        refresh();
      }
    });
    row->addWidget(back_);

    next_ = new QPushButton;
    next_->setMinimumWidth(104);
    next_->setFont(QFont("Segoe UI", 9, QFont::DemiBold));
    row->addWidget(next_);
    QObject::connect(next_, &QPushButton::clicked, [this] { onNext(); });
    return bar;
  }

  // --- state ---------------------------------------------------------------

  void onNext()
  {
    if (step_ == 4 && failed_) {
      close();               // "Schließen" after a failure
      return;
    }
    if (!canNext())
      return;
    if (step_ == 3) {
      step_ = 4;
      refresh();
      startInstall();
      return;
    }
    if (step_ == 5) {
      finishAndQuit();
      return;
    }
    ++step_;
    refresh();
  }

  bool canNext() const
  {
    if (step_ == 1)
      return eula_->isChecked();
    if (step_ == 4)
      return done_;
    return true;
  }

  void refresh()
  {
    pages_->setCurrentIndex(step_);
    sidebar_->setCurrent(step_);
    back_->setVisible(step_ > 0 && step_ < 4);
    cancel_->setEnabled(!running_);
    cancel_->setText(step_ == 5 ? QString::fromUtf8("Setup schließen") : "Abbrechen");

    QString label = "Weiter";
    if (step_ == 3) label = "Installieren";
    if (step_ == 4) label = done_ ? "Weiter" : (failed_ ? QString::fromUtf8("Schließen")
                                                        : QString::fromUtf8("Installiere …"));
    if (step_ == 5) label = "Fertigstellen";
    next_->setText(label);

    const bool enabled = canNext() || (step_ == 4 && failed_);
    next_->setCursor(enabled ? Qt::PointingHandCursor : Qt::ArrowCursor);
    next_->setStyleSheet(enabled
      ? QString("QPushButton { color:%1; background:%2; border:1px solid %3; border-radius:7px;"
                "              padding:9px 14px; }"
                "QPushButton:hover { background:%3; }")
          .arg(tok::kOnAccent).arg(tok::kAccent).arg(tok::kAccentHi)
      : QString("QPushButton { color:%1; background:%2; border:1px solid %3; border-radius:7px;"
                "              padding:9px 14px; }")
          .arg(tok::kFaint).arg(tok::kCard).arg(tok::kBorder2));
  }

  void startAutopilot()
  {
    // Screenshot mode: grab every page, advance like a user would, quit at the end.
    QDir().mkpath(opt_.shotsDir);
    auto* t = new QTimer(this);
    QObject::connect(t, &QTimer::timeout, [this, t] {
      static const char* names[] = { "welcome", "license", "folder", "shortcuts",
                                     "run", "finish" };
      if (!shotTaken_.count(step_)) {
        grab().save(opt_.shotsDir + "/" + names[step_] + ".png");
        shotTaken_.insert(step_);
      }
      if (step_ == 4 && !done_ && !failed_)
        return;                                     // let the (simulated) install finish
      if (step_ == 5) {
        t->stop();
        QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        return;
      }
      if (step_ == 1 && !eula_->isChecked()) {
        QMouseEvent click(QEvent::MouseButtonPress, QPointF(1, 1), Qt::LeftButton,
                          Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(eula_, &click);
      }
      onNext();
    });
    t->start(700);
  }

  void readExisting()
  {
    // Inno's per-user uninstall entry; _is1 is its convention for the first product copy.
    QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"
                  "{7C3E9A41-58D2-4B6E-9E17-3F84A5C0D2B9}_is1",
                  QSettings::NativeFormat);
    existingDir_ = QDir::toNativeSeparators(reg.value("InstallLocation").toString());
    while (existingDir_.endsWith('\\'))
      existingDir_.chop(1);
    existingVersion_ = reg.value("DisplayVersion").toString();
  }

  void updateDisk()
  {
    // The drive of the chosen folder; walk up until an existing path answers.
    QString probe = dir_;
    while (!probe.isEmpty() && !QFileInfo::exists(probe))
      probe = QFileInfo(probe).path();
    QStorageInfo st(probe.isEmpty() ? QDir::rootPath() : probe);
    const double totalGb = double(st.bytesTotal()) / (1024.0 * 1024 * 1024);
    const double availGb = double(st.bytesAvailable()) / (1024.0 * 1024 * 1024);
    const double needGb = opt_.sizeMb / 1024.0;
    driveLabel_->setText("LAUFWERK " + st.rootPath().left(2).toUpper());
    driveSize_->setText(QString::number(totalGb, 'f', 0) + " GB");
    const double usedFrac = totalGb > 0 ? (totalGb - availGb) / totalGb : 0;
    // The needed sliver would be invisible at true scale; the mock-up magnifies it 6x.
    diskBar_->set(usedFrac, tok::kRaised2, qMax(0.012, needGb / totalGb * 6), tok::kAccent);
    auto de = [](double v, int prec) { return QString::number(v, 'f', prec).replace('.', ','); };
    needLabel_->setText(QString::number(opt_.sizeMb) + " MB");
    availLabel_->setText(de(availGb, 1) + " GB");
    afterLabel_->setText(de(availGb - needGb, 1) + " GB");
  }

  QString licenseText() const
  {
    // Extracted next to this program by the Inno bootstrap ([Files] dontcopy).
    QFile f(QCoreApplication::applicationDirPath() + "/LICENSE.txt");
    if (f.open(QIODevice::ReadOnly))
      return QString::fromUtf8(f.readAll());
    return QString::fromUtf8(
      "GNU GENERAL PUBLIC LICENSE, Version 3.\n\n"
      "Der vollständige Text liegt dem Paket als LICENSE.txt bei und wird mit "
      "installiert.");
  }

  // --- the actual install --------------------------------------------------

  void startInstall()
  {
    running_ = true;
    elapsed_.start();
    refresh();

    logPath_ = QDir::tempPath() + "/mv-midnight-setup.log";
    QFile::remove(logPath_);
    logPos_ = 0;
    filesSeen_ = 0;

    if (opt_.innerExe.isEmpty()) {
      // Development without an engine: walk a canned script so the page can be styled.
      appendLog("+0,0s", "Simulation — kein --inner übergeben", tok::kAccentHi);
      simTimer_ = new QTimer(this);
      QObject::connect(simTimer_, &QTimer::timeout, [this] {
        static const char* files[] = { "core.dll", "wow.dll", "WoWModelViewer-Qt.exe",
                                       "Qt5Core.dll", "listfile.csv", "games\\wow\\database.xml" };
        if (simStep_ < 60) {
          setProgressFile(QString::fromUtf8(files[simStep_ % 6]));
          setPct(simStep_ * 100 / 60);
          ++simStep_;
        } else {
          simTimer_->stop();
          onInstallDone(0, QProcess::NormalExit);
        }
      });
      simTimer_->start(60);
      return;
    }

    QStringList args{ "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART",
                      "/DIR=" + dir_, "/LOG=" + QDir::toNativeSeparators(logPath_) };
    QStringList tasks;
    tasks << (desktop_->isChecked() ? "desktopicon" : "!desktopicon");
    if (cleanwx_->isChecked())
      tasks << "cleanwx";
    args << "/MERGETASKS=" + tasks.join(',');

    proc_ = new QProcess(this);
    QObject::connect(proc_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                     [this](int code, QProcess::ExitStatus st) { onInstallDone(code, st); });
    QObject::connect(proc_, &QProcess::errorOccurred, [this](QProcess::ProcessError) {
      appendLog(stamp(), "Setup-Prozess konnte nicht gestartet werden: "
                           + proc_->errorString(), tok::kDanger);
      onInstallDone(-1, QProcess::CrashExit);
    });
    proc_->start(opt_.innerExe, args);

    tail_ = new QTimer(this);
    QObject::connect(tail_, &QTimer::timeout, [this] { tailLog(); });
    tail_->start(120);
  }

  QString stamp() const
  {
    return "+" + QString::number(elapsed_.elapsed() / 1000.0, 'f', 1).replace('.', ',') + "s";
  }

  void tailLog()
  {
    QFile f(logPath_);
    if (!f.open(QIODevice::ReadOnly))
      return;
    f.seek(logPos_);
    const QByteArray chunk = f.readAll();
    logPos_ = f.pos();
    if (chunk.isEmpty())
      return;
    pending_ += chunk;
    int nl;
    while ((nl = pending_.indexOf('\n')) >= 0) {
      const QString line = QString::fromUtf8(pending_.left(nl)).trimmed();
      pending_.remove(0, nl + 1);
      const int di = line.indexOf("Dest filename: ");
      if (di >= 0) {
        ++filesSeen_;
        const QString path = line.mid(di + 15);
        setProgressFile(path.mid(path.lastIndexOf('\\') + 1));
        setPct(qMin(99, int(qint64(filesSeen_) * 100 / qMax(1, opt_.totalFiles))));
        if ((filesSeen_ % 25) == 1 || path.endsWith(".exe") || path.endsWith(".dll"))
          appendLog(stamp(), "Entpacke " + QDir::toNativeSeparators(path), tok::kMuted);
      } else if (line.contains("Starting the installation process")) {
        appendLog(stamp(), "Installation beginnt", tok::kMuted);
      } else if (line.contains("Installation process succeeded")) {
        appendLog(stamp(), "Dateien geschrieben, Einträge registriert", tok::kOk);
      } else if (line.contains("Exception message:") || line.contains("Error:")) {
        appendLog(stamp(), line, tok::kDanger);
      }
    }
  }

  void setProgressFile(const QString& name)
  {
    currentLine_->setText(name);
  }

  void setPct(int pct)
  {
    pctLabel_->setText(QString::number(pct) + " %");
    progress_->set(pct / 100.0, tok::kAccent);
  }

  void appendLog(const QString& t, const QString& text, const char* colour)
  {
    log_->append(QString("<span style='color:%1'>%2</span>&nbsp;&nbsp;"
                         "<span style='color:%3'>%4</span>")
                   .arg(tok::kFaint).arg(t).arg(colour)
                   .arg(text.toHtmlEscaped()));
    ++logLines_;
    logCount_->setText(QString::number(logLines_) + " Zeilen");
    log_->verticalScrollBar()->setValue(log_->verticalScrollBar()->maximum());
  }

  void onInstallDone(int code, QProcess::ExitStatus st)
  {
    if (tail_) {
      tail_->stop();
      tailLog();   // drain what the last tick missed
    }
    running_ = false;
    const int secs = int(elapsed_.elapsed() / 1000);
    if (code == 0 && st == QProcess::NormalExit) {
      done_ = true;
      setPct(100);
      runTitle_->setText("Dateien geschrieben");
      appendLog(stamp(), QString::fromUtf8("Installation abgeschlossen in %1 s").arg(secs),
                tok::kOk);
      buildSummary(secs);
      step_ = 5;
    } else {
      failed_ = true;
      runTitle_->setText("Installation fehlgeschlagen");
      appendLog(stamp(), QString::fromUtf8(
                  "Setup endete mit Code %1 — vollständiges Protokoll: %2")
                  .arg(code).arg(QDir::toNativeSeparators(logPath_)), tok::kDanger);
    }
    refresh();
  }

  void buildSummary(int secs)
  {
    QString links = QString::fromUtf8("Startmenü");
    if (desktop_->isChecked())
      links += " + Desktop";
    struct Row { QString k, v; } rows[] = {
      { "Zielordner", dir_ },
      { "Version", opt_.version },
      { QString::fromUtf8("Verknüpfungen"), links },
      { "Dauer", QString::number(secs) + " s" },
    };
    for (int i = 0; i < 4; ++i) {
      auto* r = new QHBoxLayout;
      r->setContentsMargins(0, 9, 0, 9);
      r->addWidget(mk(rows[i].k, "Segoe UI", 8, tok::kDim));
      r->addStretch(1);
      auto* v = mk(rows[i].v, "Consolas", 8, tok::kTextQuiet);
      r->addWidget(v);
      summaryCol_->addLayout(r);
      if (i < 3)
        summaryCol_->addWidget(hairline());
    }
  }

  void finishAndQuit()
  {
    if (readme_ && readme_->isChecked())
      QDesktopServices::openUrl(QUrl::fromLocalFile(dir_ + "/LIESMICH.txt"));
    if (launch_ && launch_->isChecked())
      QProcess::startDetached(dir_ + "/WoWModelViewer-Qt.exe", {}, dir_);
    close();
  }

  // --- members -------------------------------------------------------------

  Options opt_;
  int step_ = 0;
  bool running_ = false;
  bool done_ = false;
  bool failed_ = false;

  QString dir_;
  QString existingDir_;
  QString existingVersion_;

  Sidebar* sidebar_ = nullptr;
  QStackedWidget* pages_ = nullptr;
  QPushButton* cancel_ = nullptr;
  QPushButton* back_ = nullptr;
  QPushButton* next_ = nullptr;

  CheckRow* eula_ = nullptr;
  CheckRow* desktop_ = nullptr;
  CheckRow* cleanwx_ = nullptr;
  CheckRow* launch_ = nullptr;
  CheckRow* readme_ = nullptr;

  QLineEdit* pathField_ = nullptr;
  QLabel* driveLabel_ = nullptr;
  QLabel* driveSize_ = nullptr;
  QLabel* needLabel_ = nullptr;
  QLabel* availLabel_ = nullptr;
  QLabel* afterLabel_ = nullptr;
  Bar* diskBar_ = nullptr;

  QLabel* runKicker_ = nullptr;
  QLabel* runTitle_ = nullptr;
  QLabel* currentLine_ = nullptr;
  QLabel* pctLabel_ = nullptr;
  QLabel* logCount_ = nullptr;
  Bar* progress_ = nullptr;
  QTextEdit* log_ = nullptr;
  QLabel* finishTitle_ = nullptr;
  QVBoxLayout* summaryCol_ = nullptr;

  QProcess* proc_ = nullptr;
  QTimer* tail_ = nullptr;
  QTimer* simTimer_ = nullptr;
  int simStep_ = 0;
  std::set<int> shotTaken_;
  QString logPath_;
  qint64 logPos_ = 0;
  QByteArray pending_;
  int filesSeen_ = 0;
  int logLines_ = 0;
  QElapsedTimer elapsed_;
};

// ---------------------------------------------------------------------------------

int main(int argc, char** argv)
{
  QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  QApplication app(argc, argv);

  Options opt;
  const QStringList args = app.arguments();
  for (int i = 1; i < args.size() - 1; ++i) {
    if (args[i] == "--inner")      opt.innerExe = args[i + 1];
    if (args[i] == "--totalfiles") opt.totalFiles = args[i + 1].toInt();
    if (args[i] == "--sizemb")     opt.sizeMb = args[i + 1].toInt();
    if (args[i] == "--version")    opt.version = args[i + 1];
    if (args[i] == "--shots")      opt.shotsDir = args[i + 1];
  }
  // The Inno bootstrap passes no --shots; the environment reaches through it, which is
  // how the packaged end-to-end chain is walked unattended.
  if (opt.shotsDir.isEmpty() && !qEnvironmentVariableIsEmpty("MVSETUP_SHOTS"))
    opt.shotsDir = QString::fromLocal8Bit(qgetenv("MVSETUP_SHOTS"));
  if (opt.totalFiles <= 0) opt.totalFiles = 1300;
  if (opt.sizeMb <= 0)     opt.sizeMb = 450;

  SetupWindow w(opt);
  w.show();
  return app.exec();
}
