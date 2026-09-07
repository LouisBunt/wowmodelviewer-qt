#include "MidnightStyle.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QEvent>
#include <QFocusEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOption>
#include <QWidget>

#include "Theme.h"

namespace {

// The keyboard-focus watcher. One instance on qApp, so no widget has to opt in.
//
// Qt reports HOW focus arrived (QFocusEvent::reason). Tab, Backtab and shortcuts are the
// keyboard; a mouse press is not. The property drives the *[kbfocus="true"] rule in the sheet.
class FocusWatcher : public QObject
{
public:
  explicit FocusWatcher(QObject* parent) : QObject(parent) {}

protected:
  bool eventFilter(QObject* o, QEvent* e) override
  {
    QWidget* w = qobject_cast<QWidget*>(o);
    if (!w)
      return false;
    if (e->type() == QEvent::FocusIn) {
      const Qt::FocusReason r = static_cast<QFocusEvent*>(e)->reason();
      const bool keyboard = (r == Qt::TabFocusReason || r == Qt::BacktabFocusReason
                             || r == Qt::ShortcutFocusReason);
      if (keyboard && !w->property("kbfocus").toBool()) {
        w->setProperty("kbfocus", true);
        Theme::repolish(w);
      }
    } else if (e->type() == QEvent::FocusOut) {
      if (w->property("kbfocus").toBool()) {
        w->setProperty("kbfocus", false);
        Theme::repolish(w);
      }
    }
    return false;
  }
};

}  // namespace

MidnightStyle::MidnightStyle(QStyle* base) : QProxyStyle(base)
{
}

void MidnightStyle::polish(QWidget* w)
{
  QProxyStyle::polish(w);
  if (!w)
    return;
  // Installed once, on the application, the first time any widget is polished.
  static QObject* watcher = nullptr;
  if (!watcher && w->window()) {
    watcher = new FocusWatcher(qApp);
    qApp->installEventFilter(watcher);
  }
}

void MidnightStyle::drawPrimitive(PrimitiveElement pe, const QStyleOption* opt, QPainter* p,
                                  const QWidget* w) const
{
  if (pe == PE_IndicatorBranch) {
    // The tree's disclosure marker. Fusion draws a small filled triangle in the palette's mid
    // colour, which on a near-black ground is a grey smudge. A stroked chevron reads at every
    // scale and matches the icon set's 2px stroke.
    const bool hasChildren = opt->state & State_Children;
    if (!hasChildren) {
      return;  // no dotted "branch line" -- the indent carries the structure
    }
    const bool open = opt->state & State_Open;
    const bool hover = opt->state & State_MouseOver;
    const QRectF r = opt->rect;
    const qreal s = qMin(r.width(), r.height()) * 0.26;
    const QPointF c = r.center();

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    QPen pen(QColor(hover ? tok::fgSoft : tok::fgDim));
    pen.setWidthF(qMax(1.4, ui::scale() * 1.6));
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p->setPen(pen);
    QPainterPath path;
    if (open) {
      // pointing down
      path.moveTo(c.x() - s, c.y() - s * 0.5);
      path.lineTo(c.x(), c.y() + s * 0.5);
      path.lineTo(c.x() + s, c.y() - s * 0.5);
    } else {
      // pointing right
      path.moveTo(c.x() - s * 0.5, c.y() - s);
      path.lineTo(c.x() + s * 0.5, c.y());
      path.lineTo(c.x() - s * 0.5, c.y() + s);
    }
    p->drawPath(path);
    p->restore();
    return;
  }

  if (pe == PE_FrameFocusRect) {
    // Suppressed on purpose: the ring is painted by the stylesheet through the kbfocus
    // property, which also knows whether the keyboard put the focus there. Letting the base
    // style draw its dotted rectangle as well would double it.
    return;
  }

  QProxyStyle::drawPrimitive(pe, opt, p, w);
}

int MidnightStyle::pixelMetric(PixelMetric pm, const QStyleOption* opt, const QWidget* w) const
{
  switch (pm) {
    case PM_ScrollBarExtent:        return ui::px(10);
    case PM_ScrollBarSliderMin:     return ui::px(28);
    case PM_SmallIconSize:          return ui::px(16);
    case PM_ButtonIconSize:         return ui::px(16);
    case PM_ToolBarIconSize:        return ui::px(18);
    case PM_TreeViewIndentation:    return ui::px(15);
    case PM_DefaultFrameWidth:      return 1;
    case PM_SplitterWidth:          return ui::px(4);
    case PM_MenuButtonIndicator:    return ui::px(14);
    case PM_FocusFrameHMargin:
    case PM_FocusFrameVMargin:      return 0;
    // The layout grid, so hand-built layouts and Qt's own defaults agree.
    case PM_LayoutLeftMargin:
    case PM_LayoutRightMargin:
    case PM_LayoutTopMargin:
    case PM_LayoutBottomMargin:     return met::sp(met::Edge);
    case PM_LayoutHorizontalSpacing:
    case PM_LayoutVerticalSpacing:  return met::sp(met::Gap);
    default:                        return QProxyStyle::pixelMetric(pm, opt, w);
  }
}

int MidnightStyle::styleHint(StyleHint sh, const QStyleOption* opt, const QWidget* w,
                             QStyleHintReturn* ret) const
{
  switch (sh) {
    // A list, not a native drop-down: the popup is styled by the sheet and has to be a plain
    // view for those rules to apply.
    case SH_ComboBox_Popup:            return 0;
    case SH_ToolTip_WakeUpDelay:       return 350;
    case SH_ToolTip_FallAsleepDelay:   return 200;
    case SH_Menu_SubMenuPopupDelay:    return 120;
    // Focus is drawn by the sheet; the base style's dotted rectangle would be a second one.
    case SH_FocusFrame_AboveWidget:    return 0;
    case SH_UnderlineShortcut:         return 1;
    case SH_ItemView_ShowDecorationSelected: return 1;
    default:                           return QProxyStyle::styleHint(sh, opt, w, ret);
  }
}
