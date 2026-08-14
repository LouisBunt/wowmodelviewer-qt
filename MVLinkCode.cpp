#include "MVLinkCode.h"

#include <algorithm>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QRegularExpression>
#include <QTextStream>

namespace {

// The only format this build was written against. A later one may move fields around,
// and a wrong-but-plausible import is worse than a refusal -- the dressing-room decoder
// learned that the hard way when anything >= v15 went through the v15 layout.
const int kNewestKnownVersion = 1;

bool fail(QString* error, const QString& text)
{
  if (error)
    *error = text;
  return false;
}

}  // namespace

bool mvlink_parse_code(const QString& code, MVLinkLook* out, QString* error)
{
  if (!out)
    return false;

  const QString trimmed = code.trimmed();
  if (trimmed.isEmpty())
    return fail(error, QObject::tr("Kein Code eingegeben."));

  const QStringList parts = trimmed.split(':', QString::SkipEmptyParts);
  if (parts.isEmpty() || !parts.first().startsWith("MVM", Qt::CaseInsensitive))
    return fail(error, QObject::tr("Das sieht nicht nach einem MVLink-Code aus. Erwartet "
                                   "wird etwas, das mit \"MVM1:\" beginnt."));

  bool ok = false;
  const int version = parts.first().midRef(3).toInt(&ok);
  if (!ok || version <= 0)
    return fail(error, QObject::tr("Die Formatkennung \"%1\" ist unlesbar.").arg(parts.first()));
  if (version > kNewestKnownVersion)
    return fail(error, QObject::tr("Dieser Code stammt aus einer neueren Addon-Fassung "
                                   "(Format v%1, lesbar bis v%2). Bitte ModelViewer "
                                   "aktualisieren.").arg(version).arg(kNewestKnownVersion));

  MVLinkLook look;
  look.version = version;
  bool haveRace = false;

  for (int i = 1; i < parts.size(); ++i) {
    const QString field = parts.at(i);
    const int eq = field.indexOf('=');
    if (eq <= 0)
      return fail(error, QObject::tr("Unlesbarer Abschnitt im Code: \"%1\".").arg(field));

    const QString key = field.left(eq);
    const QString value = field.mid(eq + 1);

    if (key == "R") {
      look.race = value.toInt(&ok);
      if (!ok || look.race <= 0)
        return fail(error, QObject::tr("Unlesbare Rasse im Code: \"%1\".").arg(value));
      haveRace = true;
    } else if (key == "S") {
      look.gender = value.toInt(&ok) ? 1 : 0;
    } else {
      // A slot entry: <slot>=<itemID>.<modifier>
      const int slot = key.toInt(&ok);
      if (!ok || slot < 0)
        continue;                       // unknown key, skip rather than refuse the lot
      const QStringList item = value.split('.');
      MVLinkLook::Piece p;
      p.slot = slot;
      p.itemId = item.value(0).toInt(&ok);
      if (!ok || p.itemId <= 0)
        return fail(error, QObject::tr("Unlesbare Item-Nummer im Code: \"%1\".").arg(value));
      p.modifier = item.size() > 1 ? item.at(1).toInt() : 0;
      look.pieces.push_back(p);
    }
  }

  if (!haveRace)
    return fail(error, QObject::tr("Im Code fehlt die Rasse — er ist unvollständig."));
  if (look.pieces.empty())
    return fail(error, QObject::tr("Der Code enthält kein einziges Ausrüstungsteil."));

  *out = look;
  return true;
}

// --------------------------------------------------------------------------------------

bool mvlink_read_saved_variables(const QString& path, const QString& outfitName,
                                 QString* codeOut, QString* error)
{
  if (!codeOut)
    return false;

  QFile f(path);
  if (!f.exists())
    return fail(error, QObject::tr("Keine MVLink-Daten gefunden. Ist das Addon im Spiel "
                                   "aktiv?\n\n%1").arg(QDir::toNativeSeparators(path)));
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    return fail(error, QObject::tr("Die Datei ließ sich nicht lesen:\n%1")
                         .arg(QDir::toNativeSeparators(path)));

  const QString text = QString::fromUtf8(f.readAll());

  // One value per line, no nesting -- the addon writes it that way on purpose. Matching
  // the assignment is enough and stays readable, where a Lua parser would be a dependency
  // for four lines of data.
  if (outfitName.isEmpty()) {
    // WoW writes SavedVariables with every key bracketed and quoted -- ["current"] = "…",
    // not current = "…". Both spellings are accepted so a hand-written test file works
    // too, but the bracketed one is what actually comes out of the game.
    QRegularExpression rx("(?:\\[\"current\"\\]|\\bcurrent)\\s*=\\s*\"([^\"]+)\"");
    const auto m = rx.match(text);
    if (!m.hasMatch())
      return fail(error, QObject::tr("In der Datei steht kein aktueller Look. Im Spiel "
                                     "einmal \"Für ModelViewer ablegen\" drücken und "
                                     "danach /reload ausführen."));
    *codeOut = m.captured(1);
    return true;
  }

  // Outfit names are written as Lua table keys, ["Name"] = "code". The name comes from
  // the player and can hold regex characters, so it is escaped.
  QRegularExpression rx(QString("\\[\"%1\"\\]\\s*=\\s*\"([^\"]+)\"")
                          .arg(QRegularExpression::escape(outfitName)));
  const auto m = rx.match(text);
  if (!m.hasMatch())
    return fail(error, QObject::tr("Das Outfit \"%1\" steht nicht in der Datei.")
                         .arg(outfitName));
  *codeOut = m.captured(1);
  return true;
}

std::vector<QString> mvlink_saved_variable_paths(const QString& wowInstallFolder)
{
  std::vector<QString> found;
  if (wowInstallFolder.isEmpty())
    return found;

  // WTF\Account\<ACCOUNT>\SavedVariables\MVLink.lua, one per account. The flavour folder
  // (_retail_, _classic_) sits between the install root and WTF on modern installations,
  // so both shapes are searched.
  QStringList roots;
  roots << wowInstallFolder + "/WTF/Account";
  const QDir base(wowInstallFolder);
  for (const QString& flavour : base.entryList(QStringList() << "_*_", QDir::Dirs))
    roots << wowInstallFolder + "/" + flavour + "/WTF/Account";

  for (const QString& root : roots) {
    QDir dir(root);
    if (!dir.exists())
      continue;
    for (const QString& account : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
      const QString candidate = root + "/" + account + "/SavedVariables/MVLink.lua";
      if (QFile::exists(candidate))
        found.push_back(candidate);
    }
  }

  // Newest first: whoever plays two accounts most likely means the one just closed.
  std::sort(found.begin(), found.end(), [](const QString& a, const QString& b) {
    return QFileInfo(a).lastModified() > QFileInfo(b).lastModified();
  });
  return found;
}

QString mvlink_saved_variables_updated_at(const QString& path)
{
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    return QString();
  const QString text = QString::fromUtf8(f.readAll());
  // Same one-value-per-line contract as "current"; the addon writes it flat on purpose.
  QRegularExpression rx("\\[\"updatedAt\"\\]\\s*=\\s*\"([^\"]+)\"");
  const auto m = rx.match(text);
  return m.hasMatch() ? m.captured(1) : QString();
}

// --------------------------------------------------------------------------------------

namespace {

// Beside the exe once installed; in the source tree while developing.
QString addonSourceDir()
{
  const QString packaged = QCoreApplication::applicationDirPath() + "/addon/MVLink";
  if (QFileInfo(packaged + "/MVLink.toc").isFile())
    return packaged;
  const QString dev = QStringLiteral("addon/MVLink");
  if (QFileInfo(dev + "/MVLink.toc").isFile())
    return QFileInfo(dev).absoluteFilePath();
  return QString();
}

// Only the addon's own files, and only the kinds it consists of. A blanket recursive copy
// would happily carry along whatever else ended up in that folder.
bool copyAddonTree(const QString& from, const QString& to, QString* error)
{
  QDir().mkpath(to);
  QDir src(from);
  for (const QFileInfo& fi :
       src.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
    const QString target = to + "/" + fi.fileName();
    if (fi.isDir()) {
      if (!copyAddonTree(fi.absoluteFilePath(), target, error))
        return false;
      continue;
    }
    if (fi.suffix() != "lua" && fi.suffix() != "toc" && fi.suffix() != "xml")
      continue;
    // Overwrite: this is an update as often as it is a first install, and QFile::copy
    // refuses to write over an existing file.
    QFile::remove(target);
    if (!QFile::copy(fi.absoluteFilePath(), target)) {
      if (error)
        *error = QObject::tr("»%1« ließ sich nicht schreiben. Läuft WoW gerade, oder fehlen "
                             "Schreibrechte im Spielordner?")
                   .arg(QDir::toNativeSeparators(target));
      return false;
    }
  }
  return true;
}

}  // namespace

QString mvlink_install_addon(const QString& wowInstallFolder, QString* error)
{
  if (wowInstallFolder.isEmpty()) {
    fail(error, QObject::tr("Der WoW-Ordner ist nicht bekannt."));
    return QString();
  }

  const QString source = addonSourceDir();
  if (source.isEmpty()) {
    fail(error, QObject::tr("Das MVLink-Addon liegt nicht neben der Anwendung "
                            "(Ordner »addon\\MVLink«)."));
    return QString();
  }

  // The flavour folder, not the installation root: addons live under _retail_ (or _classic_
  // and friends). Retail first, because that is what the addon's Interface version targets;
  // if this installation has none, there is nowhere sensible to put it.
  QString flavour;
  const QDir base(wowInstallFolder);
  const QStringList flavours = base.entryList(QStringList() << "_*_", QDir::Dirs);
  if (flavours.contains("_retail_"))
    flavour = "_retail_";
  else if (!flavours.isEmpty())
    flavour = flavours.first();
  else {
    fail(error, QObject::tr("In »%1« steckt kein Spielordner wie »_retail_«. Ist das wirklich "
                            "der Ordner, in dem World of Warcraft installiert ist?")
                  .arg(QDir::toNativeSeparators(wowInstallFolder)));
    return QString();
  }

  const QString dest = wowInstallFolder + "/" + flavour + "/Interface/AddOns/MVLink";
  if (!copyAddonTree(source, dest, error))
    return QString();
  return QDir::toNativeSeparators(dest);
}
