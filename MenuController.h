#ifndef MENUCONTROLLER_H
#define MENUCONTROLLER_H

#include <QObject>
#include <QString>
#include <vector>

class ExportController;
class GLHost;
class GameFile;
class MainWindow;
class WoWModel;
class QAction;
class QMenu;

// The title bar's menus.
//
// The mock-up drew "Datei / Ansicht / Charakter / Export / Hilfe" as five labels; this
// is what actually sits behind them. It is a separate class from MainWindow because
// almost every action needs something MainWindow does not own and cannot own at
// construction time: the mounted game data, the item database, the loaded plugins.
//
// The character file format is the upstream one (SavedCharacter 2.0). Reading and
// writing it is nearly free here, because the parsing lives in wow.dll:
// WoWModel::save/load and WoWItem::save/load do the work, exactly as they do for the
// wx front-end -- so files written by either build open in the other.
class MenuController : public QObject
{
  Q_OBJECT

public:
  MenuController(MainWindow* win, GLHost* host, ExportController* exporters,
                 QObject* parent = nullptr);

  // Fills the menu bar MainWindow left empty, and connects the viewport's HUD buttons
  // to the same actions so the two entry points cannot drift apart.
  void build();

  // Enable/disable whatever depends on there being a (character) model. Call after
  // anything replaces the model.
  void modelChanged();

  // Also reachable from the viewport's "Exportieren" button.
  void exportWithDialog();

  // Import an armory character from a URL. Returns an empty string on success,
  // otherwise the reason. Separate from the menu action so it can be driven from the
  // command line (--armory), which is the only way to prove the import works without
  // clicking through a dialog. interactive=false suppresses the message boxes.
  // A trailing slash is normalised away -- the plugin's parser cannot read a link that
  // has one, and pasted links usually do.
  QString importArmory(const QString& rawUrl, bool interactive);

signals:
  // A model file the menu wants shown. main() owns the load path -- it also refreshes
  // the inspector panels and the timeline -- so the menu asks instead of doing it
  // itself. The connection is direct, so host->model() is the new model on return.
  void loadFileRequested(GameFile* file);

private:
  QMenu* addMenu(const QString& title);
  QAction* add(QMenu* menu, const QString& text, const QString& shortcut,
               void (MenuController::*slot)());

  // --- Datei
  void openByFileDataId();
  void takeScreenshot();
  void changeGameFolder();

  // --- Ansicht
  void chooseBackground();
  void applyPreset(int index);

  // --- Charakter
  void loadCharacter()       { loadCharacterFile(false); }
  void saveCharacter()       { saveCharacterFile(false); }
  void loadEquipment()       { loadCharacterFile(true); }
  void saveEquipment()       { saveCharacterFile(true); }
  void clearEquipment();
  void randomiseCharacter();
  void importArmoryCharacter();   // the dialog wrapper around importArmory()
  void importNpcFromUrl();

  // --- Hilfe
  void about();

  void loadCharacterFile(bool equipmentOnly);
  void saveCharacterFile(bool equipmentOnly);

  // Write the current character to `path` in the upstream SavedCharacter format.
  bool writeCharacterTo(const QString& path, bool equipmentOnly);

public slots:
  // Replace the loaded model with `modelPath`, carrying customization and equipment
  // across. Used by the orc posture option, which is a different model rather than a
  // different setting on the same one.
  void swapModelPreservingState(const QString& modelPath);

  // From the item browser. `standalone` asks for the item's own model on its own; only
  // head, shoulder, back and weapons have one, and the rest fall back to the mannequin
  // with a note rather than showing nothing.
  void showItem(int itemId, bool standalone);
  void showSet(int setId);

private:

  // Load the creature behind a Creature.ID, including its extended display info
  // (NPCs that are really dressed-up character models). Ported from
  // ModelViewer::LoadNPC.
  bool loadNpc(int creatureId, QString* error);

  WoWModel* characterModel() const;
  QString rememberedDir(const char* key) const;
  void rememberDir(const char* key, const QString& path);

  MainWindow* win_ = nullptr;
  GLHost* host_ = nullptr;
  ExportController* exporters_ = nullptr;

  void toggleGrid();

  // The display stand for texture-only armour. Loads a default character if the current
  // model is not one, and returns it (null if even that failed).
  WoWModel* ensureMannequin();

  // The item's own model file, if it has one. 0 for anything composed onto a body.
  int standaloneModelFor(int itemId, int* textureFileId, int* displayInfoId) const;

  QMenu* characterMenu_ = nullptr;
  QMenu* exportMenu_ = nullptr;
  QMenu* viewMenu_ = nullptr;          // also popped up by the toolbar's "Kamera"
  QAction* gridAction_ = nullptr;
  std::vector<QAction*> needsCharacter_;   // greyed out unless a character is loaded
  std::vector<QAction*> needsModel_;       // greyed out unless anything is loaded
  std::vector<QAction*> geosetToggles_;    // showUnderwear/showEars/... in order
};

#endif
