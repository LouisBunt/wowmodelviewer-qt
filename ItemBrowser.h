#ifndef ITEMBROWSER_H
#define ITEMBROWSER_H

#include <vector>

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;

// The "Items" side of the browser column.
//
// The file tree is the wrong tool for transmog: it lists MODEL FILES, and most armour has
// no model file of its own -- chest, legs, gloves, belt, boots, shirt and tabard are
// texture layers composed onto a character body. Browsing them means browsing the item
// DATABASE instead, which is what this does: filter by slot, expansion, quality and name,
// or switch to the game's own item sets.
//
// Requires ItemSparse.ExpansionID and OverallQualityID, which only became readable once
// the sparse-record walk in wow.dll could reach past the leading strings.
class ItemBrowser : public QWidget
{
  Q_OBJECT

public:
  explicit ItemBrowser(QWidget* parent = nullptr);

  // Fills the filter combos and runs the first query. Call once the item database is up.
  void initialise();

signals:
  // standalone: show the item's own model on its own. Only meaningful for the slots that
  // HAVE one (head, shoulder, back, weapons); the receiver reports when they do not.
  void itemActivated(int itemId, bool standalone);
  void setActivated(int setId);

private:
  void refresh();
  void refreshItems();
  void refreshSets();
  void setMode(bool sets);

  QComboBox* slot_ = nullptr;
  QComboBox* expansion_ = nullptr;
  QComboBox* quality_ = nullptr;
  QLineEdit* search_ = nullptr;
  QCheckBox* standalone_ = nullptr;
  QListWidget* list_ = nullptr;
  QLabel* count_ = nullptr;
  QLabel* itemsChip_ = nullptr;
  QLabel* setsChip_ = nullptr;

  bool setMode_ = false;
  bool ready_ = false;

protected:
  bool eventFilter(QObject* obj, QEvent* e) override;
};

#endif
