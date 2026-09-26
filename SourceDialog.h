#ifndef SOURCEDIALOG_H
#define SOURCEDIALOG_H

#include <QDialog>

#include "GameSource.h"

class ElidedLabel;
class QComboBox;
class QLabel;
class QPushButton;

// "Woher sollen die Spieldaten kommen?" -- the one place the data source is chosen.
//
// Shown on the first start when no installation is found, instead of going straight into a
// folder picker -- which was a dead end for anyone without WoW on this machine -- and from
// Datei > "Spieldaten-Quelle …" later. Both uses are the same dialog, so the question is
// always asked with the same words and the same two answers side by side.
//
// Each answer is one button that finishes the dialog: "WoW-Ordner wählen …" (the folder
// picker, accepted as soon as a real installation is picked) and "Online laden". There is no
// radio-plus-OK step in between, which is where a two-choice dialog loses people.
//
// Styled entirely by the application sheet: roles and variants, fonts from typo::, measures
// from met::. It sets no colour and no stylesheet of its own.
class SourceDialog : public QDialog
{
  Q_OBJECT
public:
  enum Mode {
    FirstRun,   // before the window exists; the way out is "Beenden"
    Switch      // from the menu; the way out is "Schließen", and a choice applies next start
  };

  // `current`: the source in use (Switch) or what the settings remember (FirstRun).
  // `cacheInUse`: this process has current.cacheDir open, so clearing it has to wait for the
  // next start.
  SourceDialog(Mode mode, const GameSource& current, bool cacheInUse, QWidget* parent = nullptr);

  // Valid once exec() returned Accepted.
  GameSource chosen() const { return chosen_; }

  // The folder picker with the .build.info check, shared by this dialog, the MVLink addon
  // install and the start-up error box. Empty return means cancelled.
  static QString askForWoWFolder(QWidget* parent, const QString& start);

private:
  void chooseLocal();
  void chooseOnline();
  void pickCacheDir();
  void clearCacheClicked();
  void measureCache();
  void showCacheSize(const QString& dir, quint64 bytes, int files);
  void updateOnlineButton();
  GameSource onlineChoice() const;

  Mode mode_;
  GameSource current_;
  GameSource chosen_;
  QString cacheDir_;          // the cache the dialog currently shows
  QString inUseDir_;          // the cache this process opened, if any

  ElidedLabel* folderLabel_ = nullptr;
  QComboBox* region_ = nullptr;
  QComboBox* locale_ = nullptr;
  ElidedLabel* cacheLabel_ = nullptr;
  ElidedLabel* cacheSize_ = nullptr;
  QPushButton* clearBtn_ = nullptr;
  QPushButton* onlineBtn_ = nullptr;
};

#endif  // SOURCEDIALOG_H
