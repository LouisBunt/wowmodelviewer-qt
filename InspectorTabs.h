#ifndef INSPECTORTABS_H
#define INSPECTORTABS_H

#include <vector>

#include <QWidget>

class ExportController;
class GLHost;
class MenuController;
class WoWModel;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QVBoxLayout;

// The "Charakter" tool button's tab: where a character comes IN and where he goes OUT.
//
// Everything here already existed in the menu bar, and only there -- a menu is where
// one looks for a command one already knows about, not where one discovers that this
// program can read a Wowhead link at all. The import fields are the reason this tab
// exists; loading, saving and the one-click Blender export follow the same character
// around and belong next to them.
//
// Holds no logic of its own: every button calls the MenuController function the menu
// entry calls, so the two entry points cannot drift apart.
class CharacterIoTab : public QWidget
{
  Q_OBJECT
public:
  CharacterIoTab(MenuController* menus, ExportController* exporters, GLHost* canvas,
                 QWidget* parent = nullptr);

private:
  void importWowhead();
  void importArmory();
  void exportForBlender();
  void setStatus(const QString& text, bool error);

  // Index into ExportController::formats() of the FBX exporter, or -1 when the plugin
  // is not there -- then the Blender button says so instead of failing on click.
  int fbxFormatIndex() const;

  MenuController* menus_ = nullptr;
  ExportController* exporters_ = nullptr;
  GLHost* canvas_ = nullptr;
  QLineEdit* wowheadUrl_ = nullptr;
  QLineEdit* armoryUrl_ = nullptr;
  QLabel* status_ = nullptr;
};

// The "Export" tab: format, options, animation clips, and the button.
//
// The four option checkboxes used to be decoration -- ExportController hardcoded the
// combination it passed to the plugin. They drive the export now, and switching
// "Animationen" on reveals the clip list, which is the part the wx front-end put in a
// separate dialog (AnimationExportChoiceDialog).
class ExportTab : public QWidget
{
  Q_OBJECT
public:
  explicit ExportTab(ExportController* exporters, GLHost* canvas, QWidget* parent = nullptr);
  void refreshFormats();

  // Re-read the animation list off the current model. Called when the clip list becomes
  // visible, so a model loaded after this tab was built still lists its own clips.
  void refreshClips();

private:
  ExportController* exporters_ = nullptr;
  GLHost* canvas_ = nullptr;
  QComboBox* format_ = nullptr;
  QCheckBox* optMesh_ = nullptr;
  QCheckBox* optSkeleton_ = nullptr;
  QCheckBox* optSkinning_ = nullptr;
  QCheckBox* optAnimation_ = nullptr;
  QListWidget* clipList_ = nullptr;
  QLabel* clipHint_ = nullptr;
  QLabel* status_ = nullptr;
};

#endif
