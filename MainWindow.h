#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <vector>

#include <QPoint>
#include <QWidget>

class CharacterPanel;
class FileTreeModel;
class GLHost;
class GameFile;
class QLabel;
class QLineEdit;
class QTreeView;
class TimelinePanel;
class MaterialTab;
class QStackedWidget;

// The shell from the "WoW Model Viewer Redesign" mock-up, with the real GL canvas
// where the prototype had a placeholder.
//
// The browser column and the inspector are still static mock content -- they become
// real in Phases 4 and 5. What is real here is the frame, the theming and the
// viewport.
class MainWindow : public QWidget
{
  Q_OBJECT

public:
  MainWindow();

  GLHost* canvas() const { return canvas_; }

  void setBuildLabel(const QString& text);
  void setPathLabel(const QString& text);

  // Fill the browser from the game directory once CASC is mounted.
  void populateTree();
  void setCategory(int index);
  void setInspectorTab(int index);
  MaterialTab* materialTab() const { return materialTab_; }
  QWidget* exportHost() const { return exportHost_; }
  CharacterPanel* characterPanel() const { return charPanel_; }
  TimelinePanel* timeline() const { return timeline_; }

signals:
  void fileActivated(GameFile* file);
  void fileIdActivated(int fileDataId);
  void exportRequested();

protected:
  bool eventFilter(QObject* obj, QEvent* e) override;

private slots:
  void onTreeActivated(const QModelIndex& index);
  void onSearchChanged(const QString& text);

private:
  QWidget* buildTitleBar();
  QWidget* buildToolBar();
  QWidget* buildBrowser();
  QWidget* buildViewport();
  QWidget* buildTimeline();
  QWidget* buildInspector();
  QWidget* buildStatusBar();

  GLHost* canvas_ = nullptr;
  CharacterPanel* charPanel_ = nullptr;
  TimelinePanel* timeline_ = nullptr;
  QLabel* buildLabel_ = nullptr;
  QLabel* pathLabel_ = nullptr;
  QLabel* statusPathLabel_ = nullptr;
  QLabel* resultLabel_ = nullptr;
  QLineEdit* search_ = nullptr;
  QTreeView* tree_ = nullptr;
  FileTreeModel* treeModel_ = nullptr;
  std::vector<QLabel*> catChips_;
  std::vector<QLabel*> inspectorTabs_;
  QStackedWidget* inspectorStack_ = nullptr;
  MaterialTab* materialTab_ = nullptr;
  QWidget* exportHost_ = nullptr;
  QPoint dragOffset_;
  bool dragging_ = false;
};

#endif
