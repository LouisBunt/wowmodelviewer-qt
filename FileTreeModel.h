#ifndef FILETREEMODEL_H
#define FILETREEMODEL_H

#include <map>
#include <vector>

#include <QAbstractItemModel>
#include <QString>

class GameFile;

// The listfile hierarchy as a Qt item model.
//
// The wx front-end had to populate a wxTreeCtrl row by row, which meant lazy
// expansion (TreeStackItem::appendChildren), Freeze()/Thaw() around every fill and
// manual row striping -- building all ~130k rows eagerly took about nine seconds
// and dominated startup. A QAbstractItemModel is virtual by construction: the view
// only ever asks about the rows it is actually showing, so none of that machinery
// is needed here.
class FileTreeModel : public QAbstractItemModel
{
  Q_OBJECT

public:
  enum Category { All, Characters, Creatures, Items };

  struct Node
  {
    QString name;
    GameFile* file = nullptr;         // null for folders
    int fileId = 0;                   // set on race-browser leaves, which carry no GameFile
    Node* parent = nullptr;
    std::vector<Node*> children;      // kept sorted: folders first, then by name
    std::map<QString, Node*> byName;  // lookup while building

    ~Node() { for (Node* c : children) delete c; }
    bool isFolder() const { return file == nullptr && fileId == 0; }
  };

  explicit FileTreeModel(QObject* parent = nullptr);
  ~FileTreeModel() override;

  // Rebuild from the game directory. `extensionFilter` is an extension without the
  // dot ("m2", "blp", ...); `search` narrows by substring, empty means everything.
  // Returns the number of files matched.
  int rebuild(const QString& extensionFilter, const QString& search = QString());

  // The curated race browser: the raw character/ folder is unusable for picking a
  // character, so the wx front-end replaced it with a Playable/NPC race list. Built
  // from RaceInfos::getRaceMenu(), which already resolves each race+sex to its model.
  int buildRaceBrowser(const QString& search = QString());

  void setCategory(Category c) { category_ = c; }
  Category category() const { return category_; }

  GameFile* fileAt(const QModelIndex& index) const;
  int fileIdAt(const QModelIndex& index) const;

  // QAbstractItemModel
  QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
  QModelIndex parent(const QModelIndex& child) const override;
  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role) const override;

private:
  Node* nodeFor(const QModelIndex& index) const;
  void sortRecursive(Node* n);

  Node* root_ = nullptr;
  Category category_ = All;
};

#endif
