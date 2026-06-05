// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#pragma once

#include "ipc_client.h"
#include "overlay_item.h"

#include <effect/effect.h>

#include <QDateTime>
#include <QElapsedTimer>
#include <QFileSystemWatcher>
#include <QRegularExpression>
#include <QTimer>
#include <QVariantMap>

#include <memory>

namespace KWin
{
class EffectWindow;
class OffscreenQuickView;
}  // namespace KWin

namespace SmoothScrollKWin
{

struct TitleRule
{
  QRegularExpression title;
  bool forcePassthrough = false;
  bool enabled = true;
};

struct ForcePassthroughRule
{
  QString app;
  QString windowClass;
  bool forcePassthrough = false;
  bool enabled = true;
  QList<TitleRule> titles;
};

struct Config
{
  DotVisualConfig dot;
  ArrowVisualConfig arrow;
  PassthroughVisualConfig passthrough;
  bool stopOnPointerLeaveWindow = true;
  int pollIntervalMs = 4;
  QList<ForcePassthroughRule> forcePassthroughRules;
};

struct WindowInfo
{
  QString key;
  QString desktopFileName;
  QString resourceClass;
  QString resourceName;
  QString windowClass;
  QString title;
};

class SmoothScrollEffect : public KWin::Effect
{
  Q_OBJECT

public:
  explicit SmoothScrollEffect(QObject* parent = nullptr);
  ~SmoothScrollEffect() override;

  void paintScreen(const KWin::RenderTarget& renderTarget, const KWin::RenderViewport& viewport, int mask,
                   const KWin::Region& deviceRegion, KWin::LogicalOutput* screen) override;
  bool isActive() const override;
  void reconfigure(ReconfigureFlags flags) override;

public Q_SLOTS:
  Q_SCRIPTABLE QVariantMap currentWindowInfo() const;
  Q_SCRIPTABLE QString suggestedRuleForCurrentWindow(bool forcePassthrough, bool includeTitle) const;
  Q_SCRIPTABLE QString setForcePassthroughRuleForCurrentWindow(bool forcePassthrough, bool includeTitle);

private:
  void tick();
  void loadConfig();
  bool maybeReloadConfig();
  bool shellOverviewActive() const;
  KWin::EffectWindow* windowAt(const QPointF& pos) const;
  static bool isRegularApplicationWindow(KWin::EffectWindow* window);
  static WindowInfo infoForWindow(KWin::EffectWindow* window);
  bool shouldForcePassthrough(KWin::EffectWindow* window, const WindowInfo& info) const;
  bool ruleMatches(const ForcePassthroughRule& rule, const WindowInfo& info) const;
  void setForcePassthrough(bool enabled);
  void updateOverlay(const IpcSnapshot& snapshot, const QPointF& pointer, bool forcePassthroughActive);
  void syncOverlayGeometry(const QPointF& pointer);
  QRect overlayGeometryForPointer(const QPointF& pointer, IndicatorMode mode, int size) const;
  void hideOverlay();
  static IndicatorMode overlayModeForState(const Config& config, const IpcSnapshot& snapshot,
                                           bool forcePassthroughActive);
  static int visualSizeForMode(const Config& config, IndicatorMode mode);
  static QPoint visualOffsetForMode(const Config& config, IndicatorMode mode);
  static double alphaForMode(const Config& config, IndicatorMode mode, uint32_t speed);
  static double alphaForSpeed(const DotVisualConfig& config, uint32_t speed);
  void updatePointerLeaveBrake(const IpcSnapshot& snapshot, KWin::EffectWindow* pointerWindow);
  void requestAnchorStop(const IpcSnapshot& snapshot);
  void resetScrollAnchor();
  void resetState();
  void applyTimerInterval();
  void registerDbus();
  void unregisterDbus();

  Config m_config;
  QString m_configPath;
  QDateTime m_configLastModified;
  QElapsedTimer m_configReloadTimer;
  QTimer m_timer;
  IpcClient m_ipc;
  std::unique_ptr<KWin::OffscreenQuickView> m_overlayView;
  OverlayItem* m_overlayItem = nullptr;
  bool m_overlayVisible = false;
  bool m_overlayContentDirty = false;
  IndicatorMode m_overlayMode = IndicatorMode::Hidden;
  int m_overlaySize = 0;
  double m_overlayOpacity = -1.0;
  QRect m_overlayGeometry;
  QString m_anchorWindowKey;
  bool m_stopRequestedForAnchor = false;
  uint32_t m_lastSpeed = 0;
  uint32_t m_lastPid = 0;
  bool m_lastForcePassthrough = false;
  bool m_haveLastForcePassthrough = false;
  bool m_dbusServiceRegistered = false;
  bool m_dbusObjectRegistered = false;
};

}  // namespace SmoothScrollKWin
