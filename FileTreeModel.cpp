#include "FileTreeModel.h"

#include <algorithm>
#include <set>

#include <QColor>
#include <QFont>
#include <QRegularExpression>

#include "Game.h"
#include "GameFile.h"
#include "RaceInfos.h"

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
  const QString needle = search.trimmed().toLower();
  const QString pattern = "^.*" + QRegularExpression::escape(needle) + ".*\\." + extensionFilter;

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
    ++kept;

    Node* cur = root_;
    for (int i = 0; i < parts.size() - 1; ++i) {
      auto it = cur->byName.find(parts[i]);
      Node* child;
      if (it == cur->byName.end()) {
        child = new Node;
        child->name = parts[i];
        child->parent = cur;
        cur->children.push_back(child);
        cur->byName[parts[i]] = child;
      } else {
        child = it->second;
      }
      cur = child;
    }

    Node* leaf = new Node;
    // Show the file id alongside the name, as the wx tree did.
    leaf->name = QString("%1  [%2]").arg(parts.last()).arg(f->fileDataId());
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
    case Qt::ForegroundRole:
      // Folders muted, files in the normal text colour -- the design's hierarchy cue.
      return n->isFolder() ? QColor("#8a93a0") : QColor("#e8eaee");
    default:
      return QVariant();
  }
}
