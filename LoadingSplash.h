#ifndef LOADINGSPLASH_H
#define LOADINGSPLASH_H

#include <QSplashScreen>

class QProgressBar;
class QPushButton;

// The start-up poster, with room for a download.
//
// A local start only ever needed a line of text: every stage takes seconds. The online mode's
// first start moves several hundred MB before the window can exist, and a sentence that does
// not change for three minutes reads exactly like a hang. So the splash grows a strip under
// the poster with the stage, a second line for numbers ("37 % · 1:12"), the sheet's thin
// QProgressBar and, while something is being downloaded, a way out.
//
// Two deliberate departures from QSplashScreen: a click no longer hides it (a download that
// takes minutes must not leave the user with nothing on screen), and it never processes
// events by itself -- the caller decides when, exactly as splashStage() did.
class LoadingSplash : public QSplashScreen
{
  Q_OBJECT
public:
  // The branded poster from the resources, or a drawn stand-in when a build has none.
  static LoadingSplash* create();

  void setStage(const QString& text);
  void setDetail(const QString& text);
  // 0..1 shows the bar at that fraction; below 0 hides it.
  void setProgress(double fraction);
  void setCancellable(bool on);
  bool cancelRequested() const { return cancelRequested_; }

protected:
  void drawContents(QPainter* painter) override;
  void mousePressEvent(QMouseEvent* e) override;
  void resizeEvent(QResizeEvent* e) override;

private:
  explicit LoadingSplash(const QPixmap& pm);
  void place();

  QString stage_;
  QString detail_;
  QProgressBar* bar_ = nullptr;
  QPushButton* cancel_ = nullptr;
  bool cancelRequested_ = false;
  bool cancellable_ = false;
};

#endif  // LOADINGSPLASH_H
