#include "ExportController.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QSaveFile>

#include "ExporterPlugin.h"
#include "PluginManager.h"
#include "WoWModel.h"

namespace {
// Where the Blender add-on's "Letzten WMV-Export importieren" button looks. The path
// is the whole contract -- no IPC, both sides just hardcode it (add-on counterpart:
// blender_addon/io_import_wmv_fbx/__init__.py, _last_export_path()). wow.export has
// shipped this exact mechanism for years.
QString lastExportFilePath()
{
  QString base = qEnvironmentVariable("LOCALAPPDATA");
  if (base.isEmpty())
    base = QDir::homePath();
  return base + "/WMVMidnight/last_export.txt";
}

// One "FBX:<absolute path>" line. QSaveFile so the add-on can never read a half-written
// file: the content appears atomically or not at all.
void recordLastExport(const QString& fbxPath)
{
  const QString file = lastExportFilePath();
  QDir().mkpath(QFileInfo(file).absolutePath());
  QSaveFile out(file);
  if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return;                       // a missing handshake only costs the one-click import
  out.write(("FBX:" + QFileInfo(fbxPath).absoluteFilePath() + "\n").toUtf8());
  out.commit();
}
}

ExportController::ExportController(QObject* parent) : QObject(parent)
{
}

int ExportController::loadPlugins()
{
  formats_.clear();

  PLUGINMANAGER.init("./plugins");

  for (auto it = PLUGINMANAGER.begin(); it != PLUGINMANAGER.end(); ++it) {
    auto* exporter = dynamic_cast<ExporterPlugin*>(*it);
    if (!exporter)
      continue;

    Format f;
    // menuLabel() is something like "OBJ..." -- strip the ellipsis for a button.
    f.label = QString::fromStdWString(exporter->menuLabel());
    while (f.label.endsWith('.'))
      f.label.chop(1);
    f.filter = QString::fromStdWString(exporter->fileSaveFilter());
    f.plugin = exporter;
    formats_.push_back(f);
  }

  return (int)formats_.size();
}

QString ExportController::exportModel(WoWModel* model, int formatIndex, QWidget* parent)
{
  if (!model)
    return QObject::tr("Kein Modell geladen.");
  if (formatIndex < 0 || formatIndex >= (int)formats_.size())
    return QObject::tr("Kein Exportformat gewählt.");

  Format& f = formats_[formatIndex];

  const QString suggested = QFileInfo(model->name()).baseName();
  const QString path = QFileDialog::getSaveFileName(
    parent, QString::fromStdWString(f.plugin->fileSaveTitle()), suggested, f.filter);
  if (path.isEmpty())
    return QString();          // cancelled -- not an error

  return exportTo(model, formatIndex, path);
}

QString ExportController::exportTo(WoWModel* model, int formatIndex, const QString& path)
{
  if (!model)
    return QObject::tr("Kein Modell geladen.");
  if (formatIndex < 0 || formatIndex >= (int)formats_.size())
    return QObject::tr("Kein Exportformat gewählt.");
  if (path.isEmpty())
    return QObject::tr("Kein Zielpfad angegeben.");

  Format& f = formats_[formatIndex];

  // Was hardcoded to (mesh, no skeleton, skinning, no animation), which made the four
  // checkboxes in the Export tab decoration. Now it is whatever was actually chosen.
  //
  // Note that "no skeleton" never really held: the FBX exporter forces skeleton on when
  // skinning is requested, so the armature was being written regardless of the flag.
  f.plugin->setExportOptions(options_.mesh, options_.skeleton,
                             options_.skinning, options_.animation);
  f.plugin->setAnimationsToExport(options_.clips);

  if (!f.plugin->exportModel(model, path.toStdWString())) {
    const QString why = QString::fromStdWString(f.plugin->lastError());
    return why.isEmpty() ? QObject::tr("Export fehlgeschlagen (kein Grund gemeldet).") : why;
  }

  // FBX only: the handshake exists for the Blender add-on, and that imports FBX. Done
  // here rather than in a caller because every export route -- dialog, character tab,
  // --export -- funnels through this function.
  if (f.label.contains("fbx", Qt::CaseInsensitive) ||
      f.filter.contains("fbx", Qt::CaseInsensitive))
    recordLastExport(path);

  return QString();
}
