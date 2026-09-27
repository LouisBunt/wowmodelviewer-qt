#ifndef GAMESOURCE_H
#define GAMESOURCE_H

// Where the game data comes from: a World of Warcraft installation on this machine, or
// Blizzard's public download servers (the CDN) with a cache folder in between -- the way
// wow.export's "CDN" mode works.
//
// One module for what used to be two copies of the same two constants (main.cpp and
// MenuController.cpp each defined kSettingsFile and kFolderKey) and one copy of the
// .build.info test. The data source now has more to remember than a folder -- region,
// language, cache -- and every piece of it lives here, so main(), the menu and the dialog
// cannot disagree about a key name.
//
// QtCore only: no engine header, no widget. That keeps it testable offscreen
// (tests/sourcetest). The engine side is wow::WoWFolder::setOnline(), which fetches the build
// information itself and keeps everything it downloads under <cacheDir>/<product>/.

#include <functional>
#include <vector>

#include <QString>
#include <QStringList>

struct GameSource
{
  enum Kind { None, Local, Online };
  Kind kind = None;

  QString folder;       // Local: the install root that holds .build.info
  QString region;       // Online: eu | us | kr | tw | cn -- a row of Blizzard's "versions"/"cdns"
  QString locale;       // Online: CASC locale of names and texts (deDE, enUS, ...)
  QString cacheDir;     // Online: absolute path of the CDN cache
  bool offline = false; // Online: the version service did not answer this start, so the cached
                        // build is served -- only what was downloaded before can be opened
};

namespace gamesource {

// The front-end's own settings file. Relative on purpose: main() pins the working directory
// to the executable before anything reads it.
extern const char* const kSettingsFile;

// The retail product. The only one the bundled data definitions (games/wow/<major>.0) and the
// version check in main() are made for; Classic products would open and then have no database.
extern const char* const kProduct;

struct Choice
{
  const char* code;     // as Blizzard's files and CascLib spell it
  const char* label;    // shown in the dialog
};

// The regions the dialog offers. Blizzard's "cdns" file has rows for us, eu, kr, tw and cn;
// "versions" also lists sg and xx, which have no CDN hosts. China is accepted on the command
// line but not offered: its only host is NetEase's CDN, which is untested and, from outside
// China, the slowest choice by far. Europe first -- the interface is German.
const std::vector<Choice>& regions();
bool isRegion(const QString& code);       // any of the five, cn included
QString defaultRegion();                  // from the Windows region setting, else "eu"

// CASC locales the engine maps (CASCFolder::setConfig), named in German like the rest of the
// interface. The default follows the Windows display language, English (US) otherwise.
const std::vector<Choice>& dataLocales();
bool isDataLocale(const QString& code);
QString defaultLocale();
// The data language for a display language as Windows names it ("de-DE", "zh-Hant-TW");
// English (US) for everything the game has no data language for.
QString dataLocaleFor(const QString& uiLanguage);
QString regionLabel(const QString& code);
QString localeLabel(const QString& code);

// "<exe folder>/cdn-cache". Next to the executable like wowdb.sqlite, userSettings and the
// log: that is where the setup puts everything the program writes (a per-user folder, see
// MVMidnight.iss), the write probe in main() already vouches for it, and the uninstaller
// removes it. NOT inside userSettings: that folder is what a bug report attaches, and half a
// gigabyte of game files has no business in a zip for an issue tracker.
QString defaultCacheDir();

// CASCFolder reads "<install>/.build.info". Testing for that file is the cheapest way to tell
// a real installation from a wrong folder, and it has to happen before Game::init(), which
// takes ownership of the folder object with no second attempt.
bool looksLikeWoWInstall(const QString& folder);

// A remembered local source whose folder has gone away comes back as None, so the caller asks
// instead of failing on a dead path. Unknown region/locale values fall back to the defaults.
GameSource load();
void save(const GameSource& source);

// Same kind, and the same folder (local) or region, language and cache (online) -- i.e. a
// restart would change nothing.
bool sameSource(const GameSource& a, const GameSource& b);

// One line for the trace and the dialog: "online · eu · deDE · <cache>" / "local · <folder>".
QString describe(const GameSource& source);

// The known WoW folder, independent of the data source. Online users who DO have the game
// installed (on another drive, say) still want the MVLink addon put into it and its
// SavedVariables read. Empty unless the stored folder still holds a .build.info.
QString installFolder();
void setInstallFolder(const QString& folder);

// Scheduled clearing. The cache cannot be deleted while CascLib has it open, so a clear asked
// for during an online session is carried out by the next start -- whichever source that start
// uses -- before anything opens the folder. Empty = nothing scheduled.
QString pendingClear();
void setPendingClear(const QString& dir);

// --- the command line -----------------------------------------------------------------------
//
//   WoWModelViewer-Qt.exe [<WoW-Ordner>] [<FileDataID>] [--online [<region>]]
//                         [--locale <xxYY>] [--cache <ordner>] [...]
//
// Positional arguments are the ones before the first --flag. The first one is the model when
// it is all digits and not an existing folder, so "917116 --export ..." takes the folder from
// the settings instead of trying to open a WoW installation called 917116.
struct Arguments
{
  QString folder;             // positional install folder; empty = none given
  uint modelId = 0;           // positional FileDataID; 0 = none
  bool online = false;        // --online
  QString region;             // --online <region>; empty = the saved one
  QString locale;             // --locale; empty = the saved one
  QString cacheDir;           // --cache; empty = the saved one
  QStringList notes;          // ignored or conflicting values, for the trace
};
Arguments parseArguments(const QStringList& args);   // args without the program name

// The source for this run: the settings, overridden by the command line. Nothing from the
// command line is ever saved -- a script must not change what the next interactive start does.
// A folder on the command line is taken as given, never second-guessed with a dialog.
GameSource applyArguments(GameSource saved, const Arguments& args);

// --- the cache folder -----------------------------------------------------------------------

// Creates the folder, drops the marker file into it and proves it is writable. The marker is
// what clearCache() insists on before it deletes anything.
bool prepareCache(const QString& dir, QString* error);

// True when `dir` holds the marker, i.e. this program created it. "Leeren" deletes a whole
// folder tree; pointed at "C:\Users\Name" by mistake, that must be a refusal, not a disaster.
bool isOurCache(const QString& dir);

// Empty, or why `dir` cannot be the cache: inside a WoW installation, CascLib's search for its
// build files would climb out of the cache and open the installation instead.
QString cacheDirProblem(const QString& dir);

// A folder the user picked for the cache: used as-is when it is empty or already a cache,
// otherwise a subfolder "ModelViewer-Cache" inside it.
QString cacheDirFor(const QString& picked);

// The build the last complete online open used this cache for, kept in the cache itself
// (complete-build.txt) so that clearing or moving the cache resets it with no bookkeeping.
// Differs from the build the version service names -- or is empty -- exactly when the open
// ahead downloads the big tables: the first online start, the first one after a WoW patch,
// the first one after "Leeren" or with a new cache folder.
QString completedBuild(const QString& dir);
void setCompletedBuild(const QString& dir, const QString& build);

// Deletes the cache. Refuses without the marker. Missing folder = nothing to do = success.
bool clearCache(const QString& dir, QString* error);

// Walks the folder. Seconds on a large cache (one file per downloaded asset) -- call it off
// the UI thread.
quint64 measureCache(const QString& dir, int* files = nullptr);

// Free bytes on the drive that holds (or will hold) `dir`; -1 when unknown.
qint64 freeBytes(const QString& dir);

// What a first online start writes, measured on 12.1.0.69933: ~346 MB for the open (the 1,401
// archive indexes and the encoding table, which the engine fetches ahead of it, then the root
// table; the download manifest is not fetched at all) plus ~20 MB of database tables, and the
// first models on top. Every WoW patch brings a new encoding and root table, new database
// tables and some new indexes. Rounded up, for the texts and for the free-space warning.
constexpr qint64 kFirstStartBytes = 400ll * 1024 * 1024;
constexpr qint64 kPatchBytes = 280ll * 1024 * 1024;
constexpr qint64 kComfortBytes = 1024ll * 1024 * 1024;   // warn below this before a cold start

// "1,24 GB", "380 MB", "12 KB" -- German number format, whatever the system locale.
QString formatBytes(quint64 bytes);

// --- keeping the splash alive -----------------------------------------------------------------

// Runs `job` on a worker thread and returns its result; meanwhile this thread runs an event
// loop and calls `tick` every `tickMs`, and once more at the end. For the one engine call that
// must not freeze the window: the online open, where a single request can take a minute and
// CascLib reports nothing while it runs. `job` must not touch widgets; `tick` must not touch
// what `job` works on.
bool runWhileResponsive(const std::function<bool()>& job, const std::function<void()>& tick,
                        int tickMs = 100);

}  // namespace gamesource

#endif  // GAMESOURCE_H
