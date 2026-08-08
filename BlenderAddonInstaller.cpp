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

  QStringList failed;      // one entry per Blender version that could not be updated

  for (const QString& version : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
    if (!isSupportedVersionFolder(version))
      continue;

    // scripts/addons may not exist yet on a fresh Blender profile; create it rather
    // than skipping the version -- Blender scans it on start regardless.
    const QString target = root.filePath(version + "/scripts/addons/" + kAddonName);

    // Copy beside the target and swap only once everything has arrived. Deleting first was
    // a trap: when anything went wrong afterwards -- Blender holding a file open is the
    // everyday case -- the user was left with NO add-on where a working one had been.
    //
    // Replace rather than merge, so a leftover file from an older version (or a stale
    // __pycache__) cannot survive next to the new code.
    const QString staging = target + ".new";
    QDir stagingDir(staging);
    if (stagingDir.exists())
      stagingDir.removeRecursively();
    if (!QDir().mkpath(staging)) {
      failed << QString("%1 (%2)").arg(version, QObject::tr("Ordner nicht anlegbar"));
      continue;
    }

    bool copiedAll = true;
    for (const QString& file : sourceFiles)
      copiedAll &= QFile::copy(source + "/" + file, staging + "/" + file);
    if (!copiedAll) {
      stagingDir.removeRecursively();
      failed << QString("%1 (%2)").arg(version, QObject::tr("Kopieren fehlgeschlagen"));
      continue;
    }

    QDir targetDir(target);
    if (targetDir.exists() && !targetDir.removeRecursively()) {
      // The old add-on is still in place and still works. Leave it, and say why.
      stagingDir.removeRecursively();
      failed << QString("%1 (%2)").arg(version,
                                       QObject::tr("Datei in Benutzung, Blender schließen"));
      continue;
    }
    if (!QDir().rename(staging, target)) {
      failed << QString("%1 (%2)").arg(version, QObject::tr("Umbenennen fehlgeschlagen"));
      continue;
    }

    result.installedVersions++;
  }

  // Report per version rather than abandoning the run at the first problem: with two
  // Blender installations, one failure used to hide that the other had succeeded.
  if (!failed.isEmpty()) {
    result.error = result.installedVersions > 0
        ? QObject::tr("In %1 Blender-Version(en) installiert; fehlgeschlagen: %2")
            .arg(result.installedVersions).arg(failed.join(", "))
        : QObject::tr("Installation fehlgeschlagen: %1").arg(failed.join(", "));
    return result;
  }

  if (result.installedVersions == 0)
    result.error = QObject::tr(
      "Keine Blender-Version ab 2.80 gefunden (gesucht unter %1).")
        .arg(QDir::toNativeSeparators(root.absolutePath()));

  return result;
}
