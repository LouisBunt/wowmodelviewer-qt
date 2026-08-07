#ifndef ITEMBROWSER_H
#define ITEMBROWSER_H

#include <vector>

#include <QString>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QTimer;

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
  // keepEquipment: leave what is currently worn and only add the set's pieces on
  // top. Travels in the signal because Qt connects cannot see default arguments.
  void setActivated(int setId, bool keepEquipment);

private:
  void refresh();
  void refreshItems();
  void refreshSets();
  void setMode(bool sets);

  // One result row -> one list entry. Split out because the rows are emitted twice:
  // straight through when a slot filter is set, and under slot headings when it is not.
  // `row` is ID, name, quality, item level, inventory type.
  void addItemRow(const std::vector<QString>& row);

  QComboBox* slot_ = nullptr;
  QComboBox* expansion_ = nullptr;
  QComboBox* quality_ = nullptr;
  QLineEdit* search_ = nullptr;
  QCheckBox* standalone_ = nullptr;
  QCheckBox* keepEquip_ = nullptr;   // sets mode only
  QListWidget* list_ = nullptr;
  QLabel* count_ = nullptr;
  QLabel* itemsChip_ = nullptr;
  QLabel* setsChip_ = nullptr;
  QTimer* searchDelay_ = nullptr;   // keeps every keystroke from hitting the database

  bool setMode_ = false;
  bool ready_ = false;

protected:
  bool eventFilter(QObject* obj, QEvent* e) override;
};

#endif
