#ifndef NPCBROWSER_H
#define NPCBROWSER_H

#include <QString>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;

// The "NPCs" side of the browser column.
//
// The file tree cannot answer "show me Hogger": it lists MODEL FILES, and one .m2 under
// creature/ serves dozens of differently named, differently skinned NPCs. The names live
// in the creature DATABASE, so browsing NPCs means searching that by name and letting the
// query walk Creature -> CreatureDisplayInfo -> CreatureModelData down to the .m2. The
// panel only finds and names things; loading the model, its skin textures and geosets is
// the receiver's job, reached through npcActivated.
class NpcBrowser : public QWidget
{
  Q_OBJECT

public:
  explicit NpcBrowser(QWidget* parent = nullptr);

  // Fills the type filter from CreatureType and runs the first query. Call once the
  // database is up.
  void initialise();

  // Where a display id's preview image lives once one exists. Static because the writer
  // of these files (the viewer, on first showing an NPC) needs the SAME path without
  // holding a browser instance -- one function, no chance of the two sides drifting.
  static QString thumbPath(int displayId);

  // Called after the viewer captured a new preview: gives every visible row with this
  // display id the image it was listed without. Push, not poll -- the panel never has
  // to watch the directory.
  void refreshThumb(int displayId);

signals:
  // Fired on double-click and on Enter in the search field when exactly one NPC matched.
  // Carries everything the viewer needs so the receiver does not have to repeat the
  // three-table join this panel already ran.
  void npcActivated(int creatureId, int displayId, int fileDataId, const QString& name);

private:
  void refresh();

  // Icon and tooltip for one row, from whatever userSettings/npc-thumbs holds right
  // now. Split out because it runs twice: when the row is built, and again from
  // refreshThumb once a preview appears.
  void decorate(QListWidgetItem* item, int displayId);
  void emitRow(QListWidgetItem* item);

  QLineEdit* search_ = nullptr;
  QComboBox* type_ = nullptr;    // CreatureType: Bestie, Humanoid, ...
  QListWidget* list_ = nullptr;
  QLabel* count_ = nullptr;

  bool ready_ = false;
};

#endif
