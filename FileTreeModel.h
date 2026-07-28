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
  struct Node
  {
    QString name;
    GameFile* file = nullptr;         // null for folders
    Node* parent = nullptr;
    std::vector<Node*> children;      // kept sorted: folders first, then by name
    std::map<QString, Node*> byName;  // lookup while building

    ~Node() { for (Node* c : children) delete c; }
    bool isFolder() const { return file == nullptr; }
  };

  explicit FileTreeModel(QObject* parent = nullptr);
  ~FileTreeModel() override;

  // Rebuild from the game directory. `extensionFilter` is an extension without the
  // dot ("m2", "blp", ...); `search` narrows by substring, empty means everything.
  // Returns the number of files matched.
  int rebuild(const QString& extensionFilter, const QString& search = QString());

  GameFile* fileAt(const QModelIndex& index) const;

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
};

#endif
