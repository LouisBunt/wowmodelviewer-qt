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
  void onEvent(Event* e) override;

  // Public so a headless run can exercise the same path the id field uses.
  void equip(int itemId) { equipById(itemId); }

  // Re-read everything from the model. Needed after something OTHER than this panel
  // changed it -- loading a character file, an armory import, clearing equipment.
  void refresh();

  void clearEquipment();
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

private:
  void rebuild();
  void clearRows();
  void buildEquipment();
  void buildTabard();
  void refreshEquipment();
  void equipById(int itemId);

  WoWModel* model_ = nullptr;
  QVBoxLayout* rows_ = nullptr;
  QVBoxLayout* equipRows_ = nullptr;
  QLabel* header_ = nullptr;
  QLabel* subHeader_ = nullptr;
  QLabel* equipHeader_ = nullptr;
  QLabel* tabardHeader_ = nullptr;
  QCheckBox* dhMode_ = nullptr;
  QVBoxLayout* tabardRows_ = nullptr;
  std::vector<QSpinBox*> tabardSpins_;
  QLineEdit* itemInput_ = nullptr;
  std::vector<QComboBox*> combos_;
  std::vector<QLabel*> slotLabels_;   // one per CharSlots entry we show
  bool updating_ = false;      // guards against reacting to our own writes
};

#endif
