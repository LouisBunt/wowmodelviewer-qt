#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <vector>

#include <QPoint>
#include <QWidget>

class CharacterPanel;
class FileTreeModel;
class GLHost;
class GameFile;
class ItemBrowser;
class NpcBrowser;
class LightPanel;
class QLabel;
class QLineEdit;
class QMenuBar;
class ElidedLabel;
class SegmentedBar;
class QSplitter;
class QToolButton;
class QPushButton;
class QCloseEvent;
class QTreeView;
class TimelinePanel;
class CharacterIoTab;
class QStackedWidget;

// The shell from the "WoW Model Viewer Redesign" mock-up, with the real GL canvas
// where the prototype had a placeholder.
//
// The window owns the chrome only: the frame, the theming, the browser tree and the
// inspector host. What goes INTO the menu bar comes from MenuController, and what the
// inspector's Export tab shows comes from main() -- both need the game data and the
// plugins, which do not exist yet while this constructor runs.
//
// The toolbar row under the title bar is still mock-up content: its six "Alt+n" items
// have no counterpart in this front-end yet.
class MainWindow : public QWidget
{
  Q_OBJECT

public:
  MainWindow();

  GLHost* canvas() const { return canvas_; }

  void setBuildLabel(const QString& text);
  // The data-source indicator in the title bar: a dot, a short label ("CASC · 12.1.0",
  // "CDN · 12.1.0 · offline") and the details in the tooltip. `degraded` turns the dot from
  // the ok colour to the warn colour -- the online mode without a connection.
  void setDataSource(const QString& label, const QString& tooltip, bool degraded);
  void setPathLabel(const QString& text);
  // The status bar's two fields. The long German sentences main() used to write into a
  // 30px tool-bar strip go here instead.
  void setStatus(const QString& text);
  void setSummary(const QString& text);

  // Re-set the brand line in a different family. Separate from construction because the
  // face we want lives in the game archives, which are not mounted when this window is
  // built. Does nothing if the family is empty, so the caller need not check.
  void setDisplayFont(const QString& family, int pointSize = -1);

  // Fill the browser from the game directory once CASC is mounted.
  void populateTree();
  void setCategory(int index);
  void setInspectorTab(int index);
  // Filled by main() once the exporters and the menu controller exist, the same
  // way the Export tab is.
  QWidget* characterIoHost() const { return characterIoHost_; }
  QWidget* exportHost() const { return exportHost_; }
  CharacterPanel* characterPanel() const { return charPanel_; }
  TimelinePanel* timeline() const { return timeline_; }

  // Empty when the window is built; MenuController fills it in once main() has the
  // game data and the plugins.
  QMenuBar* menuBar() const { return menuBar_; }
  ItemBrowser* itemBrowser() const { return itemBrowser_; }
  NpcBrowser* npcBrowser() const { return npcBrowser_; }

  // Move the highlight in the viewport's camera-preset row. Called from the menu too,
  // so the two ways of switching view do not drift apart.
  void setActiveCameraPreset(int index);

  // Inspector page order, so the toolbar and the rail cannot address the wrong one.
  enum InspectorTab { TabCharacter = 0, TabCharacterIo, TabLight, TabExport };

  // Read the measured frame rate off the canvas. Driven by main()'s UI timer.
  void updateStats();

  // Fill the status bar's format list from what actually loaded.
  void setExportFormats(const QStringList& labels);

  void setGridIndicator(bool on);

  // Grey out what cannot work without a model. The HUD's export button is a QLabel, not a
  // QAction, so MenuController's needsModel_ list cannot reach it -- it stayed fully lit on
  // an empty start and led straight into "Kein Modell geladen".
  void setModelActionsEnabled(bool on);

  // Reflect the item view's state in the toolbar. -1 = whole character.
  void setItemFocusIndicator(int slot);
  // Window size and splitter positions, remembered between runs. Public because the menu's
  // "Layout zurücksetzen" calls resetLayout(), and because saveLayout() runs on close.
  void saveLayout() const;
  void restoreLayout();
  void resetLayout();

private:
  // Window button look. Quiet at rest so the title bar stays calm; on hover the close
  // button turns red and the other two lift, which is what tells them apart at a glance.
  static void paintWindowButton(QLabel* b, bool hovered);

public:

signals:
  void fileActivated(GameFile* file);
  void fileIdActivated(int fileDataId);
  void exportRequested();
  void screenshotRequested();
  void cameraPresetRequested(int index);
  void cameraMenuRequested();
  void backgroundRequested();
  void fitCameraRequested();
  void gridToggleRequested();

  // The toolbar's view switch. true = show one piece only, false = the whole character.
  // MainWindow does not know which piece; whoever owns the CharacterPanel decides that.
  void itemViewRequested(bool onlyItem);

protected:
  bool eventFilter(QObject* obj, QEvent* e) override;
  void closeEvent(QCloseEvent* e) override;

private slots:
  void onTreeActivated(const QModelIndex& index);

private:
  void installShortcuts();
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
  QLabel* brandLabel_ = nullptr;     // "MIDNIGHT" -- the ornamental half of the wordmark
  QLabel* brandSub_ = nullptr;       // "ModelViewer" -- dropped first when the window narrows
  QLabel* brandVersion_ = nullptr;   // the version -- dropped second
  ElidedLabel* contextLabel_ = nullptr;  // what is loaded, once, in the title bar
  QLabel* buildLabel_ = nullptr;
  QLabel* sourceDot_ = nullptr;      // the indicator's dot, painted by setDataSource()
  QWidget* sourceStatus_ = nullptr;  // dot + label; carries the tooltip
  QLabel* pathLabel_ = nullptr;
  QLabel* exportButton_ = nullptr;   // unused since the export action moved to the tool bar
  QLabel* emptyHint_ = nullptr;      // shown over the viewport while nothing is loaded
  QLabel* viewCharChip_ = nullptr;   // toolbar: whole character
  QLabel* viewItemChip_ = nullptr;   // toolbar: only the focused piece
  QLabel* statusLabel_ = nullptr;        // "Bereit" and the short messages
  ElidedLabel* statusPathLabel_ = nullptr;  // a one-line summary of what is loaded
  QLabel* resultLabel_ = nullptr;
  QLineEdit* search_ = nullptr;
  QTreeView* tree_ = nullptr;
  FileTreeModel* treeModel_ = nullptr;
  // Toolbar and viewport-rail button ids, matched in eventFilter.
  enum ToolAction { ToolModel = 0, ToolCharacter, ToolLight, ToolCamera, ToolBackground };
  enum RailAction { RailFit = 0, RailGrid, RailLight };

  QMenuBar* menuBar_ = nullptr;
  LightPanel* lightPanel_ = nullptr;
  ItemBrowser* itemBrowser_ = nullptr;
  NpcBrowser* npcBrowser_ = nullptr;
  QStackedWidget* browserStack_ = nullptr;
  QWidget* searchWrap_ = nullptr;
  QLabel* fpsLabel_ = nullptr;
  QLabel* formatsLabel_ = nullptr;
  SegmentedBar* catBar_ = nullptr;        // the browser's five categories
  SegmentedBar* viewBar_ = nullptr;       // Charakter | Nur Teil
  SegmentedBar* camBar_ = nullptr;        // Vorn | 3/4 | Seite | Oben
  QToolButton* gridButton_ = nullptr;
  QToolButton* screenshotButton_ = nullptr;
  QPushButton* exportButton2_ = nullptr;  // the primary action, in the tool bar
  std::vector<QToolButton*> inspectorTabs_;
  QSplitter* columns_ = nullptr;          // browser | centre | inspector
  QSplitter* centre_ = nullptr;           // viewport over timeline
  QStackedWidget* inspectorStack_ = nullptr;
  QWidget* characterIoHost_ = nullptr;
  QWidget* exportHost_ = nullptr;
  QPoint dragOffset_;
  bool dragging_ = false;
};

#endif
