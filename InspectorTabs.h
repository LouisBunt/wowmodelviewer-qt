#ifndef INSPECTORTABS_H
#define INSPECTORTABS_H

#include <vector>

#include <QWidget>

class ExportController;
class GLHost;
class WoWModel;
class QCheckBox;
class QComboBox;
class QLabel;
class QVBoxLayout;

// The "Material" tab: the model's geosets with visibility checkboxes, and its
// texture list.
//
// The wx geoset tree expressed visibility through the ROW BACKGROUND COLOUR
// (*wxGREEN) and toggled it on double-click -- unreadable under a dark theme and
// undiscoverable in any theme. Checkboxes say the same thing plainly.
class MaterialTab : public QWidget
{
  Q_OBJECT
public:
  explicit MaterialTab(QWidget* parent = nullptr);
  void setModel(WoWModel* model);

private:
  void rebuild();

  WoWModel* model_ = nullptr;
  QVBoxLayout* rows_ = nullptr;
  QLabel* header_ = nullptr;
  QLabel* textureInfo_ = nullptr;
  bool updating_ = false;
};

// The "Export" tab: format, options, and the button.
class ExportTab : public QWidget
{
  Q_OBJECT
public:
  explicit ExportTab(ExportController* exporters, GLHost* canvas, QWidget* parent = nullptr);
  void refreshFormats();

private:
  ExportController* exporters_ = nullptr;
  GLHost* canvas_ = nullptr;
  QComboBox* format_ = nullptr;
  QCheckBox* optMesh_ = nullptr;
  QCheckBox* optSkeleton_ = nullptr;
  QCheckBox* optSkinning_ = nullptr;
  QCheckBox* optAnimation_ = nullptr;
  QLabel* status_ = nullptr;
};

#endif
