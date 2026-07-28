#ifndef CHARACTERPANEL_H
#define CHARACTERPANEL_H

#include <vector>

#include <QWidget>

#include "metaclasses/Observer.h"

class WoWModel;
class QComboBox;
class QLabel;
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

signals:
  void customizationChanged();

private:
  void rebuild();
  void clearRows();

  WoWModel* model_ = nullptr;
  QVBoxLayout* rows_ = nullptr;
  QLabel* header_ = nullptr;
  QLabel* subHeader_ = nullptr;
  std::vector<QComboBox*> combos_;
  bool updating_ = false;      // guards against reacting to our own writes
};

#endif
