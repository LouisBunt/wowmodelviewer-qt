#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>

class GLHost;
class QLabel;

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

private:
  QWidget* buildTitleBar();
  QWidget* buildToolBar();
  QWidget* buildBrowser();
  QWidget* buildViewport();
  QWidget* buildTimeline();
  QWidget* buildInspector();
  QWidget* buildStatusBar();

  GLHost* canvas_ = nullptr;
  QLabel* buildLabel_ = nullptr;
  QLabel* pathLabel_ = nullptr;
  QLabel* statusPathLabel_ = nullptr;
};

#endif
