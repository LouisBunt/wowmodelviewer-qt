#include "GameSource.h"

#include <atomic>

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QSettings>
#include <QStorageInfo>
#include <QThread>
#include <QTimer>

namespace gamesource {

const char* const kSettingsFile = "userSettings/qt-frontend.ini";
const char* const kProduct = "wow";

namespace {

const char* kSourceKey = "game/source";          // "local" | "online"; absent = pre-online ini
const char* kFolderKey = "game/installFolder";   // unchanged since 1.0 -- existing ini files keep working
const char* kRegionKey = "online/region";
const char* kLocaleKey = "online/locale";
const char* kCacheKey  = "online/cacheDir";      // empty = defaultCacheDir()
const char* kClearKey  = "online/clearOnNextStart";   // the cache folder to delete first thing

// Named after what it is for, and readable by whoever finds the folder in Explorer.
const char* kMarkerFile = "MODELVIEWER-CACHE.txt";

QString key(const char* k) { return QString::fromLatin1(k); }

QString labelFor(const std::vector<Choice>& list, const QString& code)
{
  for (const Choice& c : list)
    if (code == QLatin1String(c.code))
      return QString::fromUtf8(c.label);
  return code;
}

bool isAllDigits(const QString& s)
{
  if (s.isEmpty())
    return false;
  for (const QChar c : s)
    if (!c.isDigit())
      return false;
  return true;
}

}  // namespace

// --- tables ---------------------------------------------------------------------------------

const std::vector<Choice>& regions()
{
  static const std::vector<Choice> r = {
    { "eu", "Europa" },
    { "us", "Amerika" },
    { "kr", "Korea" },
    { "tw", "Taiwan" },
  };
  return r;
}

bool isRegion(const QString& code)
{
  return code == QLatin1String("cn") || labelFor(regions(), code) != code;
}

QString defaultRegion()
{
  switch (QLocale::system().country()) {
  case QLocale::UnitedStates: case QLocale::Canada: case QLocale::Mexico: case QLocale::Brazil:
  case QLocale::Argentina: case QLocale::Chile: case QLocale::Australia: case QLocale::NewZealand:
    return QStringLiteral("us");
  case QLocale::SouthKorea:
    return QStringLiteral("kr");
  case QLocale::Taiwan: case QLocale::HongKong: case QLocale::Macau:
    return QStringLiteral("tw");
  default:
    return QStringLiteral("eu");
  }
}

const std::vector<Choice>& dataLocales()
{
  static const std::vector<Choice> l = {
    { "deDE", "Deutsch" },
    { "enUS", "Englisch (USA)" },
    { "enGB", "Englisch (GB)" },
    { "frFR", "Französisch" },
    { "esES", "Spanisch (Spanien)" },
    { "esMX", "Spanisch (Mexiko)" },
    { "itIT", "Italienisch" },
    { "ptBR", "Portugiesisch (Brasilien)" },
    { "ruRU", "Russisch" },
    { "koKR", "Koreanisch" },
    { "zhTW", "Chinesisch (traditionell)" },
    { "zhCN", "Chinesisch (vereinfacht)" },
  };
  return l;
}

bool isDataLocale(const QString& code)
{
  return labelFor(dataLocales(), code) != code;
}

QString defaultLocale()
{
  // The display language, not QLocale::system() itself: on Windows that is the regional
  // format, and a German Windows set to US formats would get the names in English.
  return dataLocaleFor(QLocale::system().uiLanguages().value(0));
}

QString dataLocaleFor(const QString& uiLanguage)
{
  const QLocale l(uiLanguage);
  switch (l.language()) {
  case QLocale::German:     return QStringLiteral("deDE");
  case QLocale::French:     return QStringLiteral("frFR");
  case QLocale::Spanish:    return l.country() == QLocale::Spain ? QStringLiteral("esES")
                                                                  : QStringLiteral("esMX");
  case QLocale::Italian:    return QStringLiteral("itIT");
  case QLocale::Portuguese: return QStringLiteral("ptBR");
  case QLocale::Russian:    return QStringLiteral("ruRU");
  case QLocale::Korean:     return QStringLiteral("koKR");
  case QLocale::Chinese:    return l.script() == QLocale::TraditionalChineseScript
                                     ? QStringLiteral("zhTW") : QStringLiteral("zhCN");
  case QLocale::English:    return l.country() == QLocale::UnitedKingdom ? QStringLiteral("enGB")
                                                                          : QStringLiteral("enUS");
  default:                  return QStringLiteral("enUS");
  }
}

QString regionLabel(const QString& code)
{
  return code == QLatin1String("cn") ? QString::fromUtf8("China") : labelFor(regions(), code);
}

QString localeLabel(const QString& code) { return labelFor(dataLocales(), code); }

QString defaultCacheDir()
{
  return QCoreApplication::applicationDirPath() + QStringLiteral("/cdn-cache");
}

bool looksLikeWoWInstall(const QString& folder)
{
  return !folder.isEmpty() && QFile::exists(folder + "/.build.info");
}

// --- settings -------------------------------------------------------------------------------

GameSource load()
{
  QSettings s(QString::fromLatin1(kSettingsFile), QSettings::IniFormat);
  GameSource src;
  src.folder = s.value(key(kFolderKey)).toString();

  src.region = s.value(key(kRegionKey)).toString().toLower();
  if (!isRegion(src.region))
    src.region = defaultRegion();
  src.locale = s.value(key(kLocaleKey)).toString();
  if (!isDataLocale(src.locale))
    src.locale = defaultLocale();
  src.cacheDir = s.value(key(kCacheKey)).toString();
  if (src.cacheDir.isEmpty())
    src.cacheDir = defaultCacheDir();

  // No "game/source" key is every ini written before the online mode existed: those people
  // chose a folder, and a valid folder is what they keep getting.
  const QString kind = s.value(key(kSourceKey)).toString();
  if (kind == QLatin1String("online"))
    src.kind = GameSource::Online;
  else if (looksLikeWoWInstall(src.folder))
    src.kind = GameSource::Local;
  else
    src.kind = GameSource::None;
  return src;
}

void save(const GameSource& src)
{
  QDir().mkpath("userSettings");
  QSettings s(QString::fromLatin1(kSettingsFile), QSettings::IniFormat);
  switch (src.kind) {
  case GameSource::Local:
    s.setValue(key(kSourceKey), QStringLiteral("local"));
    s.setValue(key(kFolderKey), src.folder);
    break;
  case GameSource::Online:
    s.setValue(key(kSourceKey), QStringLiteral("online"));
    s.setValue(key(kRegionKey), src.region);
    s.setValue(key(kLocaleKey), src.locale);
    // The default is stored as empty, so a moved installation keeps finding its own cache.
    s.setValue(key(kCacheKey), QDir::cleanPath(src.cacheDir) == QDir::cleanPath(defaultCacheDir())
                                 ? QString() : QDir::cleanPath(src.cacheDir));
    break;
  case GameSource::None:
    s.remove(key(kSourceKey));
    break;
  }
  s.sync();
}

bool sameSource(const GameSource& a, const GameSource& b)
{
  if (a.kind != b.kind)
    return false;
  if (a.kind == GameSource::Local)
    return QDir::cleanPath(a.folder).compare(QDir::cleanPath(b.folder), Qt::CaseInsensitive) == 0;
  if (a.kind == GameSource::Online)
    return a.region == b.region && a.locale == b.locale
           && QDir::cleanPath(a.cacheDir).compare(QDir::cleanPath(b.cacheDir), Qt::CaseInsensitive) == 0;
  return true;
}

QString describe(const GameSource& src)
{
  switch (src.kind) {
  case GameSource::Local:
    return QStringLiteral("local · ") + QDir::toNativeSeparators(src.folder);
  case GameSource::Online:
    return QStringLiteral("online · %1 · %2 · %3%4")
        .arg(src.region, src.locale, QDir::toNativeSeparators(src.cacheDir),
             src.offline ? QStringLiteral(" · offline") : QString());
  case GameSource::None:
    break;
  }
  return QStringLiteral("none");
}

QString installFolder()
{
  QSettings s(QString::fromLatin1(kSettingsFile), QSettings::IniFormat);
  const QString folder = s.value(key(kFolderKey)).toString();
  return looksLikeWoWInstall(folder) ? folder : QString();
}

void setInstallFolder(const QString& folder)
{
  QDir().mkpath("userSettings");
  QSettings s(QString::fromLatin1(kSettingsFile), QSettings::IniFormat);
  s.setValue(key(kFolderKey), folder);
  s.sync();
}

QString pendingClear()
{
  QSettings s(QString::fromLatin1(kSettingsFile), QSettings::IniFormat);
  return s.value(key(kClearKey)).toString();
}

void setPendingClear(const QString& dir)
{
  QDir().mkpath("userSettings");
  QSettings s(QString::fromLatin1(kSettingsFile), QSettings::IniFormat);
  if (dir.isEmpty())
    s.remove(key(kClearKey));
  else
    s.setValue(key(kClearKey), QDir::cleanPath(dir));
  s.sync();
}

// --- command line -----------------------------------------------------------------------------

Arguments parseArguments(const QStringList& args)
{
  Arguments a;

  QStringList positional;
  for (const QString& arg : args) {
    if (arg.startsWith("--"))
      break;
    positional << arg;
  }
  if (!positional.isEmpty()) {
    // All digits and not a folder that exists: the model, with the folder left to the settings.
    // A WoW folder named "917116" still works -- it exists, so it is taken as the folder.
    const bool firstIsModel = isAllDigits(positional.first()) && !QDir(positional.first()).exists();
    if (firstIsModel)
      positional.prepend(QString());
    a.folder = positional.value(0);
    const QString id = positional.value(1);
    if (!id.isEmpty()) {
      bool ok = false;
      a.modelId = id.toUInt(&ok);
      if (!ok)
        a.notes << QString("'%1' ist keine FileDataID -- ignoriert").arg(id);
    }
  }

  for (int i = 0; i < args.size(); ++i) {
    const QString arg = args.at(i);
    const QString next = i + 1 < args.size() ? args.at(i + 1) : QString();
    const bool hasValue = !next.isEmpty() && !next.startsWith("--");
    if (arg == QLatin1String("--online")) {
      a.online = true;
      if (hasValue) {
        ++i;
        if (isRegion(next.toLower()))
          a.region = next.toLower();
        else
          a.notes << QString("--online: Region '%1' gibt es nicht (eu, us, kr, tw, cn) -- "
                             "die gespeicherte gilt").arg(next);
      }
    } else if (arg == QLatin1String("--locale")) {
      if (!hasValue) {
        a.notes << QStringLiteral("--locale ohne Wert -- ignoriert");
        continue;
      }
      ++i;
      if (isDataLocale(next))
        a.locale = next;
      else
        a.notes << QString("--locale: '%1' ist keine Spielsprache (deDE, enUS, frFR, ...) -- "
                           "ignoriert").arg(next);
    } else if (arg == QLatin1String("--cache")) {
      if (!hasValue) {
        a.notes << QStringLiteral("--cache ohne Ordner -- ignoriert");
        continue;
      }
      ++i;
      a.cacheDir = QDir::cleanPath(QDir::fromNativeSeparators(next));
    }
  }

  if (a.online && !a.folder.isEmpty())
    a.notes << QString("--online: der Ordner '%1' wird nicht benutzt").arg(a.folder);
  return a;
}

GameSource applyArguments(GameSource src, const Arguments& a)
{
  if (a.online) {
    src.kind = GameSource::Online;
  } else if (!a.folder.isEmpty()) {
    src.kind = GameSource::Local;
    src.folder = a.folder;
  }
  // Online settings apply to an online run, whichever way it became one.
  if (!a.region.isEmpty())
    src.region = a.region;
  if (!a.locale.isEmpty())
    src.locale = a.locale;
  if (!a.cacheDir.isEmpty())
    src.cacheDir = QDir(a.cacheDir).absolutePath();
  return src;
}

// --- cache folder ---------------------------------------------------------------------------

bool prepareCache(const QString& dir, QString* error)
{
  const QString problem = cacheDirProblem(dir);
  if (!problem.isEmpty()) {
    *error = problem;
    return false;
  }
  if (!QDir().mkpath(dir)) {
    *error = QObject::tr("Der Cache-Ordner ließ sich nicht anlegen:\n%1")
               .arg(QDir::toNativeSeparators(dir));
    return false;
  }
  // Written every start, not only when missing: it doubles as the write test.
  QFile f(QDir(dir).filePath(QString::fromLatin1(kMarkerFile)));
  if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
    *error = QObject::tr("In den Cache-Ordner kann nicht geschrieben werden:\n%1")
               .arg(QDir::toNativeSeparators(dir));
    return false;
  }
  f.write(QString::fromUtf8(
    "Online-Cache von ModelViewer: Midnight -- Spieldateien von Blizzards Download-Servern.\n"
    "Darf gelöscht werden; fehlende Dateien werden beim nächsten Online-Start neu geladen.\n")
            .toUtf8());
  return true;
}

bool isOurCache(const QString& dir)
{
  return QFile::exists(QDir(dir).filePath(QString::fromLatin1(kMarkerFile)));
}

QString cacheDirProblem(const QString& dir)
{
  if (dir.isEmpty())
    return QObject::tr("Kein Cache-Ordner angegeben.");
  // CascLib looks for its build files in the folder it is given and then in every parent
  // (CheckCascBuildFileDirs). Next to a .build.info that finds the installation, not the cache.
  // Walked by name, not with QDir::cdUp(), which refuses to leave a folder that does not exist
  // yet -- and a cache folder about to be created is exactly what gets checked here.
  for (QString path = QDir::cleanPath(QDir(dir).absolutePath());;) {
    if (QFile::exists(path + QStringLiteral("/.build.info")))
      return QObject::tr("Der Cache darf nicht in einer WoW-Installation liegen (%1) — dort "
                         "würde das Programm die Installation statt des Caches öffnen.")
               .arg(QDir::toNativeSeparators(path));
    const QString parent = QFileInfo(path).path();
    if (parent == path)
      break;
    path = parent;
  }
  return QString();
}

QString cacheDirFor(const QString& picked)
{
  const QDir d(picked);
  if (isOurCache(picked) || d.isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden))
    return QDir::cleanPath(picked);
  return QDir::cleanPath(d.filePath(QStringLiteral("ModelViewer-Cache")));
}

QString completedBuild(const QString& dir)
{
  QFile f(QDir(dir).filePath(QStringLiteral("complete-build.txt")));
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    return QString();
  return QString::fromUtf8(f.readAll()).trimmed();
}

void setCompletedBuild(const QString& dir, const QString& build)
{
  QFile f(QDir(dir).filePath(QStringLiteral("complete-build.txt")));
  if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    f.write(build.toUtf8() + "\n");
}

bool clearCache(const QString& dir, QString* error)
{
  if (!QDir(dir).exists())
    return true;
  if (!isOurCache(dir)) {
    *error = QObject::tr("»%1« ist kein Cache-Ordner dieses Programms (die Datei %2 fehlt) — "
                         "gelöscht wird dort nichts.")
               .arg(QDir::toNativeSeparators(dir), QString::fromLatin1(kMarkerFile));
    return false;
  }
  if (!QDir(dir).removeRecursively()) {
    *error = QObject::tr("Der Cache ließ sich nicht vollständig löschen. Läuft ModelViewer "
                         "noch ein zweites Mal?\n%1").arg(QDir::toNativeSeparators(dir));
    return false;
  }
  return true;
}

quint64 measureCache(const QString& dir, int* files)
{
  quint64 total = 0;
  int n = 0;
  QDirIterator it(dir, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    it.next();
    total += (quint64)it.fileInfo().size();
    ++n;
  }
  if (files)
    *files = n;
  return total;
}

qint64 freeBytes(const QString& dir)
{
  // QStorageInfo needs a path that exists; a cache not created yet is measured at the nearest
  // parent that does (by name -- QDir::cdUp() will not leave a folder that does not exist).
  QString path = QDir::cleanPath(QDir(dir).absolutePath());
  while (!QFileInfo::exists(path)) {
    const QString parent = QFileInfo(path).path();
    if (parent == path)
      return -1;
    path = parent;
  }
  const QStorageInfo info(path);
  return info.isValid() ? info.bytesAvailable() : -1;
}

QString formatBytes(quint64 bytes)
{
  const QLocale de(QLocale::German);
  const double kb = 1024.0, mb = kb * 1024.0, gb = mb * 1024.0;
  if (bytes >= (quint64)gb)
    return de.toString(bytes / gb, 'f', 2) + QStringLiteral(" GB");
  if (bytes >= (quint64)mb)
    return de.toString(bytes / mb, 'f', bytes >= 100 * mb ? 0 : 1) + QStringLiteral(" MB");
  if (bytes >= (quint64)kb)
    return de.toString(qRound(bytes / kb)) + QStringLiteral(" KB");
  return de.toString((qulonglong)bytes) + QStringLiteral(" Byte");
}

// --- worker ---------------------------------------------------------------------------------

bool runWhileResponsive(const std::function<bool()>& job, const std::function<void()>& tick,
                        int tickMs)
{
  std::atomic<bool> result(false);
  QThread* worker = QThread::create([&job, &result]() { result = job(); });

  QEventLoop loop;
  QTimer timer;
  QObject::connect(&timer, &QTimer::timeout, &timer, [&tick]() { tick(); });
  // Queued across threads: a job that finishes before exec() still ends the loop, because the
  // quit waits in this thread's queue until the loop runs.
  QObject::connect(worker, &QThread::finished, &loop, &QEventLoop::quit);
  timer.start(tickMs);
  worker->start();
  if (!worker->isFinished())
    loop.exec();
  worker->wait();
  timer.stop();
  tick();
  delete worker;
  return result;
}

}  // namespace gamesource
