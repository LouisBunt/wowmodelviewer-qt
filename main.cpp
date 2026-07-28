// Phase 1: prove the WMV render pipeline runs under Qt.
//
// Links core.dll/wow.dll unchanged, initialises CASC the same way the wx front-end
// does, loads one M2 by FileDataID and draws it. No UI beyond a window.
//
//   phase1.exe <wow-data-folder> <fileDataId>
//
// Example:
//   phase1.exe "C:\Program Files (x86)\World of Warcraft" 1000001

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QTextStream>
#include <QTimer>
#include <QMessageBox>
#include <QString>
#include <QVBoxLayout>

#include "GLHost.h"
#include "CharacterPanel.h"
#include "MainWindow.h"

#include "Game.h"
#include "GameFile.h"
#include "CharTexture.h"
#include "RaceInfos.h"
#include "WoWDatabase.h"
#include "WoWFolder.h"
#include "WoWModel.h"

// Append a stage marker to a file. The window never appears while the game data
// is loading, so this is the only way to see which call blocks.
static void trace(const QString& stage)
{
  QFile f("C:/Users/braun/wmv-qt/phase1-trace.txt");
  if (f.open(QIODevice::Append | QIODevice::Text)) {
    QTextStream(&f) << QDateTime::currentDateTime().toString("HH:mm:ss.zzz")
                    << "  " << stage << "\n";
  }
}

int main(int argc, char** argv)
{
  trace("main entered");
  QApplication app(argc, argv);
  trace("QApplication constructed");

  const QString dataFolder = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                      : QStringLiteral("C:/Program Files (x86)/World of Warcraft");
  const uint fileId = argc > 2 ? QString::fromLocal8Bit(argv[2]).toUInt() : 1000001u;

  auto* win = new MainWindow;
  GLHost* host = win->canvas();
  // Kept so the existing failure paths below still have somewhere to report to.
  auto* status = new QLabel;

  // --- game init: identical to what modelviewer.cpp does, minus the wx wrapping
  // CASCFolder builds the build-info path as "<folder>\..\.build.info", so the
  // folder handed to WoWFolder has to be the client's Data directory, not the
  // install root. Passing the root silently yields zero configs.
  QString cascFolder = QDir::fromNativeSeparators(dataFolder);
  while (cascFolder.endsWith('/'))
    cascFolder.chop(1);
  if (!cascFolder.endsWith("/Data", Qt::CaseInsensitive))
    cascFolder += "/Data";
  trace("CASC folder = " + cascFolder);

  trace("before WoWFolder construction");
  auto* folder = new wow::WoWFolder(QDir::toNativeSeparators(cascFolder));
  trace("WoWFolder constructed; before Game::init");
  core::Game::instance().init(folder, new wow::WoWDatabase());
  trace("Game::init returned");
  if (!core::Game::instance().initDone()) {
    status->setText("Game::init failed -- is the data folder correct?");
    win->show();
    return app.exec();
  }
  // Game::init() only wires the objects together -- initDone() merely checks two
  // pointers. Nothing is opened until a config is chosen and handed to setConfig(),
  // which is what actually mounts the CASC storage. Skipping this was why every
  // getFile() came back null.
  trace("before configsFound");
  std::vector<core::GameConfig> configs = GAMEDIRECTORY.configsFound();
  trace(QString("configsFound returned %1 entries").arg(configs.size()));
  if (configs.empty()) {
    trace("FATAL: no config found in the data folder");
    status->setText("No WoW locale/config found in that folder");
    win->show();
    QTimer::singleShot(3000, qApp, &QApplication::quit);
    return app.exec();
  }

  // Same rule the wx front-end uses: prefer the retail "wow" product, newest build.
  auto isNewer = [](const QString& a, const QString& b) {
    const QStringList va = a.split('.'), vb = b.split('.');
    const int n = qMax(va.size(), vb.size());
    for (int i = 0; i < n; ++i) {
      const long long na = i < va.size() ? va[i].toLongLong() : 0;
      const long long nb = i < vb.size() ? vb[i].toLongLong() : 0;
      if (na != nb) return na > nb;
    }
    return false;
  };
  size_t best = 0;
  for (size_t i = 1; i < configs.size(); ++i) {
    const bool bestRetail = configs[best].product == "wow";
    const bool iRetail    = configs[i].product == "wow";
    if (iRetail != bestRetail) { if (iRetail) best = i; }
    else if (isNewer(configs[i].version, configs[best].version)) best = i;
  }
  const core::GameConfig config = configs[best];
  trace(QString("chosen config: %1 / %2 / %3")
          .arg(config.locale).arg(config.product).arg(config.version));

  trace("before setConfig (mounts CASC)");
  if (!GAMEDIRECTORY.setConfig(config)) {
    trace(QString("FATAL: setConfig failed, lastError=%1").arg(GAMEDIRECTORY.lastError()));
    status->setText(QString("setConfig failed (error %1)").arg(GAMEDIRECTORY.lastError()));
    win->show();
    QTimer::singleShot(3000, qApp, &QApplication::quit);
    return app.exec();
  }
  trace("setConfig returned OK");

  // configFolder must match the client's major version; the listfile path below is
  // resolved relative to it.
  const QString cfgFolder = QString("games/wow/%1.0/").arg(config.version.section('.', 0, 0));
  core::Game::instance().setConfigFolder(cfgFolder);
  trace("configFolder = " + cfgFolder);

  trace("before initFromListfile");
  GAMEDIRECTORY.initFromListfile("../../../listfile.csv");
  trace("initFromListfile returned");

  // The database, texture regions and race registry. Without these WoWModel cannot
  // resolve a model file to a race, infos.raceID stays -1, and character models draw
  // as a handful of untextured geosets -- which is exactly what Phase 1 produced.
  trace("before GAMEDATABASE.initFromXML");
  if (!GAMEDATABASE.initFromXML("database.xml"))
    trace("WARNING: database.xml init failed -- character data will be empty");
  else
    trace("database.xml loaded");

  CharTexture::initRegions();
  RaceInfos::init();
  trace("RaceInfos::init done");

  trace("before getFile");
  GameFile* file = GAMEDIRECTORY.getFile(fileId);
  trace("getFile returned");
  if (!file) {
    status->setText(QString("FileDataID %1 not found in CASC").arg(fileId));
    win->show();
    return app.exec();
  }

  // The model must be built only AFTER the GL context exists: WoWModel uploads its
  // textures during construction, and without a current context those uploads fail
  // silently -- geometry survives (CPU side) but everything renders untextured.
  // GLHost creates the context in showEvent, so show first, pump the event queue,
  // then load.
  trace("before show (context comes up here)");
  win->show();
  app.processEvents();
  trace(QString("context ready = %1").arg(host->isReady() ? "yes" : "NO"));
  if (!host->isReady())
    trace("GL init error: " + host->lastError());

  trace("before WoWModel construction");
  auto* model = new WoWModel(file, false);
  trace("WoWModel constructed");
  host->setModel(model);

  for (int i = 1; i < argc - 1; ++i) {
    const QString arg(argv[i]);
    if (arg == "--shot")
      host->grabAfter(90, QString::fromLocal8Bit(argv[i + 1]));  // ~1.5 s of frames
    else if (arg == "--view") {                                   // "<yaw>,<pitch>"
      const QStringList yp = QString::fromLocal8Bit(argv[i + 1]).split(',');
      if (yp.size() == 2) {
        host->setView(yp[0].toFloat(), yp[1].toFloat());
        trace(QString("view set to yaw=%1 pitch=%2").arg(yp[0]).arg(yp[1]));
      }
    }
  }
  win->setBuildLabel(QString("CASC · %1").arg(config.version));
  win->setPathLabel(file->fullname());

  // Fill the browser and make picking a row load that model.
  trace("before populateTree");
  win->populateTree();
  trace("populateTree returned");

  QObject::connect(win, &MainWindow::fileActivated, [win, host](GameFile* picked) {
    if (!picked)
      return;
    auto* m = new WoWModel(picked, false);
    host->setModel(m);
    win->setPathLabel(picked->fullname());
    // Character models need their CharDetails set up before they render complete;
    // creatures and props resolve raceID == -1 and are left alone.
    win->characterPanel()->setModel(m->infos.raceID != -1 ? m : nullptr);
  });

  if (win->characterPanel())
    win->characterPanel()->setModel(model->infos.raceID != -1 ? model : nullptr);

  win->show();

  // The GL context is set up in GLHost::showEvent, i.e. after show(). If it did
  // not come up, report why to a file and quit -- otherwise the window just sits
  // there doing nothing and the reason stays invisible.
  QTimer::singleShot(2500, [host]() {
    if (host->isReady())
      return;
    QFile f("C:/Users/braun/wmv-qt/phase1-error.txt");
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
      QTextStream out(&f);
      out << (host->lastError().isEmpty()
                ? QStringLiteral("video never initialised and no error was recorded "
                                 "-- showEvent/initVideo did not run at all")
                : host->lastError()) << "\n";
    }
    qApp->quit();
  });

  return app.exec();
}
