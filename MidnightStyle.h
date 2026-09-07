#ifndef MIDNIGHTSTYLE_H
#define MIDNIGHTSTYLE_H

#include <QProxyStyle>

// What the stylesheet cannot reach.
//
// Qt's style sheets stop at a handful of things: the tree view's branch indicators are drawn by
// the style, not by QSS; the pixel metrics that decide scrollbar width, icon size and tree
// indentation come from the style; and the focus rectangle is a primitive. This proxy sits over
// Fusion and supplies exactly those, so the rest can stay declarative in Theme::sheet().
//
// It also decides WHEN a focus ring is shown: only for a widget reached with the keyboard. Qt
// gives every clicked button focus as well, and a ring that appears on click reads as a stuck
// selection. The application filter sets the dynamic property "kbfocus" on tab-focus, and the
// stylesheet paints the ring; this class installs the filter.
class MidnightStyle : public QProxyStyle
{
  Q_OBJECT
public:
  explicit MidnightStyle(QStyle* base);

  void drawPrimitive(PrimitiveElement pe, const QStyleOption* opt, QPainter* p,
                     const QWidget* w = nullptr) const override;
  int pixelMetric(PixelMetric pm, const QStyleOption* opt = nullptr,
                  const QWidget* w = nullptr) const override;
  int styleHint(StyleHint sh, const QStyleOption* opt = nullptr, const QWidget* w = nullptr,
                QStyleHintReturn* ret = nullptr) const override;
  void polish(QWidget* w) override;
};

#endif  // MIDNIGHTSTYLE_H
