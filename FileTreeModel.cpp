#include "FileTreeModel.h"

#include <algorithm>
#include <set>

#include <QColor>
#include <QFont>
#include <QRegularExpression>

#include "Game.h"
#include "GameFile.h"
#include "RaceInfos.h"

namespace {
// The listfile's top level is the raw folder layout of the game: "character" and
// "creature" sit in it next to "cameras", "interface", "test" and "particles". Someone
// looking for a model has to know which of those thirteen names is worth opening.
//
// So the ones that hold models people actually look for are named in German and put
// first, in the order they get used; everything else keeps its real name but moves
// under "Sonstiges", one fold away instead of in the way.
const struct { const char* folder; const char* label; int rank; } kTopLevels[] = {
  { "character",    "Charaktere",           0 },
  { "creature",     "Kreaturen",            1 },
  { "item",         "Gegenstände",          2 },
  { "world",        "Welt und Gebäude",     3 },
  { "environments", "Umgebung",             4 },
  { "spells",       "Zauber und Effekte",   5 },
  { "spell",        "Zauber und Effekte",   5 },
  { "particles",    "Partikel",             6 },
};

const int kOtherRank = 9;
}

FileTreeModel::FileTreeModel(QObject* parent)
  : QAbstractItemModel(parent), root_(new Node)
{
}

FileTreeModel::~FileTreeModel()
{
  delete root_;
}

int FileTreeModel::rebuild(const QString& extensionFilter, const QString& search)
{
  beginResetModel();

  delete root_;
  root_ = new Node;

  // Same query the wx file control used: a regex over the full path, anchored on
  // the extension.
  //
  // A search that is nothing but digits means the FileDataID, not a path fragment --
  // the ids are no longer printed next to the names, so this is how one stays
  // reachable. The regex cannot express it (the id is not part of the path), so the
  // extension filter runs wide and the id is matched per file below.
  const QString needle = search.trimmed().toLower();
  const bool byFileId = !needle.isEmpty() &&
                        QRegularExpression("^\\d+$").match(needle).hasMatch();
  const uint wantedId = byFileId ? needle.toUInt() : 0;
  const QString pattern = byFileId
    ? ("^.*\\." + extensionFilter)
    : ("^.*" + QRegularExpression::escape(needle) + ".*\\." + extensionFilter);

  std::set<GameFile*> files;
  GAMEDIRECTORY.getFilteredFiles(files, const_cast<QString&>(pattern));

  int kept = 0;
  for (GameFile* f : files) {
    if (!f)
      continue;

    // fullname() mixes '/' and '\\' depending on where the entry came from.
    const QString full = f->fullname().replace('\\', '/');
    const QStringList parts = full.split('/', QString::SkipEmptyParts);
    if (parts.isEmpty())
      continue;

    // Category narrows by top-level folder. "Characters" is handled separately by
    // buildRaceBrowser(), so it never reaches here.
    if (category_ == Creatures && !full.startsWith("creature/", Qt::CaseInsensitive))
      continue;
    if (category_ == Items && !full.startsWith("item/", Qt::CaseInsensitive))
      continue;
    if (byFileId && f->fileDataId() != wantedId)
      continue;
    ++kept;

    const auto childNamed = [](Node* p, const QString& name, int rank) {
      auto it = p->byName.find(name);
      if (it != p->byName.end())
        return it->second;
      Node* child = new Node;
      child->name = name;
      child->rank = rank;
      child->parent = p;
      p->children.push_back(child);
      p->byName[name] = child;
      return child;
    };

    Node* cur = root_;

    // The first path segment is the one that gets renamed or tucked away; everything
    // below it keeps the game's own names, which are the ones people recognise once
    // they are in the right branch.
    int firstPart = 0;
    if (parts.size() > 1) {
      const QString top = parts[0].toLower();
      const auto* known = std::find_if(
        std::begin(kTopLevels), std::end(kTopLevels),
        [&top](const auto& e) { return top == QString::fromLatin1(e.folder); });

      if (known != std::end(kTopLevels)) {
        cur = childNamed(cur, QString::fromUtf8(known->label), known->rank);
        firstPart = 1;                 // its real name is now the heading
      } else {
        cur = childNamed(cur, QString::fromUtf8("Sonstiges"), kOtherRank);
      }
    }

    for (int i = firstPart; i < parts.size() - 1; ++i)
      cur = childNamed(cur, parts[i], 0);

    Node* leaf = new Node;
    // Just the file name. The wx tree printed the FileDataID next to it, which put a
    // number nobody reads in front of every single row; it lives in the tooltip now
    // and a digits-only search still finds a file by it.
    leaf->name = parts.last();
    leaf->fileId = (int)f->fileDataId();
    leaf->file = f;
    leaf->parent = cur;
    cur->children.push_back(leaf);
  }

  sortRecursive(root_);

  endResetModel();
  return kept;
}

int FileTreeModel::buildRaceBrowser(const QString& search)
{
  beginResetModel();

  delete root_;
  root_ = new Node;

  auto* playable = new Node;
  playable->name = QString::fromUtf8("Spielbar");
  playable->parent = root_;
  auto* npc = new Node;
  npc->name = QString::fromUtf8("NPC");
  npc->parent = root_;

  const QString needle = search.trimmed().toLower();
  int count = 0;

  for (const auto& e : RaceInfos::getRaceMenu()) {
    const QString raceName = QString::fromStdString(e.name);
    if (!needle.isEmpty() && !raceName.toLower().contains(needle))
      continue;

    Node* group = e.isNPC ? npc : playable;
    auto* race = new Node;
    race->name = raceName.isEmpty() ? QString("Rasse %1").arg(e.raceID) : raceName;
    race->parent = group;
    group->children.push_back(race);

    // One leaf per sex, each pointing at that model's FileDataID.
    const struct { int id; const char* label; } sexes[] = {
      { e.maleFileID, "Männlich" }, { e.femaleFileID, "Weiblich" }
    };
    for (const auto& s : sexes) {
      if (s.id <= 0)
        continue;
      auto* leaf = new Node;
      leaf->name = QString::fromUtf8(s.label);
      leaf->fileId = s.id;
      leaf->parent = race;
      race->children.push_back(leaf);
      ++count;
    }
  }

  // Drop an empty group rather than showing a dead heading.
  if (!playable->children.empty())
    root_->children.push_back(playable);
  else
    delete playable;
  if (!npc->children.empty())
    root_->children.push_back(npc);
  else
    delete npc;

  endResetModel();
  return count;
}

void FileTreeModel::sortRecursive(Node* n)
{
  std::sort(n->children.begin(), n->children.end(), [](const Node* a, const Node* b) {
    if (a->rank != b->rank)
      return a->rank < b->rank;           // the curated top level, in its own order
    if (a->isFolder() != b->isFolder())
      return a->isFolder();               // folders first
    return a->name.compare(b->name, Qt::CaseInsensitive) < 0;
  });
  for (Node* c : n->children)
    sortRecursive(c);
}

FileTreeModel::Node* FileTreeModel::nodeFor(const QModelIndex& index) const
{
  if (!index.isValid())
    return root_;
  return static_cast<Node*>(index.internalPointer());
}

GameFile* FileTreeModel::fileAt(const QModelIndex& index) const
{
  Node* n = nodeFor(index);
  return n ? n->file : nullptr;
}

int FileTreeModel::fileIdAt(const QModelIndex& index) const
{
  Node* n = nodeFor(index);
  return n ? n->fileId : 0;
}

QModelIndex FileTreeModel::index(int row, int column, const QModelIndex& parent) const
{
  if (!hasIndex(row, column, parent))
    return QModelIndex();

  Node* p = nodeFor(parent);
  if (!p || row >= (int)p->children.size())
    return QModelIndex();

  return createIndex(row, column, p->children[row]);
}

QModelIndex FileTreeModel::parent(const QModelIndex& child) const
{
  if (!child.isValid())
    return QModelIndex();

  Node* n = nodeFor(child);
  if (!n || !n->parent || n->parent == root_)
    return QModelIndex();

  Node* p = n->parent;
  Node* gp = p->parent;
  if (!gp)
    return QModelIndex();

  const auto it = std::find(gp->children.begin(), gp->children.end(), p);
  const int row = (it == gp->children.end()) ? 0 : (int)(it - gp->children.begin());
  return createIndex(row, 0, p);
}

int FileTreeModel::rowCount(const QModelIndex& parent) const
{
  if (parent.column() > 0)
    return 0;
  Node* p = nodeFor(parent);
  return p ? (int)p->children.size() : 0;
}

int FileTreeModel::columnCount(const QModelIndex&) const
{
  return 1;
}

QVariant FileTreeModel::data(const QModelIndex& index, int role) const
{
  if (!index.isValid())
    return QVariant();

  Node* n = nodeFor(index);
  if (!n)
    return QVariant();

  switch (role) {
    case Qt::DisplayRole:
      return n->name;
    case Qt::ToolTipRole:
      // Where the FileDataID went when it left the row label. Folders have none.
      return n->isFolder() ? QVariant()
                           : QVariant(QString::fromUtf8("FileDataID %1").arg(n->fileId));
    case Qt::ForegroundRole:
      // Folders muted, files in the normal text colour -- the design's hierarchy cue.
      return n->isFolder() ? QColor("#8a93a0") : QColor("#e8eaee");
    default:
      return QVariant();
  }
}
