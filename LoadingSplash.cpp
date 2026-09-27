#include "LoadingSplash.h"

#include <QFontMetrics>
#include <QGuiApplication>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>
#include <QVariant>

#include "Theme.h"

namespace {
// The strip under the poster that carries the stage, the numbers, the way out and the bar. The
// poster is artwork with its own composition down to the last few pixels;
// QSplashScreen::showMessage() wrote the stage across its bottom edge. A strip in the chrome
// colour reads as what it is -- the status bar of the window that is about to appear.
//
//   Archiv-Verzeichnisse und Dateitabelle werden geladen ...
//   31 % . 0:08 . einmalig rund 400 MB                  [Abbrechen]
//   =================-------------------------------------------------
//
// The stage has a line of its own. Beside the button it lost a quarter of its room, and the
// longest stage of a first start no longer fitted even at 100 %; the numbers are shorter, and
// the end of them is what may go first.
int stageHeight() { return ui::px(20); }
int bandHeight()
{
  return met::sp(met::Gap) + stageHeight() + met::hCtl() + met::sp(met::Snug) + ui::px(4)
         + met::sp(met::Pad);
}
// Where the stage line, the row of numbers and button, and the bar go, for a splash of w x h.
struct BandLayout
{
  QRect stage, row, bar;
  BandLayout(int w, int h)
  {
    const int side = met::sp(met::Edge);
    const int top = h - bandHeight() + met::sp(met::Gap);
    stage = QRect(side, top, qMax(0, w - 2 * side), stageHeight());
    row = QRect(side, stage.bottom() + 1, stage.width(), met::hCtl());
    bar = QRect(side, row.bottom() + 1 + met::sp(met::Snug), stage.width(), ui::px(4));
  }
};
}

LoadingSplash* LoadingSplash::create()
{
  // Compiled in via the Qt resource system so the exe stays self-contained. The drawn
  // fallback only exists so a build without resources still shows SOMETHING.
  QPixmap poster(":/splash.png");
  if (poster.isNull()) {
    poster = QPixmap(420, 160);
    poster.fill(QColor(tok::bgRaised));
    QPainter p(&poster);
    p.setPen(QColor(tok::lineBorder));
    p.drawRect(0, 0, poster.width() - 1, poster.height() - 1);
    p.setPen(QColor(tok::accent));
    QFont f = typo::font(typo::Display);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 2.0);
    p.setFont(f);
    p.drawText(QRect(0, 40, poster.width(), 30), Qt::AlignCenter, "MODEL VIEWER");
    p.end();
  }

  // The strip's text is measured in ui::px(), the poster is a fixed bitmap. Scaled together,
  // or at 150 % the stage line has a third less room than it was written for -- capped so the
  // poster never takes more than two thirds of the screen height.
  const int designWidth = ui::px(poster.width());
  double f = ui::scale();
  if (const QScreen* screen = QGuiApplication::primaryScreen())
    f = qMin(f, 0.66 * screen->availableGeometry().height() / qMax(1, poster.height()));
  if (f > 1.01)
    poster = poster.scaled(poster.size() * f, Qt::KeepAspectRatio, Qt::SmoothTransformation);

  // Where the cap holds the poster back (150 % on a 1080p screen), the strip still gets the
  // width its text was written for, and the poster's own backdrop -- one translucent black to
  // its very edges -- is carried out to meet it, so the wider window shows no seam.
  const int width = qMax(poster.width(), designWidth);
  QPixmap pm(width, poster.height() + bandHeight());
  pm.fill(QColor(tok::bgChrome));
  QPainter p(&pm);
  if (width > poster.width())
    p.fillRect(QRect(0, 0, width, poster.height()), poster.toImage().pixelColor(0, 0));
  p.drawPixmap((width - poster.width()) / 2, 0, poster);
  p.fillRect(QRect(0, poster.height(), pm.width(), 1), QColor(tok::lineBorder));
  p.end();
  return new LoadingSplash(pm);
}

LoadingSplash::LoadingSplash(const QPixmap& pm) : QSplashScreen(pm)
{
  bar_ = new QProgressBar(this);
  bar_->setTextVisible(false);
  bar_->setRange(0, 1000);
  bar_->setFixedHeight(ui::px(4));
  bar_->hide();

  cancel_ = new QPushButton(QString::fromUtf8("Abbrechen"), this);
  cancel_->setProperty("variant", "quiet");
  cancel_->setCursor(Qt::PointingHandCursor);
  cancel_->hide();
  connect(cancel_, &QPushButton::clicked, this, [this]() {
    // Gone rather than greyed: the sheet's quiet variant looks the same enabled and disabled,
    // and a second click would have nothing to add. The engine stops at its next progress
    // report -- inside the one large request there is none, so this can take a while.
    cancelRequested_ = true;
    cancel_->hide();
    stage_ = QString::fromUtf8("Wird abgebrochen …");
    repaint();
  });
  place();
}

void LoadingSplash::place()
{
  const BandLayout band(width(), height());
  bar_->setGeometry(band.bar);
  cancel_->adjustSize();
  cancel_->move(band.row.right() + 1 - cancel_->width(),
                band.row.top() + (band.row.height() - cancel_->height()) / 2);
}

void LoadingSplash::resizeEvent(QResizeEvent* e)
{
  QSplashScreen::resizeEvent(e);
  place();
}

void LoadingSplash::setStage(const QString& text)
{
  if (cancelRequested_ && cancellable_)
    return;                       // "Wird abgebrochen" stays up until the open has returned
  stage_ = text;
  repaint();
}

void LoadingSplash::setDetail(const QString& text)
{
  if (text == detail_)
    return;
  detail_ = text;
  repaint();
}

void LoadingSplash::setProgress(double fraction)
{
  if (fraction < 0.0) {
    bar_->hide();
    return;
  }
  bar_->setValue((int)qBound(0.0, fraction * 1000.0, 1000.0));
  bar_->show();
}

void LoadingSplash::setCancellable(bool on)
{
  cancellable_ = on;
  cancel_->setVisible(on && !cancelRequested_);
  repaint();
}

void LoadingSplash::drawContents(QPainter* p)
{
  const BandLayout band(width(), height());
  const QFont body = typo::font(typo::Body);
  const QFont mono = typo::font(typo::Mono);

  p->setPen(QColor(tok::fgSoft));
  p->setFont(body);
  p->drawText(band.stage, Qt::AlignLeft | Qt::AlignVCenter,
              QFontMetrics(body).elidedText(stage_, Qt::ElideRight, band.stage.width()));
  if (!detail_.isEmpty()) {
    // The numbers stop short of the button while it is shown, rather than running under it.
    QRect detail = band.row;
    if (cancel_->isVisible())
      detail.setRight(cancel_->x() - met::sp(met::Gap));
    p->setPen(QColor(tok::fgMuted));
    p->setFont(mono);
    p->drawText(detail, Qt::AlignLeft | Qt::AlignVCenter,
                QFontMetrics(mono).elidedText(detail_, Qt::ElideRight, detail.width()));
  }
}

void LoadingSplash::mousePressEvent(QMouseEvent*)
{
  // QSplashScreen hides itself on any click. With a download underway that left nothing on
  // screen for minutes; the splash now stays until finish().
}
