// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#include "overlay_item.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>

#include <algorithm>
#include <cmath>

namespace SmoothScrollKWin
{

OverlayItem::OverlayItem(QQuickItem* parent) : QQuickPaintedItem(parent)
{
  setAntialiasing(true);
  setFillColor(Qt::transparent);
}

void OverlayItem::setMode(IndicatorMode mode)
{
  if (m_mode == mode)
  {
    return;
  }

  m_mode = mode;
  update();
}

void OverlayItem::setDotConfig(const DotVisualConfig& config)
{
  m_dot = config;
  update();
}

void OverlayItem::setArrowConfig(const ArrowVisualConfig& config)
{
  m_arrow = config;
  update();
}

void OverlayItem::setAutoScrollConfig(const AutoScrollVisualConfig& config)
{
  m_autoScroll = config;
  update();
}

void OverlayItem::setPassthroughConfig(const PassthroughVisualConfig& config)
{
  m_passthrough = config;
  update();
}

void OverlayItem::paint(QPainter* painter)
{
  if (m_mode == IndicatorMode::Hidden)
  {
    return;
  }

  painter->setRenderHint(QPainter::Antialiasing, true);

  if (m_mode == IndicatorMode::AutoScroll)
  {
    drawAutoScroll(painter);
    return;
  }
  if (m_mode == IndicatorMode::AutoScrollDot)
  {
    drawAutoScrollDot(painter);
    return;
  }

  QColor shadow(0, 0, 0);
  shadow.setAlphaF(0.28);
  painter->setPen(Qt::NoPen);
  painter->setBrush(shadow);
  if (m_mode == IndicatorMode::Arrow)
  {
    drawArrow(painter, 1);
  }
  else if (m_mode == IndicatorMode::Passthrough)
  {
    drawPassthrough(painter, 1);
  }
  else
  {
    drawDot(painter, 1);
  }

  painter->setPen(Qt::NoPen);
  painter->setBrush(colorForMode());
  if (m_mode == IndicatorMode::Arrow)
  {
    drawArrow(painter, 0);
  }
  else if (m_mode == IndicatorMode::Passthrough)
  {
    drawPassthrough(painter, 0);
  }
  else
  {
    drawDot(painter, 0);
  }
}

QColor OverlayItem::colorForMode() const
{
  if (m_mode == IndicatorMode::Passthrough)
  {
    return m_passthrough.color;
  }
  if (m_mode == IndicatorMode::Arrow)
  {
    return m_arrow.color;
  }
  return m_dot.color;
}

void OverlayItem::drawDot(QPainter* painter, int shadowOffset)
{
  const double size = width();
  const double radius = std::max(1.0, (size - shadowOffset * 2.0) / 2.0);
  const QPointF center(size / 2.0 + shadowOffset, size / 2.0 + shadowOffset);
  painter->drawEllipse(center, radius, radius);
}

void OverlayItem::drawArrow(QPainter* painter, int shadowOffset)
{
  const double size = width();
  const double center = size / 2.0 + shadowOffset;
  const double pad = std::max<double>(m_arrow.minPadding, std::round(size * m_arrow.paddingScale));
  const double head = std::max<double>(m_arrow.minHeadSize, std::round(size * m_arrow.headSizeScale));
  const double lineWidth = std::max<double>(m_arrow.minLineWidth, std::round(size * m_arrow.lineWidthScale));
  const double start = pad + head;
  const double end = size - pad - head;

  QPen pen(painter->brush().color(), lineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
  painter->setPen(pen);
  painter->drawLine(QPointF(start + shadowOffset, center), QPointF(end + shadowOffset, center));
  painter->drawLine(QPointF(center, start + shadowOffset), QPointF(center, end + shadowOffset));
  painter->setPen(Qt::NoPen);

  fillTriangle(painter, QPointF(center, pad + shadowOffset), head, m_arrow.headWidthScale, Qt::UpArrow);
  fillTriangle(painter, QPointF(center, size - pad + shadowOffset), head, m_arrow.headWidthScale, Qt::DownArrow);
  fillTriangle(painter, QPointF(pad + shadowOffset, center), head, m_arrow.headWidthScale, Qt::LeftArrow);
  fillTriangle(painter, QPointF(size - pad + shadowOffset, center), head, m_arrow.headWidthScale, Qt::RightArrow);
}

void OverlayItem::drawAutoScroll(QPainter* painter)
{
  const double size = width();
  const double center = size / 2.0;
  const double ringWidth = std::max(1.0, std::round(size * 0.05));
  const double radius = std::max(1.0, (size - ringWidth - 4.0) / 2.0);
  const double triangleLength = std::max(3.0, std::round(size * 0.15));
  const double triangleHalfWidth = std::max(2.0, std::round(size * 0.09));
  const double triangleTipDistance = std::max(triangleLength, radius - ringWidth - 2.0);

  const auto drawFrame = [&](const QColor& color, int shadowOffset) {
    const double frameCenter = center + shadowOffset;
    painter->setPen(QPen(color, ringWidth));
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(QPointF(frameCenter, frameCenter), radius, radius);
    painter->setPen(Qt::NoPen);
    painter->setBrush(color);

    const double widthScale = triangleHalfWidth / triangleLength;
    fillTriangle(painter, QPointF(frameCenter, frameCenter - triangleTipDistance), triangleLength, widthScale,
                 Qt::UpArrow);
    fillTriangle(painter, QPointF(frameCenter, frameCenter + triangleTipDistance), triangleLength, widthScale,
                 Qt::DownArrow);
    fillTriangle(painter, QPointF(frameCenter - triangleTipDistance, frameCenter), triangleLength, widthScale,
                 Qt::LeftArrow);
    fillTriangle(painter, QPointF(frameCenter + triangleTipDistance, frameCenter), triangleLength, widthScale,
                 Qt::RightArrow);
  };

  QColor shadow(0, 0, 0);
  shadow.setAlphaF(0.32);
  drawFrame(shadow, 1);
  drawFrame(m_autoScroll.color, 0);
}

void OverlayItem::drawAutoScrollDot(QPainter* painter)
{
  const double radius = m_autoScroll.dotSize / 2.0;
  QColor shadow(0, 0, 0);
  shadow.setAlphaF(0.32);
  painter->setPen(Qt::NoPen);
  painter->setBrush(shadow);
  painter->drawEllipse(QPointF(radius + 1.0, radius + 1.0), radius, radius);
  painter->setBrush(m_autoScroll.dotColor);
  painter->drawEllipse(QPointF(radius, radius), radius, radius);
}

void OverlayItem::drawPassthrough(QPainter* painter, int shadowOffset)
{
  const double size = width();
  const int maxPad = std::max(0, static_cast<int>(std::floor((size - 2.0) / 2.0)));
  const int pad = std::min(
      maxPad, std::max(m_passthrough.minPadding, static_cast<int>(std::round(size * m_passthrough.paddingScale))));
  const double lineWidth =
      std::max<double>(m_passthrough.minLineWidth, std::round(size * m_passthrough.lineWidthScale));
  const double start = pad + shadowOffset;
  const double end = size - pad + shadowOffset;

  QPen pen(painter->brush().color(), lineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
  painter->setPen(pen);
  painter->drawLine(QPointF(start, start), QPointF(end, end));
  painter->drawLine(QPointF(end, start), QPointF(start, end));
  painter->setPen(Qt::NoPen);
}

void OverlayItem::fillTriangle(QPainter* painter, const QPointF& point, double head, double widthScale,
                               Qt::ArrowType direction)
{
  const double half = head * widthScale;
  QPolygonF polygon;

  switch (direction)
  {
    case Qt::UpArrow:
      polygon << point << QPointF(point.x() - half, point.y() + head) << QPointF(point.x() + half, point.y() + head);
      break;
    case Qt::DownArrow:
      polygon << point << QPointF(point.x() - half, point.y() - head) << QPointF(point.x() + half, point.y() - head);
      break;
    case Qt::LeftArrow:
      polygon << point << QPointF(point.x() + head, point.y() - half) << QPointF(point.x() + head, point.y() + half);
      break;
    case Qt::RightArrow:
      polygon << point << QPointF(point.x() - head, point.y() - half) << QPointF(point.x() - head, point.y() + half);
      break;
    default:
      return;
  }

  painter->drawPolygon(polygon);
}

}  // namespace SmoothScrollKWin
