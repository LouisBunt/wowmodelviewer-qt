// Tests for the data-source layer: the settings a start reads, the command line that
// overrides them, and the cache folder the online mode deletes on request.
//
// Every one of these fails silently in the application. A mis-read ini starts the wrong
// source without a word; a command line that takes a model id for a folder ends a scripted
// run with "keine WoW-Installation"; and a "Leeren" that does not insist on its marker file
// deletes whatever folder it was pointed at.
//
// QtCore only: no network, no game data, no display.
//
//   sourcetest
//
// Exit code = number of failing checks, so a plain shell/CI check works.
#include <cstdio>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QThread>

#include "GameSource.h"

namespace {

int failures = 0;

#define CHECK(cond)                                                        \
  do {                                                                     \
    if (!(cond)) {                                                         \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
      failures++;                                                          \
    }                                                                      \
  } while (0)

void touch(const QString& path)
{
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile f(path);
  f.open(QIODevice::WriteOnly);
  f.write("x");
}

void writeIni(const QString& text)
{
  QDir().mkpath("userSettings");
  QFile::remove(QString::fromLatin1(gamesource::kSettingsFile));
  QFile f(QString::fromLatin1(gamesource::kSettingsFile));
  f.open(QIODevice::WriteOnly | QIODevice::Text);
  f.write(text.toUtf8());
}

// --- settings ---------------------------------------------------------------------------------
void testSettings(const QString& root)
{
  // A machine that never ran the program: nothing is known, the caller has to ask.
  QFile::remove(QString::fromLatin1(gamesource::kSettingsFile));
  CHECK(gamesource::load().kind == GameSource::None);
  CHECK(gamesource::isRegion(gamesource::load().region));
  CHECK(gamesource::isDataLocale(gamesource::load().locale));

  // An ini from before the online mode: only game/installFolder. It must keep meaning "local"
  // -- updating the program must not turn a working setup into a question.
  const QString wow = root + "/World of Warcraft";
  touch(wow + "/.build.info");
  writeIni(QString("[game]\ninstallFolder=%1\n").arg(wow));
  GameSource old = gamesource::load();
  CHECK(old.kind == GameSource::Local);
  CHECK(old.folder == wow);
  CHECK(gamesource::installFolder() == wow);

  // The remembered folder is gone (WoW uninstalled, drive not mounted): None, not Local with a
  // dead path, so the start asks instead of failing -- and the folder is still there to show.
  writeIni(QString("[game]\ninstallFolder=%1\n").arg(root + "/Gone"));
  old = gamesource::load();
  CHECK(old.kind == GameSource::None);
  CHECK(old.folder == root + "/Gone");
  CHECK(gamesource::installFolder().isEmpty());

  // Round trip of an online source, and the WoW folder surviving next to it: someone online
  // with WoW on another drive still installs the MVLink addon there.
  writeIni(QString("[game]\ninstallFolder=%1\n").arg(wow));
  GameSource on;
  on.kind = GameSource::Online;
  on.region = "kr";
  on.locale = "koKR";
  on.cacheDir = root + "/cache";
  gamesource::save(on);
  const GameSource back = gamesource::load();
  CHECK(back.kind == GameSource::Online);
  CHECK(back.region == "kr");
  CHECK(back.locale == "koKR");
  CHECK(back.cacheDir == root + "/cache");
  CHECK(gamesource::installFolder() == wow);
  CHECK(gamesource::sameSource(back, on));

  // The default cache is stored as empty, so a moved installation keeps finding its own.
  on.cacheDir = gamesource::defaultCacheDir();
  gamesource::save(on);
  {
    QFile f(QString::fromLatin1(gamesource::kSettingsFile));
    f.open(QIODevice::ReadOnly | QIODevice::Text);
    CHECK(!QString::fromUtf8(f.readAll()).contains(gamesource::defaultCacheDir()));
  }
  CHECK(gamesource::load().cacheDir == gamesource::defaultCacheDir());

  // Garbage in the ini falls back to a valid default instead of reaching the engine, which
  // would refuse the locale and the open with it.
  writeIni("[game]\nsource=online\n[online]\nregion=moon\nlocale=de_DE\n");
  const GameSource bad = gamesource::load();
  CHECK(bad.kind == GameSource::Online);
  CHECK(gamesource::isRegion(bad.region));
  CHECK(gamesource::isDataLocale(bad.locale));

  // Scheduled clear: set, read, removed.
  gamesource::setPendingClear(root + "/cache/");
  CHECK(gamesource::pendingClear() == QDir::cleanPath(root + "/cache"));
  gamesource::setPendingClear(QString());
  CHECK(gamesource::pendingClear().isEmpty());
}

// --- the command line -------------------------------------------------------------------------
void testArguments(const QString& root)
{
  using gamesource::parseArguments;

  // The documented form: folder, then model.
  auto a = parseArguments({"D:/World of Warcraft", "917116", "--shot", "x.png"});
  CHECK(a.folder == "D:/World of Warcraft");
  CHECK(a.modelId == 917116u);
  CHECK(!a.online);

  // A model alone. This used to be read as a WoW folder named "917116" and ended in
  // "keine WoW-Installation" -- the 3D-print example in LIESMICH.txt does exactly this.
  a = parseArguments({"917116", "--print-height", "180", "--export", "STL,C:/figur.stl"});
  CHECK(a.folder.isEmpty());
  CHECK(a.modelId == 917116u);

  // ... unless a folder of that name really exists.
  QDir().mkpath(root + "/12345");
  const QString cwd = QDir::currentPath();
  QDir::setCurrent(root);
  a = parseArguments({"12345"});
  CHECK(a.folder == "12345");
  CHECK(a.modelId == 0u);
  QDir::setCurrent(cwd);

  // Flags are never positional.
  a = parseArguments({"--shot", "x.png"});
  CHECK(a.folder.isEmpty() && a.modelId == 0u);

  // --online with and without a region; --locale and --cache.
  a = parseArguments({"--online"});
  CHECK(a.online && a.region.isEmpty());
  a = parseArguments({"917116", "--online", "EU", "--locale", "enUS", "--cache", root + "/c"});
  CHECK(a.online);
  CHECK(a.region == "eu");
  CHECK(a.locale == "enUS");
  CHECK(a.cacheDir == QDir::cleanPath(root + "/c"));
  CHECK(a.modelId == 917116u);
  CHECK(a.notes.isEmpty());

  // China: not in the dialog, accepted here.
  a = parseArguments({"--online", "cn"});
  CHECK(a.region == "cn");

  // Wrong values are reported and ignored, never passed on.
  a = parseArguments({"--online", "de", "--locale", "de_DE"});
  CHECK(a.online);
  CHECK(a.region.isEmpty());
  CHECK(a.locale.isEmpty());
  CHECK(a.notes.size() == 2);

  // --online followed directly by another flag takes no value from it.
  a = parseArguments({"--online", "--shot", "x.png"});
  CHECK(a.online && a.region.isEmpty() && a.notes.isEmpty());

  // A folder and --online: the flag wins, and the trace says the folder is unused.
  a = parseArguments({"D:/World of Warcraft", "--online"});
  CHECK(a.online);
  CHECK(!a.notes.isEmpty());

  // Applying: the command line wins for this run and changes nothing on disk.
  GameSource saved;
  saved.kind = GameSource::Local;
  saved.folder = "D:/WoW";
  saved.region = "eu";
  saved.locale = "deDE";
  saved.cacheDir = root + "/cache";
  GameSource run = gamesource::applyArguments(saved, parseArguments({"--online", "us"}));
  CHECK(run.kind == GameSource::Online);
  CHECK(run.region == "us");
  CHECK(run.locale == "deDE");
  run = gamesource::applyArguments(saved, parseArguments({"E:/Other WoW"}));
  CHECK(run.kind == GameSource::Local && run.folder == "E:/Other WoW");
  run = gamesource::applyArguments(saved, parseArguments({"917116"}));
  CHECK(run.kind == GameSource::Local && run.folder == "D:/WoW");
}

// --- the cache folder -------------------------------------------------------------------------
void testCache(const QString& root)
{
  QString err;

  // Not ours: no marker, nothing deleted.
  const QString foreign = root + "/Documents";
  touch(foreign + "/important.txt");
  CHECK(!gamesource::clearCache(foreign, &err));
  CHECK(!err.isEmpty());
  CHECK(QFile::exists(foreign + "/important.txt"));
  // Picked as the cache location, it gets a subfolder instead of being taken over.
  CHECK(gamesource::cacheDirFor(foreign) == QDir::cleanPath(foreign + "/ModelViewer-Cache"));

  // Inside a WoW installation, CascLib would find the installation's .build.info first.
  const QString wow = root + "/World of Warcraft";
  touch(wow + "/.build.info");
  CHECK(!gamesource::cacheDirProblem(wow + "/cdn-cache").isEmpty());
  CHECK(!gamesource::cacheDirProblem(wow + "/two/levels/down").isEmpty());   // not created yet
  CHECK(!gamesource::prepareCache(wow + "/cdn-cache", &err));
  CHECK(!QDir(wow + "/cdn-cache").exists());

  // Ours: marked, measured, cleared.
  const QString cache = root + "/cdn-cache";
  CHECK(gamesource::cacheDirProblem(cache).isEmpty());
  CHECK(gamesource::prepareCache(cache, &err));
  CHECK(gamesource::isOurCache(cache));
  CHECK(gamesource::cacheDirFor(cache) == QDir::cleanPath(cache));
  touch(cache + "/wow/versions");
  touch(cache + "/wow/data/ab/cd/abcd0123");
  int files = 0;
  CHECK(gamesource::measureCache(cache, &files) > 0);
  CHECK(files == 3);
  // The completed build lives in the cache, so "Leeren" resets it without bookkeeping.
  CHECK(gamesource::completedBuild(cache).isEmpty());
  gamesource::setCompletedBuild(cache, "12.1.0.69933");
  CHECK(gamesource::completedBuild(cache) == "12.1.0.69933");
  CHECK(gamesource::prepareCache(cache, &err));      // a later start keeps it
  CHECK(gamesource::completedBuild(cache) == "12.1.0.69933");
  CHECK(gamesource::freeBytes(cache) > 0);
  CHECK(gamesource::freeBytes(cache + "/not/yet/created") > 0);
  CHECK(gamesource::clearCache(cache, &err));
  CHECK(!QDir(cache).exists());
  CHECK(gamesource::clearCache(cache, &err));        // nothing there: nothing to do

  CHECK(gamesource::formatBytes(1331439862ull) == QString::fromUtf8("1,24 GB"));
  CHECK(gamesource::formatBytes(398458880ull) == QString::fromUtf8("380 MB"));
  CHECK(gamesource::formatBytes(12u * 1024u) == QString::fromUtf8("12 KB"));
  // The sizes the dialog, the splash and the free-space warning announce.
  CHECK(gamesource::formatBytes(gamesource::kFirstStartBytes) == QString::fromUtf8("400 MB"));
  CHECK(gamesource::formatBytes(gamesource::kPatchBytes) == QString::fromUtf8("280 MB"));
}

// --- the data language ------------------------------------------------------------------------
void testLocales()
{
  // The display language decides, in the forms Windows reports it.
  CHECK(gamesource::dataLocaleFor("de-DE") == "deDE");
  CHECK(gamesource::dataLocaleFor("de-CH") == "deDE");
  CHECK(gamesource::dataLocaleFor("en-US") == "enUS");
  CHECK(gamesource::dataLocaleFor("en-GB") == "enGB");
  CHECK(gamesource::dataLocaleFor("fr-FR") == "frFR");
  CHECK(gamesource::dataLocaleFor("es-ES") == "esES");
  CHECK(gamesource::dataLocaleFor("es-MX") == "esMX");
  CHECK(gamesource::dataLocaleFor("pt-BR") == "ptBR");
  CHECK(gamesource::dataLocaleFor("ko-KR") == "koKR");
  CHECK(gamesource::dataLocaleFor("zh-TW") == "zhTW");
  CHECK(gamesource::dataLocaleFor("zh-Hant-HK") == "zhTW");
  CHECK(gamesource::dataLocaleFor("zh-CN") == "zhCN");
  // A language the game has no data for, or none reported at all: English (US).
  CHECK(gamesource::dataLocaleFor("ja-JP") == "enUS");
  CHECK(gamesource::dataLocaleFor(QString()) == "enUS");
  // Whatever this machine reports, the defaults are values the engine takes.
  CHECK(gamesource::isDataLocale(gamesource::defaultLocale()));
  CHECK(gamesource::isRegion(gamesource::defaultRegion()));
}

// --- the worker -------------------------------------------------------------------------------
void testWorker()
{
  // The job runs elsewhere, the ticks here, and the result comes back.
  int ticks = 0;
  const Qt::HANDLE mainThread = QThread::currentThreadId();
  Qt::HANDLE jobThread = nullptr;
  const bool r = gamesource::runWhileResponsive(
    [&jobThread]() { jobThread = QThread::currentThreadId(); QThread::msleep(350); return true; },
    [&ticks]() { ++ticks; }, 50);
  CHECK(r);
  CHECK(jobThread != nullptr && jobThread != mainThread);
  CHECK(ticks >= 4);                 // ~7 while running, and the final one

  // A job that is done before the loop starts must not hang it.
  ticks = 0;
  CHECK(!gamesource::runWhileResponsive([]() { return false; }, [&ticks]() { ++ticks; }, 50));
  CHECK(ticks >= 1);
}

}  // namespace

int main(int argc, char** argv)
{
  QCoreApplication app(argc, argv);
  QTemporaryDir root;
  // The settings path is relative, exactly as in the application.
  QDir::setCurrent(root.path());

  testSettings(root.path());
  testArguments(root.path());
  testCache(root.path());
  testLocales();
  testWorker();

  if (failures == 0)
    std::printf("sourcetest: all checks passed\n");
  else
    std::printf("sourcetest: %d check(s) FAILED\n", failures);
  return failures;
}
