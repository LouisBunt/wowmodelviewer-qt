#ifndef CHARACTERPANEL_H
#define CHARACTERPANEL_H

#include <vector>

#include <QWidget>

#include "metaclasses/Observer.h"

class WoWModel;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QVBoxLayout;

// The character customization column.
//
// Mirrors what CharDetailsFrame did under wx: list every ChrCustomizationOption of
// the model's ChrModel and give each one a picker bound to CharDetails.
//
// Observer lifetime needs no special care here -- Observer::~Observer() detaches
// from every observable it is attached to, Observable::~Observable() does the
// reverse, and Observable::notify() iterates a snapshot with a re-check so an
// observer destroyed mid-notification is skipped rather than called through a
// dangling pointer.
class CharacterPanel : public QWidget, public Observer
{
  Q_OBJECT

public:
  explicit CharacterPanel(QWidget* parent = nullptr);

  void setModel(WoWModel* model);

  // The geoset switches take EVERY model, not only characters -- a creature has geosets
  // too, and the tab they used to live in was given every model. Kept apart from
  // setModel() for exactly that reason: that one is character-only and is handed a null
  // for a creature, which would otherwise take the switches away with it.
  void setGeosetModel(WoWModel* model);

  void onEvent(Event* e) override;

  // Public so a headless run can exercise the same path the id field uses.
  void equip(int itemId) { equipById(itemId); }

  // Re-read everything from the model. Needed after something OTHER than this panel
  // changed it -- loading a character file, an armory import, clearing equipment.
  void refresh();

  void clearEquipment();

  // Take ONE piece off. Public for the same reason equip() is: a headless run can
  // then prove the path without clicking the row's "x".
  void unequipSlot(int slot);

  // Item view: show ONLY the piece in `slot`, hiding the figure and every other item;
  // slot < 0 restores the whole character. See the implementation for why this beats
  // loading the item's model on its own.
  void setItemFocus(int slot);
  int itemFocus() const { return focusSlot_; }

  // True when the piece worn in `slot` has geometry of its own -- the precondition for
  // the item view to show anything at all.
  bool slotHasOwnModel(int slot) const;

  void randomise();

  // Public so the armory import can run the same check: it applies customizations
  // through CharDetails directly rather than through this panel's pickers.
  //
  // CAUTION: on a match this emits postureModelRequested, whose handler REPLACES the
  // model -- deleting the current one. Call it only when nothing is still holding a
  // WoWModel pointer.
  void checkPostureVariant(uint choiceId);

  WoWModel* model() const { return model_; }

signals:
  void customizationChanged();

  // A customization choice that needs a DIFFERENT model file than the one loaded --
  // the orc's upright posture is the only one in the game data. The panel cannot load
  // models itself, so it names the file and lets the owner do the swap.
  void postureModelRequested(const QString& modelPath);

  // The item view was switched on or off; the owner reframes the camera on what is
  // left visible. The panel cannot do it itself -- the camera belongs to the viewport.
  void itemFocusChanged(int slot);

private:
  void rebuild();
  void clearRows();
  void buildEquipment();
  void buildTabard();
  void refreshEquipment();
  void equipById(int itemId);
  void buildGeosets();

  WoWModel* model_ = nullptr;
  WoWModel* geosetModel_ = nullptr;
  QVBoxLayout* rows_ = nullptr;
  QVBoxLayout* equipRows_ = nullptr;
  QLabel* header_ = nullptr;
  QLabel* subHeader_ = nullptr;
  QLabel* equipHeader_ = nullptr;
  QLabel* tabardHeader_ = nullptr;
  QCheckBox* dhMode_ = nullptr;
  QLabel* geosetHeader_ = nullptr;
  QVBoxLayout* geosetRows_ = nullptr;
  QVBoxLayout* tabardRows_ = nullptr;
  std::vector<QSpinBox*> tabardSpins_;
  QLineEdit* itemInput_ = nullptr;
  std::vector<QComboBox*> combos_;
  std::vector<QLabel*> slotLabels_;   // one per CharSlots entry we show
  std::vector<QLabel*> clearButtons_; // the per-row "x", visible only when worn
  std::vector<QLabel*> focusButtons_; // the per-row eye: show only this piece
  int focusSlot_ = -1;                // -1 = whole character visible
  bool updating_ = false;      // guards against reacting to our own writes

protected:
  bool eventFilter(QObject* obj, QEvent* e) override;
};

#endif
