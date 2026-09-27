#include "SourceDialog.h"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QThread>
#include <QVBoxLayout>

#include "Theme.h"
#include "UiKit.h"

namespace {

QLabel* hint(const QString& text)
{
  auto* l = new QLabel(text);
  l->setFont(typo::font(typo::Small));
  l->setProperty("role", "hint");
  l->setWordWrap(true);
  return l;
}

ElidedLabel* pathLabel()
{
  auto* l = new ElidedLabel;
  l->setElideMode(Qt::ElideMiddle);   // drive and last folder survive, the middle goes
  l->setFont(typo::font(typo::Mono));
  l->setProperty("role", "mono");
  return l;
}

QPushButton* button(const QString& text, const char* variant = nullptr)
{
  auto* b = new QPushButton(text);
  if (variant)
    b->setProperty("variant", variant);
  b->setCursor(Qt::PointingHandCursor);
  b->setAutoDefault(false);            // Enter must not start a download by accident
  return b;
}

}  // namespace

QString SourceDialog::askForWoWFolder(QWidget* parent, const QString& tried)
{
  QString start = tried;
  for (;;) {
    const QString picked = QFileDialog::getExistingDirectory(
      parent, QString::fromUtf8("WoW-Installationsordner wählen"), start,
      QFileDialog::ShowDirsOnly);
    if (picked.isEmpty())
      return QString();
    if (gamesource::looksLikeWoWInstall(picked))
      return QDir::cleanPath(picked);

    // Naming the file we looked for beats "invalid folder": it tells the user both what is
    // wrong and that the Data subfolder is not what we want.
    if (QMessageBox::warning(
          parent, QString::fromUtf8("Keine WoW-Installation"),
          QString::fromUtf8("In\n\n%1\n\nliegt keine .build.info. Bitte den Ordner wählen, in "
                            "dem WoW installiert ist -- nicht den Data-Unterordner.")
            .arg(QDir::toNativeSeparators(picked)),
          QMessageBox::Retry | QMessageBox::Cancel, QMessageBox::Retry) == QMessageBox::Cancel)
      return QString();
    start = picked;
  }
}

SourceDialog::SourceDialog(Mode mode, const GameSource& current, bool cacheInUse,
                           QWidget* parent)
  : QDialog(parent), mode_(mode), current_(current), cacheDir_(QDir::cleanPath(current.cacheDir))
{
  setWindowTitle(QString::fromUtf8("Spieldaten"));
  // Fixed width, height from the content: the hints wrap, and a dialog whose width floats lets
  // Qt size it from the unwrapped text and squeeze the groups on top of each other.
  setFixedWidth(ui::px(560));
  if (cacheInUse)
    inUseDir_ = cacheDir_;

  auto* col = new QVBoxLayout(this);
  col->setContentsMargins(met::sp(met::Section), met::sp(met::Section),
                          met::sp(met::Section), met::sp(met::Edge));
  col->setSpacing(met::sp(met::Gap));

  auto* title = new QLabel(mode == FirstRun
                             ? QString::fromUtf8("Woher sollen die Spieldaten kommen?")
                             : QString::fromUtf8("Spieldaten-Quelle"));
  title->setFont(typo::font(typo::Title));
  title->setProperty("role", "title");
  col->addWidget(title);

  QString introText;
  if (mode == FirstRun) {
    introText = QString::fromUtf8(
      "ModelViewer braucht die Modelle, Texturen und die Datenbank aus World of Warcraft. "
      "Auf diesem Rechner wurde keine Installation gefunden — die Daten lassen sich aber "
      "auch direkt von Blizzard laden.");
    if (!current.folder.isEmpty())
      introText += QString::fromUtf8("\n\nIm zuletzt benutzten Ordner %1 liegt keine "
                                     "WoW-Installation mehr.")
                     .arg(QDir::toNativeSeparators(current.folder));
  } else {
    const QString now = current.kind == GameSource::Online
      ? QString::fromUtf8("online (%1, %2)%3")
          .arg(gamesource::regionLabel(current.region), gamesource::localeLabel(current.locale),
               current.offline ? QString::fromUtf8(", zurzeit offline") : QString())
      : QString::fromUtf8("die WoW-Installation in %1").arg(QDir::toNativeSeparators(current.folder));
    introText = QString::fromUtf8("In Benutzung: %1. Eine Änderung gilt ab dem nächsten Start "
                                  "— die Spieldaten werden nur beim Programmstart eingebunden.")
                  .arg(now);
  }
  auto* intro = new QLabel(introText);
  intro->setFont(typo::font(typo::Body));
  intro->setWordWrap(true);
  col->addWidget(intro);
  col->addSpacing(met::sp(met::Snug));

  // --- a WoW installation --------------------------------------------------------------
  col->addWidget(uikit::sectionLabel(QString::fromUtf8("WOW-INSTALLATION AUF DIESEM RECHNER")));
  col->addWidget(hint(QString::fromUtf8(
    "Am schnellsten, ohne Download. Gewählt wird der Ordner »World of Warcraft« mit der "
    "Datei .build.info — nicht »_retail_« und nicht »Data«.")));
  {
    auto* row = new QHBoxLayout;
    row->setSpacing(met::sp(met::Gap));
    folderLabel_ = pathLabel();
    const QString known = gamesource::looksLikeWoWInstall(current.folder) ? current.folder
                                                                          : QString();
    folderLabel_->setFullText(known.isEmpty() ? QString::fromUtf8("kein Ordner gewählt")
                                              : QDir::toNativeSeparators(known));
    row->addWidget(folderLabel_, 1);
    auto* pick = button(QString::fromUtf8("WoW-Ordner wählen …"));
    connect(pick, &QPushButton::clicked, this, &SourceDialog::chooseLocal);
    row->addWidget(pick);
    col->addLayout(row);
  }

  col->addSpacing(met::sp(met::Gap));
  col->addWidget(uikit::hairline());
  col->addSpacing(met::sp(met::Snug));

  // --- online --------------------------------------------------------------------------
  col->addWidget(uikit::sectionLabel(QString::fromUtf8("ONLINE — OHNE WOW-INSTALLATION")));
  col->addWidget(hint(QString::fromUtf8(
    "Die Dateien kommen von Blizzards öffentlichen Download-Servern, wie bei wow.export. Der "
    "erste Start lädt einmalig rund %1, jede neue WoW-Version etwa %2; sonst nur, was du neu "
    "ansiehst. Alles Geladene bleibt im Cache und geht auch ohne Internet.")
      .arg(gamesource::formatBytes(gamesource::kFirstStartBytes),
           gamesource::formatBytes(gamesource::kPatchBytes))));

  const QString lRegion = QString::fromUtf8("Region");
  const QString lLocale = QString::fromUtf8("Sprache");
  const QString lCache  = QString::fromUtf8("Cache");
  const int column = PropertyRow::measureColumn({lRegion, lLocale, lCache});

  region_ = uikit::wideCombo();
  for (const auto& r : gamesource::regions())
    region_->addItem(QString::fromUtf8(r.label), QString::fromLatin1(r.code));
  // China is not offered (see gamesource::regions) -- but a region that came from the command
  // line or a hand-edited ini is shown rather than silently replaced.
  if (region_->findData(current.region) < 0 && gamesource::isRegion(current.region))
    region_->addItem(gamesource::regionLabel(current.region), current.region);
  region_->setCurrentIndex(qMax(0, region_->findData(current.region)));
  region_->setToolTip(QString::fromUtf8(
    "Welche Download-Server und welcher Spielstand benutzt werden. Die Dateien sind überall "
    "dieselben; nur am Patchtag kann eine Region kurz vorn liegen."));
  col->addWidget(new PropertyRow(lRegion, region_, column));

  locale_ = uikit::wideCombo();
  for (const auto& l : gamesource::dataLocales())
    locale_->addItem(QString::fromUtf8(l.label), QString::fromLatin1(l.code));
  locale_->setCurrentIndex(qMax(0, locale_->findData(current.locale)));
  locale_->setToolTip(QString::fromUtf8(
    "Sprache der Namen aus den Spieldaten — Items, NPCs, Anpassungen. Die Oberfläche bleibt "
    "deutsch. Nach einem Wechsel wird die Datenbank einmal neu aufgebaut."));
  col->addWidget(new PropertyRow(lLocale, locale_, column));

  {
    auto* cacheBox = new QWidget;
    auto* cacheCol = new QVBoxLayout(cacheBox);
    cacheCol->setContentsMargins(0, 0, 0, 0);
    cacheCol->setSpacing(met::sp(met::Tight));
    cacheLabel_ = pathLabel();
    cacheLabel_->setFullText(QDir::toNativeSeparators(cacheDir_));
    cacheCol->addWidget(cacheLabel_);
    auto* cacheRow = new QHBoxLayout;
    cacheRow->setSpacing(met::sp(met::Gap));
    cacheSize_ = new ElidedLabel;
    cacheSize_->setFont(typo::font(typo::Mono));
    cacheSize_->setProperty("role", "mono");
    cacheRow->addWidget(cacheSize_, 1);
    auto* move = button(QString::fromUtf8("Ordner …"));
    connect(move, &QPushButton::clicked, this, &SourceDialog::pickCacheDir);
    cacheRow->addWidget(move);
    clearBtn_ = button(QString::fromUtf8("Leeren"), "danger");
    connect(clearBtn_, &QPushButton::clicked, this, &SourceDialog::clearCacheClicked);
    cacheRow->addWidget(clearBtn_);
    cacheCol->addLayout(cacheRow);
    auto* cacheProp = new PropertyRow(lCache, cacheBox, column);
    // Level with the path, the first of the two lines, rather than floating between them.
    cacheProp->label()->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    col->addWidget(cacheProp);
  }

  {
    auto* row = new QHBoxLayout;
    row->addStretch(1);
    QString label = QString::fromUtf8("Online laden");
    if (mode == Switch)
      label = current.kind == GameSource::Online ? QString::fromUtf8("Übernehmen")
                                                 : QString::fromUtf8("Online verwenden");
    onlineBtn_ = button(label, "primary");
    onlineBtn_->setMinimumHeight(met::hCta());
    connect(onlineBtn_, &QPushButton::clicked, this, &SourceDialog::chooseOnline);
    row->addWidget(onlineBtn_);
    col->addLayout(row);
  }
  connect(region_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &SourceDialog::updateOnlineButton);
  connect(locale_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &SourceDialog::updateOnlineButton);

  col->addSpacing(met::sp(met::Snug));
  col->addWidget(uikit::hairline());

  // --- the way out ---------------------------------------------------------------------------
  auto* buttons = new QHBoxLayout;
  buttons->addStretch(1);
  auto* close = button(mode == FirstRun ? QString::fromUtf8("Beenden")
                                        : QString::fromUtf8("Schließen"), "quiet");
  connect(close, &QPushButton::clicked, this, &QDialog::reject);
  buttons->addWidget(close);
  col->addLayout(buttons);

  updateOnlineButton();
  measureCache();

  // The wrapped hints only know their height once the width is known. Qt sizes a top-level
  // dialog from the unwrapped text and then squeezes the rows on top of each other -- the
  // combo boxes ended up overlapping. Ask the layout for the real height at the fixed width.
  col->activate();
  setMinimumHeight(col->heightForWidth(width()));
}

GameSource SourceDialog::onlineChoice() const
{
  GameSource out = current_;
  out.kind = GameSource::Online;
  out.region = region_->currentData().toString();
  out.locale = locale_->currentData().toString();
  out.cacheDir = cacheDir_;
  out.offline = false;
  return out;
}

void SourceDialog::chooseLocal()
{
  const QString picked = askForWoWFolder(this, current_.folder);
  if (picked.isEmpty())
    return;
  // Only the kind and the folder change. Region, language and cache stay as they were, so
  // switching back to online later finds its settings -- and its cache -- again.
  chosen_ = current_;
  chosen_.kind = GameSource::Local;
  chosen_.folder = picked;
  chosen_.offline = false;
  accept();
}

void SourceDialog::chooseOnline()
{
  const GameSource next = onlineChoice();
  const QString problem = gamesource::cacheDirProblem(next.cacheDir);
  if (!problem.isEmpty()) {
    QMessageBox::warning(this, QString::fromUtf8("Online-Cache"), problem);
    return;
  }
  // A first download is several hundred MB. Say so while there is still a choice, not after
  // the disk ran full halfway through. Only from the menu: on the first start main() asks the
  // same question a moment later, for every way a start can become online.
  const qint64 free = gamesource::freeBytes(next.cacheDir);
  if (mode_ == Switch && free >= 0 && free < gamesource::kComfortBytes
      && gamesource::completedBuild(next.cacheDir).isEmpty()) {
    if (QMessageBox::warning(
          this, QString::fromUtf8("Wenig Platz"),
          QString::fromUtf8("Auf dem Laufwerk des Caches sind nur noch %1 frei. Der erste "
                            "Online-Start braucht rund %2, jede neue WoW-Version rund %3 mehr.")
            .arg(gamesource::formatBytes((quint64)free),
                 gamesource::formatBytes(gamesource::kFirstStartBytes),
                 gamesource::formatBytes(gamesource::kPatchBytes)),
          QMessageBox::Ignore | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Ignore)
      return;
  }
  chosen_ = next;
  accept();
}

void SourceDialog::updateOnlineButton()
{
  // "Übernehmen" with nothing changed would restart for nothing.
  if (mode_ == Switch && current_.kind == GameSource::Online)
    onlineBtn_->setEnabled(!gamesource::sameSource(onlineChoice(), current_));
}

void SourceDialog::pickCacheDir()
{
  const QString picked = QFileDialog::getExistingDirectory(
    this, QString::fromUtf8("Ordner für den Online-Cache"), cacheDir_,
    QFileDialog::ShowDirsOnly);
  if (picked.isEmpty())
    return;
  // A non-empty folder gets a subfolder of its own: "Leeren" deletes the cache folder
  // wholesale, and whatever else lived in the picked folder must not go with it.
  const QString dir = gamesource::cacheDirFor(picked);
  const QString problem = gamesource::cacheDirProblem(dir);
  if (!problem.isEmpty()) {
    QMessageBox::warning(this, QString::fromUtf8("Online-Cache"), problem);
    return;
  }
  cacheDir_ = dir;
  cacheLabel_->setFullText(QDir::toNativeSeparators(cacheDir_));
  measureCache();
  updateOnlineButton();
}

void SourceDialog::clearCacheClicked()
{
  const QString dir = cacheDir_;
  if (QMessageBox::question(
        this, QString::fromUtf8("Online-Cache leeren"),
        QString::fromUtf8("Alle heruntergeladenen Spieldateien in\n\n%1\n\nlöschen? Der nächste "
                          "Online-Start lädt dann wieder rund %2.")
          .arg(QDir::toNativeSeparators(dir),
               gamesource::formatBytes(gamesource::kFirstStartBytes)),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
    return;

  // CascLib keeps files of an open storage open; deleting under it fails halfway on Windows
  // and leaves a cache that is neither whole nor gone. The next start deletes it before
  // anything opens it.
  if (!inUseDir_.isEmpty() && dir == inUseDir_) {
    gamesource::setPendingClear(dir);
    measureCache();
    return;
  }
  QString err;
  if (!gamesource::clearCache(dir, &err))
    QMessageBox::warning(this, QString::fromUtf8("Online-Cache leeren"), err);
  measureCache();
}

void SourceDialog::measureCache()
{
  const QString dir = cacheDir_;
  clearBtn_->setEnabled(false);
  cacheSize_->setProperty("state", QVariant());
  Theme::repolish(cacheSize_);

  if (QDir::cleanPath(gamesource::pendingClear()) == dir) {
    cacheSize_->setFullText(QString::fromUtf8("wird beim nächsten Start geleert"));
    return;
  }
  if (!QDir(dir).exists()) {
    showCacheSize(dir, 0, 0);
    return;
  }
  cacheSize_->setFullText(QString::fromUtf8("wird gemessen …"));

  // A cache that has been used for a while is tens of thousands of files; walking them on the
  // UI thread froze the dialog for seconds. The result comes back queued, and only if the
  // dialog still exists and still shows the folder it was measured for.
  QPointer<SourceDialog> self(this);
  QThread* t = QThread::create([self, dir]() {
    int files = 0;
    const quint64 bytes = gamesource::measureCache(dir, &files);
    QMetaObject::invokeMethod(qApp, [self, dir, bytes, files]() {
      if (self)
        self->showCacheSize(dir, bytes, files);
    }, Qt::QueuedConnection);
  });
  connect(t, &QThread::finished, t, &QObject::deleteLater);
  t->start(QThread::LowPriority);
}

void SourceDialog::showCacheSize(const QString& dir, quint64 bytes, int files)
{
  if (dir != cacheDir_)
    return;                        // the folder was changed while this one was measured
  const qint64 free = gamesource::freeBytes(dir);
  const QString freeText = free < 0 ? QString()
                                    : QString::fromUtf8(" · %1 frei")
                                        .arg(gamesource::formatBytes((quint64)free));
  // The marker file alone does not count as content.
  const bool empty = files <= 1;
  cacheSize_->setFullText((empty ? QString::fromUtf8("leer") : gamesource::formatBytes(bytes))
                          + freeText);
  cacheSize_->setTooltipSuffix(empty ? QString()
                                     : QString::fromUtf8("%1 Dateien")
                                         .arg(QLocale(QLocale::German).toString(files)));
  if (free >= 0 && free < gamesource::kComfortBytes) {
    cacheSize_->setProperty("state", QStringLiteral("warn"));
    Theme::repolish(cacheSize_);
  }
  clearBtn_->setEnabled(!empty && gamesource::isOurCache(dir));
}
