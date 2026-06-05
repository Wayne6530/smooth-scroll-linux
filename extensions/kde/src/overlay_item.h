// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#pragma once

#include <QColor>
#include <QQuickPaintedItem>

namespace SmoothScrollKWin
{

enum class IndicatorMode
{
  Hidden,
  Dot,
  Arrow,
  Passthrough,
};

struct DotVisualConfig
{
  bool enabled = true;
  int offsetX = 12;
  int offsetY = 0;
  int size = 8;
  QColor color = QColor(QStringLiteral("#4ea1ff"));
  double minAlpha = 0.16;
  int minAlphaSpeed = 100;
  double maxAlpha = 0.9;
  int maxAlphaSpeed = 3200;
};

struct ArrowVisualConfig
{
  bool enabled = true;
  int offsetX = 12;
  int offsetY = 0;
  int size = 22;
  QColor color = QColor(QStringLiteral("#4ea1ff"));
  double alpha = 0.9;
  double paddingScale = 0.12;
  int minPadding = 2;
  double headSizeScale = 0.18;
  int minHeadSize = 3;
  double lineWidthScale = 0.08;
  int minLineWidth = 1;
  double headWidthScale = 0.5;
};

struct PassthroughVisualConfig
{
  bool enabled = true;
  bool forceWhenNoRegularWindow = true;
  int offsetX = 12;
  int offsetY = 0;
  int size = 12;
  double paddingScale = 0.22;
  int minPadding = 3;
  double lineWidthScale = 0.12;
  int minLineWidth = 2;
  QColor color = QColor(QStringLiteral("#ff5c5c"));
  double alpha = 0.9;
};

class OverlayItem : public QQuickPaintedItem
{
  Q_OBJECT

public:
  explicit OverlayItem(QQuickItem* parent = nullptr);

  void setMode(IndicatorMode mode);
  void setDotConfig(const DotVisualConfig& config);
  void setArrowConfig(const ArrowVisualConfig& config);
  void setPassthroughConfig(const PassthroughVisualConfig& config);

  void paint(QPainter* painter) override;

private:
  QColor colorForMode() const;
  void drawDot(QPainter* painter, int shadowOffset);
  void drawArrow(QPainter* painter, int shadowOffset);
  void drawPassthrough(QPainter* painter, int shadowOffset);
  static void fillTriangle(QPainter* painter, const QPointF& point, double head, double widthScale,
                           Qt::ArrowType direction);

  IndicatorMode m_mode = IndicatorMode::Hidden;
  DotVisualConfig m_dot;
  ArrowVisualConfig m_arrow;
  PassthroughVisualConfig m_passthrough;
};

}  // namespace SmoothScrollKWin
