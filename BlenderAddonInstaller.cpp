#include "BlenderAddonInstaller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QRegularExpression>
#include <QStandardPaths>

namespace {

const char* kAddonName = "io_import_wmv_fbx";

// Blender's per-user config root. On Windows that is the ROAMING profile
// (%APPDATA%\Blender Foundation\Blender) -- deliberately read from the environment,
// because Qt's GenericConfigLocation resolves to %LOCALAPPDATA%, one folder over,
// where the version scan comes back empty on every machine.
QString blenderConfigRoot()
{
  QString base = qEnvironmentVariable("APPDATA");
  if (base.isEmpty())
    base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
  return base + "/Blender Foundation/Blender";
}

// "3.6", "4.2", "5.1" ... -- Blender's version folders. Anything below 2.80 predates
// the current add-on API and is skipped, exactly as wow.export does.
bool isSupportedVersionFolder(const QString& name)
{
  const QRegularExpressionMatch m =
    QRegularExpression("^(\\d+)\\.(\\d+)$").match(name);
  if (!m.hasMatch())
    return false;
  const int major = m.captured(1).toInt();
  const int minor = m.captured(2).toInt();
  return major > 2 || (major == 2 && minor >= 80);
}

}  // namespace

QString BlenderAddonInstaller::addonSourceDir()
{
  const QString packaged =
    QCoreApplication::applicationDirPath() + "/blender_addon/" + kAddonName;
  if (QFileInfo(packaged + "/__init__.py").isFile())
    return packaged;

  // Development tree: the add-on lives in the upstream submodule next to this repo.
  const QString dev = QStringLiteral("upstream/blender_addon/") + kAddonName;
  if (QFileInfo(dev + "/__init__.py").isFile())
    return QFileInfo(dev).absoluteFilePath();

  return QString();
}

BlenderAddonInstaller::Result BlenderAddonInstaller::install()
{
  Result result;

  const QString source = addonSourceDir();
  if (source.isEmpty()) {
    result.error = QObject::tr(
      "Das Blender-Addon liegt nicht neben der Anwendung (Ordner \"blender_addon\").");
    return result;
  }

  QDir root(blenderConfigRoot());
  if (!root.exists()) {
    result.error = QObject::tr(
      "Kein Blender-Konfigurationsordner gefunden. Wurde Blender schon einmal gestartet?");
    return result;
  }

  const QStringList sourceFiles =
    QDir(source).entryList(QDir::Files | QDir::NoDotAndDotDot);

  for (const QString& version : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
    if (!isSupportedVersionFolder(version))
      continue;

    // scripts/addons may not exist yet on a fresh Blender profile; create it rather
    // than skipping the version -- Blender scans it on start regardless.
    const QString target = root.filePath(version + "/scripts/addons/" + kAddonName);

    // Replace, don't merge: a leftover file from an older add-on version (or a stale
    // __pycache__) must not survive next to the new code.
    QDir targetDir(target);
    if (targetDir.exists())
      targetDir.removeRecursively();
    if (!QDir().mkpath(target)) {
      result.error = QObject::tr("Konnte nicht nach %1 schreiben.").arg(target);
      return result;
    }

    bool copiedAll = true;
    for (const QString& file : sourceFiles)
      copiedAll &= QFile::copy(source + "/" + file, target + "/" + file);
    if (!copiedAll) {
      result.error = QObject::tr("Kopieren nach %1 ist fehlgeschlagen.").arg(target);
      return result;
    }

    result.installedVersions++;
  }

  if (result.installedVersions == 0)
    result.error = QObject::tr(
      "Keine Blender-Version ab 2.80 gefunden (gesucht unter %1).")
        .arg(QDir::toNativeSeparators(root.absolutePath()));

  return result;
}
