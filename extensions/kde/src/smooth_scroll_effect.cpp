// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#include "smooth_scroll_effect.h"

#include <core/output.h>
#include <effect/effecthandler.h>
#include <effect/effectwindow.h>
#include <effect/offscreenquickview.h>
#include <window.h>

#include <KPluginFactory>

#include <QDBusConnection>
#include <QDBusError>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLoggingCategory>
#include <QQuickItem>
#include <QSaveFile>
#include <QStandardPaths>
#include <QVariantMap>

#include <algorithm>
#include <cmath>

Q_LOGGING_CATEGORY(SMOOTH_SCROLL_KWIN, "smooth-scroll.kwin")

namespace SmoothScrollKWin
{

namespace
{
constexpr int MinPollIntervalMs = 1;
constexpr int MaxPollIntervalMs = 1000;
constexpr int ConfigReloadIntervalMs = 250;
constexpr int OverlayRepaintMargin = 4;
constexpr int OverlayCanvasMargin = 4;
constexpr const char* ConfigDirName = "smooth-scroll-kde-effect";
constexpr const char* ConfigFileName = "config.json";
constexpr const char* DbusServiceName = "org.smooth_scroll.KWinEffect";
constexpr const char* DbusObjectPath = "/SmoothScrollKWinEffect";

struct RuleMatcher
{
  QString key;
  QString value;
  QString source;
};

bool readBool(const QJsonObject& object, const QString& key, bool fallback)
{
  const QJsonValue value = object.value(key);
  return value.isBool() ? value.toBool() : fallback;
}

int readInt(const QJsonObject& object, const QString& key, int fallback, int min, int max)
{
  const QJsonValue value = object.value(key);
  if (!value.isDouble())
  {
    return fallback;
  }

  return std::clamp(value.toInt(fallback), min, max);
}

double readDouble(const QJsonObject& object, const QString& key, double fallback, double min, double max)
{
  const QJsonValue value = object.value(key);
  if (!value.isDouble())
  {
    return fallback;
  }

  return std::clamp(value.toDouble(fallback), min, max);
}

QString readString(const QJsonObject& object, const QString& key)
{
  const QJsonValue value = object.value(key);
  return value.isString() ? value.toString() : QString();
}

QColor readColor(const QJsonObject& object, const QString& key, const QColor& fallback)
{
  const QString value = readString(object, key);
  if (value.isEmpty())
  {
    return fallback;
  }

  const QColor color(value);
  return color.isValid() ? color : fallback;
}

bool stringMatches(const QString& pattern, const QString& value)
{
  if (pattern.isEmpty())
  {
    return true;
  }

  if (value == pattern)
  {
    return true;
  }

  if (!pattern.endsWith(QLatin1String(".desktop")) && value == pattern + QLatin1String(".desktop"))
  {
    return true;
  }

  if (pattern.endsWith(QLatin1String(".desktop")) && pattern.chopped(8) == value)
  {
    return true;
  }

  return false;
}

bool isOverviewEffectName(const QString& name)
{
  return name == QLatin1String("overview") || name == QLatin1String("windowview") ||
         name == QLatin1String("desktopgrid") || name == QLatin1String("presentwindows") ||
         name == QLatin1String("presentwindows_current") || name == QLatin1String("presentwindows_all");
}

RuleMatcher ruleMatcherForInfo(const WindowInfo& info)
{
  if (!info.desktopFileName.isEmpty())
  {
    return { QStringLiteral("app"), info.desktopFileName, QStringLiteral("desktop_file_name") };
  }
  if (!info.resourceClass.isEmpty())
  {
    return { QStringLiteral("class"), info.resourceClass, QStringLiteral("resource_class") };
  }
  if (!info.windowClass.isEmpty())
  {
    return { QStringLiteral("class"), info.windowClass, QStringLiteral("window_class") };
  }
  if (!info.resourceName.isEmpty())
  {
    return { QStringLiteral("class"), info.resourceName, QStringLiteral("resource_name") };
  }
  return {};
}

QString exactTitleRegex(const QString& title)
{
  return QStringLiteral("^%1$").arg(QRegularExpression::escape(title));
}

QJsonObject makeRuleObject(const WindowInfo& info, bool forcePassthrough, bool includeTitle)
{
  const RuleMatcher matcher = ruleMatcherForInfo(info);
  QJsonObject rule;
  if (matcher.key.isEmpty())
  {
    return rule;
  }

  rule.insert(QStringLiteral("enabled"), true);
  rule.insert(matcher.key, matcher.value);
  rule.insert(QStringLiteral("force_passthrough"), includeTitle ? false : forcePassthrough);

  if (includeTitle && !info.title.isEmpty())
  {
    QJsonObject titleRule;
    titleRule.insert(QStringLiteral("enabled"), true);
    titleRule.insert(QStringLiteral("title"), exactTitleRegex(info.title));
    titleRule.insert(QStringLiteral("force_passthrough"), forcePassthrough);
    rule.insert(QStringLiteral("titles"), QJsonArray{ titleRule });
  }

  return rule;
}

QString compactJson(const QJsonObject& object)
{
  return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

QString indentedJson(const QJsonObject& object)
{
  return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Indented)).trimmed();
}

bool objectHasMatcher(const QJsonObject& object, const RuleMatcher& matcher)
{
  if (matcher.key.isEmpty())
  {
    return false;
  }

  const QString pattern = object.value(matcher.key).toString();
  return !pattern.isEmpty() && stringMatches(pattern, matcher.value);
}

int findMatchingRuleIndex(const QJsonArray& rules, const RuleMatcher& matcher)
{
  for (qsizetype i = 0; i < rules.size(); ++i)
  {
    if (!rules.at(i).isObject())
    {
      continue;
    }
    if (objectHasMatcher(rules.at(i).toObject(), matcher))
    {
      return static_cast<int>(i);
    }
  }
  return -1;
}

bool upsertTitleRule(QJsonObject& rule, const QString& titlePattern, bool forcePassthrough)
{
  bool updated = false;
  QJsonArray titles = rule.value(QStringLiteral("titles")).toArray();
  for (qsizetype i = 0; i < titles.size(); ++i)
  {
    if (!titles.at(i).isObject())
    {
      continue;
    }
    QJsonObject titleObject = titles.at(i).toObject();
    if (titleObject.value(QStringLiteral("title")).toString() != titlePattern)
    {
      continue;
    }

    titleObject.insert(QStringLiteral("enabled"), true);
    titleObject.insert(QStringLiteral("force_passthrough"), forcePassthrough);
    titles.replace(i, titleObject);
    updated = true;
    break;
  }

  if (!updated)
  {
    QJsonObject titleObject;
    titleObject.insert(QStringLiteral("enabled"), true);
    titleObject.insert(QStringLiteral("title"), titlePattern);
    titleObject.insert(QStringLiteral("force_passthrough"), forcePassthrough);
    titles.append(titleObject);
  }

  rule.insert(QStringLiteral("titles"), titles);
  return updated;
}

void requestOverlayRepaint(const QRect& geometry)
{
  if (!geometry.isNull())
  {
    const QRect repaintGeometry =
        geometry.adjusted(-OverlayRepaintMargin, -OverlayRepaintMargin, OverlayRepaintMargin, OverlayRepaintMargin);
    KWin::effects->addRepaint(repaintGeometry.x(), repaintGeometry.y(), repaintGeometry.width(),
                              repaintGeometry.height());
  }
}

}  // namespace

SmoothScrollEffect::SmoothScrollEffect(QObject* parent) : KWin::Effect(parent)
{
  const QString configRoot = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
  m_configPath =
      QDir(configRoot)
          .filePath(QStringLiteral("%1/%2").arg(QLatin1String(ConfigDirName), QLatin1String(ConfigFileName)));

  loadConfig();
  initializeOverlayViews();

  connect(&m_timer, &QTimer::timeout, this, &SmoothScrollEffect::tick);
  m_timer.setTimerType(Qt::PreciseTimer);
  m_configReloadTimer.start();
  applyTimerInterval();
  m_timer.start();
  registerDbus();
}

SmoothScrollEffect::~SmoothScrollEffect()
{
  m_timer.stop();
  unregisterDbus();
  hideOverlay();
  m_ipc.setForcePassthrough(false);
  m_ipc.close();
}

void SmoothScrollEffect::initializeOverlayViews()
{
  m_overlayView = std::make_unique<KWin::OffscreenQuickView>(KWin::OffscreenQuickView::ExportMode::Texture, true);
#if SMOOTH_SCROLL_KWIN_6_7_OR_NEWER
  m_overlayView->setAutomaticRepaint(false);
#endif
  m_overlayView->setVisible(false);
  m_overlayItem = new OverlayItem(m_overlayView->contentItem());
  m_overlayItem->setMode(IndicatorMode::Hidden);

  m_autoScrollDotView = std::make_unique<KWin::OffscreenQuickView>(KWin::OffscreenQuickView::ExportMode::Texture, true);
#if SMOOTH_SCROLL_KWIN_6_7_OR_NEWER
  m_autoScrollDotView->setAutomaticRepaint(false);
#endif
  m_autoScrollDotView->setVisible(false);
  m_autoScrollDotItem = new OverlayItem(m_autoScrollDotView->contentItem());
  m_autoScrollDotItem->setMode(IndicatorMode::AutoScrollDot);

  applyOverlayConfig();

#if !SMOOTH_SCROLL_KWIN_6_7_OR_NEWER
  connect(m_overlayView.get(), &KWin::OffscreenQuickView::repaintNeeded, this,
          [this]() { requestOverlayRepaint(m_overlayGeometry); });
  connect(m_autoScrollDotView.get(), &KWin::OffscreenQuickView::repaintNeeded, this,
          [this]() { requestOverlayRepaint(m_autoScrollDotGeometry); });
#endif
}

#if SMOOTH_SCROLL_KWIN_6_7_OR_NEWER
void SmoothScrollEffect::recreateOverlayViewsForScaleChange()
{
  const QRect oldOverlayGeometry = m_overlayGeometry;
  const QRect oldAutoScrollDotGeometry = m_autoScrollDotGeometry;

  m_overlayItem = nullptr;
  m_overlayView.reset();
  m_autoScrollDotItem = nullptr;
  m_autoScrollDotView.reset();

  m_overlayVisible = false;
  m_overlayContentDirty = false;
  m_autoScrollDotVisible = false;
  m_autoScrollDotContentDirty = false;
  m_overlayMode = IndicatorMode::Hidden;
  m_overlaySize = 0;
  m_overlayViewSize = 0;
  m_autoScrollDotViewSize = 0;
  m_overlayDevicePixelRatio = 0.0;
  m_autoScrollDotDevicePixelRatio = 0.0;
  m_overlayOpacity = -1.0;
  m_overlayGeometry = QRect();
  m_autoScrollDotGeometry = QRect();

  initializeOverlayViews();
  requestOverlayRepaint(oldOverlayGeometry);
  requestOverlayRepaint(oldAutoScrollDotGeometry);
}
#endif

#if !SMOOTH_SCROLL_KWIN_6_7_OR_NEWER
void SmoothScrollEffect::prePaintScreen(KWin::ScreenPrePaintData& data, std::chrono::milliseconds presentTime)
{
  if (m_overlayVisible && !m_overlayGeometry.isNull())
  {
    // KWin 6.6 can retain parts of an independently composited effect from
    // older swapchain buffers when another part of the screen causes a
    // repaint. Include the complete indicator in every frame KWin already
    // intends to paint, without scheduling frames while the screen is idle.
    data.paint = data.paint.united(m_overlayGeometry);
  }
  KWin::effects->prePaintScreen(data, presentTime);
}

void SmoothScrollEffect::paintScreen(const KWin::RenderTarget& renderTarget, const KWin::RenderViewport& viewport,
                                     int mask, const KWin::Region& deviceRegion, KWin::LogicalOutput* screen)
{
  KWin::effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen);

  if (m_overlayVisible && m_overlayView)
  {
    syncOverlayGeometry(KWin::effects->cursorPos());
    if (m_overlayContentDirty)
    {
      m_overlayView->update();
      m_overlayContentDirty = false;
    }
    KWin::effects->renderOffscreenQuickView(renderTarget, viewport, m_overlayView.get());

    if (m_autoScrollDotVisible && m_autoScrollDotView)
    {
      syncAutoScrollDotGeometry();
      if (m_autoScrollDotContentDirty)
      {
        m_autoScrollDotView->update();
        m_autoScrollDotContentDirty = false;
      }
      KWin::effects->renderOffscreenQuickView(renderTarget, viewport, m_autoScrollDotView.get());
    }
  }
}
#endif

bool SmoothScrollEffect::isActive() const
{
  return m_overlayVisible;
}

void SmoothScrollEffect::reconfigure(ReconfigureFlags flags)
{
  Q_UNUSED(flags)
  loadConfig();
  applyOverlayConfig();
  applyTimerInterval();
}

QVariantMap SmoothScrollEffect::currentWindowInfo() const
{
  const QPointF pointer = KWin::effects->cursorPos();
  KWin::EffectWindow* window = windowAt(pointer);
  const WindowInfo info = infoForWindow(window);
  const RuleMatcher matcher = ruleMatcherForInfo(info);
  const bool regular = isRegularApplicationWindow(window);

  QVariantMap map;
  map.insert(QStringLiteral("found"), window != nullptr);
  map.insert(QStringLiteral("regular_window"), regular);
  map.insert(QStringLiteral("cursor_x"), pointer.x());
  map.insert(QStringLiteral("cursor_y"), pointer.y());
  map.insert(QStringLiteral("key"), info.key);
  map.insert(QStringLiteral("desktop_file_name"), info.desktopFileName);
  map.insert(QStringLiteral("resource_class"), info.resourceClass);
  map.insert(QStringLiteral("resource_name"), info.resourceName);
  map.insert(QStringLiteral("window_class"), info.windowClass);
  map.insert(QStringLiteral("title"), info.title);
  map.insert(QStringLiteral("recommended_rule_key"), matcher.key);
  map.insert(QStringLiteral("recommended_rule_value"), matcher.value);
  map.insert(QStringLiteral("recommended_rule_source"), matcher.source);
  map.insert(QStringLiteral("force_passthrough_now"), shouldForcePassthrough(window, info));
  return map;
}

QString SmoothScrollEffect::suggestedRuleForCurrentWindow(bool forcePassthrough, bool includeTitle) const
{
  KWin::EffectWindow* window = windowAt(KWin::effects->cursorPos());
  if (!isRegularApplicationWindow(window))
  {
    return QStringLiteral("ERROR: no regular application window under cursor");
  }

  const WindowInfo info = infoForWindow(window);
  const QJsonObject rule = makeRuleObject(info, forcePassthrough, includeTitle);
  if (rule.isEmpty())
  {
    return QStringLiteral("ERROR: the window does not expose a usable app or class id");
  }

  return indentedJson(rule);
}

QString SmoothScrollEffect::setForcePassthroughRuleForCurrentWindow(bool forcePassthrough, bool includeTitle)
{
  KWin::EffectWindow* window = windowAt(KWin::effects->cursorPos());
  if (!isRegularApplicationWindow(window))
  {
    return QStringLiteral("ERROR: no regular application window under cursor");
  }

  const WindowInfo info = infoForWindow(window);
  const RuleMatcher matcher = ruleMatcherForInfo(info);
  if (matcher.key.isEmpty())
  {
    return QStringLiteral("ERROR: the window does not expose a usable app or class id");
  }

  QJsonObject root;
  QFile file(m_configPath);
  if (file.exists())
  {
    if (!file.open(QIODevice::ReadOnly))
    {
      return QStringLiteral("ERROR: failed to read %1: %2").arg(m_configPath, file.errorString());
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
      return QStringLiteral("ERROR: failed to parse %1: %2").arg(m_configPath, parseError.errorString());
    }

    root = document.object();
  }

  QJsonArray rules = root.value(QStringLiteral("force_passthrough_rules")).toArray();
  const int existingRuleIndex = findMatchingRuleIndex(rules, matcher);
  const bool ruleExists = existingRuleIndex >= 0;
  bool titleRuleUpdated = false;
  QJsonObject rule =
      ruleExists ? rules.at(existingRuleIndex).toObject() : makeRuleObject(info, forcePassthrough, includeTitle);

  rule.insert(QStringLiteral("enabled"), true);
  rule.insert(matcher.key, matcher.value);

  if (includeTitle)
  {
    if (info.title.isEmpty())
    {
      return QStringLiteral("ERROR: the window title is empty; use includeTitle=false");
    }
    if (!rule.contains(QStringLiteral("force_passthrough")))
    {
      rule.insert(QStringLiteral("force_passthrough"), false);
    }
    titleRuleUpdated = upsertTitleRule(rule, exactTitleRegex(info.title), forcePassthrough);
  }
  else
  {
    rule.insert(QStringLiteral("force_passthrough"), forcePassthrough);
  }

  if (ruleExists)
  {
    rules.replace(existingRuleIndex, rule);
  }
  else
  {
    rules.append(rule);
  }
  root.insert(QStringLiteral("force_passthrough_rules"), rules);

  const QFileInfo configInfo(m_configPath);
  if (!QDir().mkpath(configInfo.absolutePath()))
  {
    return QStringLiteral("ERROR: failed to create %1").arg(configInfo.absolutePath());
  }

  QSaveFile saveFile(m_configPath);
  if (!saveFile.open(QIODevice::WriteOnly))
  {
    return QStringLiteral("ERROR: failed to write %1: %2").arg(m_configPath, saveFile.errorString());
  }

  saveFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
  if (!saveFile.commit())
  {
    return QStringLiteral("ERROR: failed to commit %1: %2").arg(m_configPath, saveFile.errorString());
  }

  loadConfig();
  applyOverlayConfig();
  applyTimerInterval();

  const QString action = ruleExists ? (includeTitle && !titleRuleUpdated ? QStringLiteral("Added title override") :
                                                                           QStringLiteral("Updated")) :
                                      QStringLiteral("Added");
  return QStringLiteral("%1 %2=%3 force_passthrough=%4 in %5\n%6")
      .arg(action, matcher.key, matcher.value, forcePassthrough ? QStringLiteral("true") : QStringLiteral("false"),
           m_configPath, compactJson(rule));
}

void SmoothScrollEffect::tick()
{
  maybeReloadConfig();

  const IpcSnapshot snapshot = m_ipc.readSnapshot();
  if (!snapshot.valid)
  {
    resetState();
    return;
  }

  if (m_lastPid != snapshot.pid)
  {
    resetScrollAnchor();
    m_haveLastForcePassthrough = false;
    m_lastPid = snapshot.pid;
  }

  const QPointF pointer = KWin::effects->cursorPos();
  const bool shellOverview = shellOverviewActive();
  KWin::EffectWindow* pointerWindow = shellOverview ? nullptr : windowAt(pointer);
  const WindowInfo pointerInfo = infoForWindow(pointerWindow);

  const bool forcePassthrough = shellOverview || shouldForcePassthrough(pointerWindow, pointerInfo);
  setForcePassthrough(forcePassthrough);
  updateOverlay(snapshot, pointer, forcePassthrough);

  if (forcePassthrough)
  {
    resetScrollAnchor();
  }
  else
  {
    updatePointerLeaveBrake(snapshot, pointerWindow);
  }

  m_lastSpeed = snapshot.speed;
}

void SmoothScrollEffect::loadConfig()
{
  Config next;

  const QFileInfo fileInfo(m_configPath);
  m_configLastModified = fileInfo.exists() ? fileInfo.lastModified() : QDateTime();

  QFile file(m_configPath);
  if (!file.open(QIODevice::ReadOnly))
  {
    m_config = next;
    return;
  }

  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject())
  {
    qCWarning(SMOOTH_SCROLL_KWIN) << "Failed to parse config" << m_configPath << parseError.errorString();
    m_config = next;
    return;
  }

  const QJsonObject root = document.object();
  const QJsonObject dot = root.value(QStringLiteral("dot")).toObject();
  const QJsonObject arrow = root.value(QStringLiteral("arrow")).toObject();
  const QJsonObject autoScroll = root.value(QStringLiteral("auto_scroll")).toObject();
  const QJsonObject passthroughVisual = root.value(QStringLiteral("passthrough")).toObject();
  const QJsonObject scroll = root.value(QStringLiteral("scroll")).toObject();

  next.dot.enabled = readBool(dot, QStringLiteral("enabled"), next.dot.enabled);
  next.dot.offsetX = readInt(dot, QStringLiteral("offset_x"), next.dot.offsetX, -10000, 10000);
  next.dot.offsetY = readInt(dot, QStringLiteral("offset_y"), next.dot.offsetY, -10000, 10000);
  next.dot.size = readInt(dot, QStringLiteral("size"), next.dot.size, 1, 512);
  next.dot.color = readColor(dot, QStringLiteral("color"), next.dot.color);
  next.dot.minAlpha = readDouble(dot, QStringLiteral("min_alpha"), next.dot.minAlpha, 0.0, 1.0);
  next.dot.minAlphaSpeed = readInt(dot, QStringLiteral("min_alpha_speed"), next.dot.minAlphaSpeed, 0, 65535);
  next.dot.maxAlpha = readDouble(dot, QStringLiteral("max_alpha"), next.dot.maxAlpha, 0.0, 1.0);
  next.dot.maxAlphaSpeed = readInt(dot, QStringLiteral("max_alpha_speed"), next.dot.maxAlphaSpeed, 0, 65535);

  next.arrow.enabled = readBool(arrow, QStringLiteral("enabled"), next.arrow.enabled);
  next.arrow.offsetX = readInt(arrow, QStringLiteral("offset_x"), next.arrow.offsetX, -10000, 10000);
  next.arrow.offsetY = readInt(arrow, QStringLiteral("offset_y"), next.arrow.offsetY, -10000, 10000);
  next.arrow.size = readInt(arrow, QStringLiteral("size"), next.arrow.size, 1, 512);
  next.arrow.color = readColor(arrow, QStringLiteral("color"), next.arrow.color);
  next.arrow.alpha = readDouble(arrow, QStringLiteral("alpha"), next.arrow.alpha, 0.0, 1.0);
  next.arrow.paddingScale = readDouble(arrow, QStringLiteral("padding_scale"), next.arrow.paddingScale, 0.0, 1.0);
  next.arrow.minPadding = readInt(arrow, QStringLiteral("min_padding"), next.arrow.minPadding, 0, 512);
  next.arrow.headSizeScale = readDouble(arrow, QStringLiteral("head_size_scale"), next.arrow.headSizeScale, 0.0, 1.0);
  next.arrow.minHeadSize = readInt(arrow, QStringLiteral("min_head_size"), next.arrow.minHeadSize, 0, 512);
  next.arrow.lineWidthScale =
      readDouble(arrow, QStringLiteral("line_width_scale"), next.arrow.lineWidthScale, 0.0, 1.0);
  next.arrow.minLineWidth = readInt(arrow, QStringLiteral("min_line_width"), next.arrow.minLineWidth, 0, 512);
  next.arrow.headWidthScale =
      readDouble(arrow, QStringLiteral("head_width_scale"), next.arrow.headWidthScale, 0.0, 4.0);

  next.autoScroll.enabled = readBool(autoScroll, QStringLiteral("enabled"), next.autoScroll.enabled);
  next.autoScroll.offsetX = readInt(autoScroll, QStringLiteral("offset_x"), next.autoScroll.offsetX, -256, 256);
  next.autoScroll.offsetY = readInt(autoScroll, QStringLiteral("offset_y"), next.autoScroll.offsetY, -256, 256);
  next.autoScroll.size = readInt(autoScroll, QStringLiteral("size"), next.autoScroll.size, 20, 256);
  next.autoScroll.color = readColor(autoScroll, QStringLiteral("color"), next.autoScroll.color);
  next.autoScroll.dotColor = readColor(autoScroll, QStringLiteral("dot_color"), next.autoScroll.dotColor);
  next.autoScroll.alpha = readDouble(autoScroll, QStringLiteral("alpha"), next.autoScroll.alpha, 0.0, 1.0);
  next.autoScroll.dotSize = readInt(autoScroll, QStringLiteral("dot_size"), next.autoScroll.dotSize, 2, 64);

  next.passthrough.enabled = readBool(passthroughVisual, QStringLiteral("enabled"), next.passthrough.enabled);
  next.passthrough.forceWhenNoRegularWindow = readBool(
      passthroughVisual, QStringLiteral("force_when_no_regular_window"), next.passthrough.forceWhenNoRegularWindow);
  next.passthrough.offsetX =
      readInt(passthroughVisual, QStringLiteral("offset_x"), next.passthrough.offsetX, -10000, 10000);
  next.passthrough.offsetY =
      readInt(passthroughVisual, QStringLiteral("offset_y"), next.passthrough.offsetY, -10000, 10000);
  next.passthrough.size = readInt(passthroughVisual, QStringLiteral("size"), next.passthrough.size, 1, 512);
  next.passthrough.paddingScale =
      readDouble(passthroughVisual, QStringLiteral("padding_scale"), next.passthrough.paddingScale, 0.0, 1.0);
  next.passthrough.minPadding =
      readInt(passthroughVisual, QStringLiteral("min_padding"), next.passthrough.minPadding, 0, 512);
  next.passthrough.lineWidthScale =
      readDouble(passthroughVisual, QStringLiteral("line_width_scale"), next.passthrough.lineWidthScale, 0.0, 1.0);
  next.passthrough.minLineWidth =
      readInt(passthroughVisual, QStringLiteral("min_line_width"), next.passthrough.minLineWidth, 0, 512);
  next.passthrough.color = readColor(passthroughVisual, QStringLiteral("color"), next.passthrough.color);
  next.passthrough.alpha = readDouble(passthroughVisual, QStringLiteral("alpha"), next.passthrough.alpha, 0.0, 1.0);

  next.stopOnPointerLeaveWindow =
      readBool(scroll, QStringLiteral("stop_on_pointer_leave_window"), next.stopOnPointerLeaveWindow);
  next.pollIntervalMs =
      readInt(scroll, QStringLiteral("poll_interval_ms"), next.pollIntervalMs, MinPollIntervalMs, MaxPollIntervalMs);

  const QJsonArray rules = root.value(QStringLiteral("force_passthrough_rules")).toArray();
  for (const QJsonValue& ruleValue : rules)
  {
    if (!ruleValue.isObject())
    {
      continue;
    }

    const QJsonObject ruleObject = ruleValue.toObject();
    ForcePassthroughRule rule;
    rule.enabled = readBool(ruleObject, QStringLiteral("enabled"), true);
    rule.app = readString(ruleObject, QStringLiteral("app"));
    rule.windowClass = readString(ruleObject, QStringLiteral("class"));
    rule.forcePassthrough = readBool(ruleObject, QStringLiteral("force_passthrough"), false);

    const QJsonArray titleRules = ruleObject.value(QStringLiteral("titles")).toArray();
    for (const QJsonValue& titleValue : titleRules)
    {
      if (!titleValue.isObject())
      {
        continue;
      }

      const QJsonObject titleObject = titleValue.toObject();
      const QString pattern = readString(titleObject, QStringLiteral("title"));
      if (pattern.isEmpty())
      {
        continue;
      }

      TitleRule titleRule;
      titleRule.enabled = readBool(titleObject, QStringLiteral("enabled"), true);
      titleRule.forcePassthrough = readBool(titleObject, QStringLiteral("force_passthrough"), rule.forcePassthrough);
      titleRule.title = QRegularExpression(pattern);
      if (!titleRule.title.isValid())
      {
        qCWarning(SMOOTH_SCROLL_KWIN) << "Ignoring invalid title regex" << pattern << titleRule.title.errorString();
        continue;
      }

      rule.titles.append(titleRule);
    }

    next.forcePassthroughRules.append(rule);
  }

  m_config = next;
}

void SmoothScrollEffect::applyOverlayConfig()
{
  if (m_overlayItem)
  {
    m_overlayItem->setDotConfig(m_config.dot);
    m_overlayItem->setArrowConfig(m_config.arrow);
    m_overlayItem->setAutoScrollConfig(m_config.autoScroll);
    m_overlayItem->setPassthroughConfig(m_config.passthrough);
    const int viewSize =
        std::max({ m_config.dot.size, m_config.arrow.size, m_config.autoScroll.size, m_config.passthrough.size }) +
        OverlayCanvasMargin * 2;
    if (m_overlayViewSize != viewSize)
    {
      m_overlayItem->setWidth(viewSize);
      m_overlayItem->setHeight(viewSize);
      m_overlayViewSize = viewSize;
    }
    m_overlayContentDirty = true;
    requestOverlayRepaint(m_overlayGeometry);
  }
  if (m_autoScrollDotItem)
  {
    m_autoScrollDotItem->setAutoScrollConfig(m_config.autoScroll);
    m_autoScrollDotItem->setVisualSize(m_config.autoScroll.dotSize);
    m_autoScrollDotContentDirty = true;
    requestOverlayRepaint(m_autoScrollDotGeometry);
  }
}

bool SmoothScrollEffect::maybeReloadConfig()
{
  if (m_configReloadTimer.isValid() && !m_configReloadTimer.hasExpired(ConfigReloadIntervalMs))
  {
    return false;
  }
  m_configReloadTimer.restart();

  const QFileInfo fileInfo(m_configPath);
  const QDateTime modified = fileInfo.exists() ? fileInfo.lastModified() : QDateTime();
  if (modified == m_configLastModified)
  {
    return false;
  }

  const int oldInterval = m_config.pollIntervalMs;
  loadConfig();
  applyOverlayConfig();
  if (oldInterval != m_config.pollIntervalMs)
  {
    applyTimerInterval();
  }
  return true;
}

bool SmoothScrollEffect::shellOverviewActive() const
{
  const QStringList activeEffects = KWin::effects->activeEffects();
  return std::any_of(activeEffects.begin(), activeEffects.end(), isOverviewEffectName);
}

KWin::EffectWindow* SmoothScrollEffect::windowAt(const QPointF& pos) const
{
  const QList<KWin::EffectWindow*> windows = KWin::effects->stackingOrder();
  for (auto it = windows.crbegin(); it != windows.crend(); ++it)
  {
    KWin::EffectWindow* window = *it;
    if (!window || window->isDeleted() || window->isMinimized() || !window->isVisible())
    {
      continue;
    }

    if (window->frameGeometry().contains(pos))
    {
      return window;
    }
  }

  return nullptr;
}

bool SmoothScrollEffect::isRegularApplicationWindow(KWin::EffectWindow* window)
{
  if (!window || window->isDeleted() || window->isMinimized() || !window->isVisible())
  {
    return false;
  }

  if (window->isDesktop() || window->isDock() || window->isMenu() || window->isDropdownMenu() ||
      window->isPopupMenu() || window->isTooltip() || window->isNotification() || window->isCriticalNotification() ||
      window->isOnScreenDisplay() || window->isComboBox() || window->isDNDIcon() || window->isOutline() ||
      window->isLockScreen() || window->isInputMethod())
  {
    return false;
  }

  return window->isNormalWindow() || window->isDialog();
}

WindowInfo SmoothScrollEffect::infoForWindow(KWin::EffectWindow* window)
{
  WindowInfo info;
  if (!window)
  {
    return info;
  }

  info.key = window->internalId().toString(QUuid::WithoutBraces);
  info.windowClass = window->windowClass();
  info.title = window->caption();

  if (KWin::Window* kwinWindow = window->window())
  {
    info.desktopFileName = kwinWindow->desktopFileName();
    info.resourceClass = kwinWindow->resourceClass();
    info.resourceName = kwinWindow->resourceName();
  }

  return info;
}

bool SmoothScrollEffect::shouldForcePassthrough(KWin::EffectWindow* window, const WindowInfo& info) const
{
  if (!isRegularApplicationWindow(window))
  {
    return m_config.passthrough.forceWhenNoRegularWindow;
  }

  for (const ForcePassthroughRule& rule : m_config.forcePassthroughRules)
  {
    if (!ruleMatches(rule, info))
    {
      continue;
    }

    for (const TitleRule& titleRule : rule.titles)
    {
      if (!titleRule.enabled)
      {
        continue;
      }

      const QRegularExpressionMatch match = titleRule.title.match(info.title);
      if (match.hasMatch())
      {
        return titleRule.forcePassthrough;
      }
    }

    return rule.forcePassthrough;
  }

  return false;
}

bool SmoothScrollEffect::ruleMatches(const ForcePassthroughRule& rule, const WindowInfo& info) const
{
  if (!rule.enabled)
  {
    return false;
  }

  if (!rule.app.isEmpty())
  {
    return stringMatches(rule.app, info.desktopFileName) || stringMatches(rule.app, info.resourceClass) ||
           stringMatches(rule.app, info.windowClass);
  }

  if (!rule.windowClass.isEmpty())
  {
    return stringMatches(rule.windowClass, info.resourceClass) || stringMatches(rule.windowClass, info.resourceName) ||
           stringMatches(rule.windowClass, info.windowClass);
  }

  return false;
}

void SmoothScrollEffect::setForcePassthrough(bool enabled)
{
  if (m_haveLastForcePassthrough && m_lastForcePassthrough == enabled)
  {
    return;
  }

  if (m_ipc.setForcePassthrough(enabled))
  {
    m_lastForcePassthrough = enabled;
    m_haveLastForcePassthrough = true;
  }
}

void SmoothScrollEffect::updateOverlay(const IpcSnapshot& snapshot, const QPointF& pointer, bool forcePassthroughActive)
{
  if (!m_overlayView || !m_overlayItem || !m_autoScrollDotView || !m_autoScrollDotItem)
  {
    return;
  }

  const IndicatorMode mode = overlayModeForState(m_config, snapshot, forcePassthroughActive);
  if (mode == IndicatorMode::Hidden)
  {
    hideOverlay();
    return;
  }

#if SMOOTH_SCROLL_KWIN_6_7_OR_NEWER
  const KWin::LogicalOutput* pointerOutput = KWin::effects->screenAt(pointer.toPoint());
  const qreal pointerDevicePixelRatio = pointerOutput ? pointerOutput->scale() : 1.0;
  if (m_overlayDevicePixelRatio > 0.0 && !qFuzzyCompare(m_overlayDevicePixelRatio, pointerDevicePixelRatio))
  {
    recreateOverlayViewsForScaleChange();
  }
#endif

  const int size = visualSizeForMode(m_config, mode);
  const double alpha = alphaForMode(m_config, mode, snapshot.speed);
#if SMOOTH_SCROLL_KWIN_6_7_OR_NEWER
  bool overlayModeChanged = false;
#endif

  if (m_overlayMode != mode)
  {
    m_overlayItem->setMode(mode);
    m_overlayMode = mode;
    m_overlayContentDirty = true;
#if SMOOTH_SCROLL_KWIN_6_7_OR_NEWER
    overlayModeChanged = true;
#endif
  }
  if (m_overlaySize != size)
  {
    m_overlayItem->setVisualSize(size);
    m_overlaySize = size;
    m_overlayContentDirty = true;
  }
  if (std::abs(m_overlayOpacity - alpha) > 0.001)
  {
#if SMOOTH_SCROLL_KWIN_6_7_OR_NEWER
    m_overlayItem->setOpacity(alpha);
    m_overlayContentDirty = true;
#else
    m_overlayView->setOpacity(alpha);
    requestOverlayRepaint(m_overlayGeometry);
#endif
    m_overlayOpacity = alpha;
  }

  syncOverlayGeometry(pointer);

  if (mode == IndicatorMode::AutoScroll)
  {
    m_autoScrollDotItem->setMode(IndicatorMode::AutoScrollDot);
    if (!m_autoScrollDotVisible)
    {
      // hideAutoScrollDot() leaves the item in Hidden mode. Keep our manual
      // update bookkeeping in sync with the QQuickItem state change.
      m_autoScrollDotContentDirty = true;
    }
    m_autoScrollOffsetX = snapshot.autoScrollOffsetX;
    m_autoScrollOffsetY = snapshot.autoScrollOffsetY;

    const int dotViewSize = m_config.autoScroll.dotSize + OverlayCanvasMargin * 2;
    if (m_autoScrollDotViewSize != dotViewSize)
    {
      m_autoScrollDotItem->setWidth(dotViewSize);
      m_autoScrollDotItem->setHeight(dotViewSize);
      m_autoScrollDotViewSize = dotViewSize;
      m_autoScrollDotContentDirty = true;
    }
#if SMOOTH_SCROLL_KWIN_6_7_OR_NEWER
    if (std::abs(m_autoScrollDotItem->opacity() - alpha) > 0.001)
    {
      m_autoScrollDotItem->setOpacity(alpha);
      m_autoScrollDotContentDirty = true;
    }
#else
    m_autoScrollDotView->setOpacity(alpha);
#endif
    syncAutoScrollDotGeometry();

    if (!m_autoScrollDotVisible)
    {
      m_autoScrollDotView->show();
      m_autoScrollDotVisible = true;
#if !SMOOTH_SCROLL_KWIN_6_7_OR_NEWER
      requestOverlayRepaint(m_autoScrollDotGeometry);
#endif
    }
    if (m_autoScrollDotContentDirty)
    {
#if SMOOTH_SCROLL_KWIN_6_7_OR_NEWER
      updateOffscreenView(m_autoScrollDotView.get());
      m_autoScrollDotContentDirty = false;
      requestOverlayRepaint(m_autoScrollDotGeometry);
#else
      m_autoScrollDotItem->update();
      requestOverlayRepaint(m_autoScrollDotGeometry);
#endif
    }
  }
  else
  {
    hideAutoScrollDot();
  }

  if (!m_overlayVisible)
  {
    m_overlayView->show();
    m_overlayVisible = true;
#if !SMOOTH_SCROLL_KWIN_6_7_OR_NEWER
    requestOverlayRepaint(m_overlayGeometry);
#endif
  }

  if (m_overlayContentDirty)
  {
#if SMOOTH_SCROLL_KWIN_6_7_OR_NEWER
    updateOffscreenView(m_overlayView.get(), overlayModeChanged ? 3 : 2);
    m_overlayContentDirty = false;
    requestOverlayRepaint(m_overlayGeometry);
#else
    m_overlayItem->update();
    requestOverlayRepaint(m_overlayGeometry);
#endif
  }
}

void SmoothScrollEffect::syncOverlayGeometry(const QPointF& pointer)
{
  if (!m_overlayView || m_overlayMode == IndicatorMode::Hidden || m_overlayViewSize <= 0)
  {
    return;
  }

  const QRect nextGeometry = overlayGeometryForPointer(pointer, m_overlayMode, m_overlayViewSize);
  if (m_overlayGeometry != nextGeometry)
  {
    const QRect oldGeometry = m_overlayGeometry;
    m_overlayView->setGeometry(nextGeometry);
    m_overlayGeometry = nextGeometry;
    requestOverlayRepaint(oldGeometry);
    requestOverlayRepaint(nextGeometry);
  }

  if (syncViewDevicePixelRatio(m_overlayView.get(), m_overlayItem, nextGeometry, m_overlayDevicePixelRatio))
  {
    m_overlayContentDirty = true;
  }
}

void SmoothScrollEffect::syncAutoScrollDotGeometry()
{
  if (!m_autoScrollDotView || m_overlayMode != IndicatorMode::AutoScroll || m_autoScrollDotViewSize <= 0 ||
      m_overlayGeometry.isNull())
  {
    return;
  }

  const QRect nextGeometry = autoScrollDotGeometry();
  if (m_autoScrollDotGeometry != nextGeometry)
  {
    const QRect oldGeometry = m_autoScrollDotGeometry;
    m_autoScrollDotView->setGeometry(nextGeometry);
    m_autoScrollDotGeometry = nextGeometry;
    requestOverlayRepaint(oldGeometry);
    requestOverlayRepaint(nextGeometry);
  }

  if (syncViewDevicePixelRatio(m_autoScrollDotView.get(), m_autoScrollDotItem, nextGeometry,
                               m_autoScrollDotDevicePixelRatio))
  {
    m_autoScrollDotContentDirty = true;
  }
}

bool SmoothScrollEffect::syncViewDevicePixelRatio(KWin::OffscreenQuickView* view, OverlayItem* item,
                                                  const QRect& geometry, qreal& currentDevicePixelRatio)
{
  if (!view || !item || geometry.isNull())
  {
    return false;
  }

  const KWin::LogicalOutput* output = KWin::effects->screenAt(geometry.center());
  const qreal devicePixelRatio = output ? output->scale() : 1.0;
  if (qFuzzyCompare(currentDevicePixelRatio, devicePixelRatio))
  {
    return false;
  }

  view->setDevicePixelRatio(devicePixelRatio);
  item->setRenderDevicePixelRatio(devicePixelRatio);
  currentDevicePixelRatio = devicePixelRatio;
  return true;
}

#if SMOOTH_SCROLL_KWIN_6_7_OR_NEWER
void SmoothScrollEffect::updateOffscreenView(KWin::OffscreenQuickView* view, int renderPasses)
{
  if (!view || !view->isVisible())
  {
    return;
  }

  // The custom texture node is synchronized by the first update. The next
  // update exports that texture to the buffer consumed by KWin's scene item.
  // Mode switches get one additional pass so a second view update cannot
  // leave the main surface on an older swapchain buffer.
  for (int pass = 0; pass < renderPasses; ++pass)
  {
    view->update(nullptr);
  }
}
#endif

QRect SmoothScrollEffect::overlayGeometryForPointer(const QPointF& pointer, IndicatorMode mode, int size) const
{
  const QPoint offset = visualOffsetForMode(m_config, mode);
  const auto virtualGeometry = KWin::effects->virtualScreenGeometry();

  const int minX = std::floor(virtualGeometry.x());
  const int minY = std::floor(virtualGeometry.y());
  const int maxX = std::max(minX, static_cast<int>(std::ceil(virtualGeometry.x() + virtualGeometry.width())) - size);
  const int maxY = std::max(minY, static_cast<int>(std::ceil(virtualGeometry.y() + virtualGeometry.height())) - size);
  const int x = std::clamp(static_cast<int>(std::round(pointer.x() + offset.x() - size / 2.0)), minX, maxX);
  const int y = std::clamp(static_cast<int>(std::round(pointer.y() + offset.y() - size / 2.0)), minY, maxY);
  return QRect(x, y, size, size);
}

QRect SmoothScrollEffect::autoScrollDotGeometry() const
{
  const auto virtualGeometry = KWin::effects->virtualScreenGeometry();
  // Anchor to the displayed marker rather than the raw pointer so the two
  // visuals remain aligned when the origin marker is clamped at a screen edge.
  const double originX = m_overlayGeometry.x() + m_overlayViewSize / 2.0;
  const double originY = m_overlayGeometry.y() + m_overlayViewSize / 2.0;
  const double dotViewRadius = m_autoScrollDotViewSize / 2.0;

  const int minX = std::floor(virtualGeometry.x());
  const int minY = std::floor(virtualGeometry.y());
  const int maxX = std::max(minX, static_cast<int>(std::ceil(virtualGeometry.x() + virtualGeometry.width())) -
                                      m_autoScrollDotViewSize);
  const int maxY = std::max(minY, static_cast<int>(std::ceil(virtualGeometry.y() + virtualGeometry.height())) -
                                      m_autoScrollDotViewSize);
  const int x = std::clamp(static_cast<int>(std::round(originX + m_autoScrollOffsetX - dotViewRadius)), minX, maxX);
  const int y = std::clamp(static_cast<int>(std::round(originY + m_autoScrollOffsetY - dotViewRadius)), minY, maxY);
  return QRect(x, y, m_autoScrollDotViewSize, m_autoScrollDotViewSize);
}

void SmoothScrollEffect::hideOverlay()
{
  if (!m_overlayVisible && !m_autoScrollDotVisible)
  {
    return;
  }

  const QRect oldGeometry = m_overlayGeometry;
  hideAutoScrollDot();
  m_overlayVisible = false;
  m_overlayContentDirty = false;
  m_overlayMode = IndicatorMode::Hidden;
  m_overlaySize = 0;
  m_overlayOpacity = -1.0;
  m_overlayGeometry = QRect();
  if (m_overlayItem)
  {
    m_overlayItem->setMode(IndicatorMode::Hidden);
  }
  if (m_overlayView)
  {
#if SMOOTH_SCROLL_KWIN_6_7_OR_NEWER
    if (m_overlayView->isVisible())
    {
      updateOffscreenView(m_overlayView.get());
    }
#else
    m_overlayView->hide();
#endif
  }
  requestOverlayRepaint(oldGeometry);
}

void SmoothScrollEffect::hideAutoScrollDot()
{
  if (!m_autoScrollDotVisible)
  {
    return;
  }

  const QRect oldGeometry = m_autoScrollDotGeometry;
  m_autoScrollDotVisible = false;
  m_autoScrollDotGeometry = QRect();
  m_autoScrollOffsetX = 0;
  m_autoScrollOffsetY = 0;
  if (m_autoScrollDotItem)
  {
    m_autoScrollDotItem->setMode(IndicatorMode::Hidden);
  }
  if (m_autoScrollDotView)
  {
#if SMOOTH_SCROLL_KWIN_6_7_OR_NEWER
    if (m_autoScrollDotView->isVisible())
    {
      updateOffscreenView(m_autoScrollDotView.get());
    }
    m_autoScrollDotView->hide();
#else
    m_autoScrollDotView->hide();
#endif
  }
  requestOverlayRepaint(oldGeometry);
}

IndicatorMode SmoothScrollEffect::overlayModeForState(const Config& config, const IpcSnapshot& snapshot,
                                                      bool forcePassthroughActive)
{
  if (forcePassthroughActive && config.passthrough.enabled)
  {
    return IndicatorMode::Passthrough;
  }
  if (snapshot.autoScroll)
  {
    return config.autoScroll.enabled ? IndicatorMode::AutoScroll : IndicatorMode::Hidden;
  }
  if (snapshot.dragView && config.arrow.enabled)
  {
    return IndicatorMode::Arrow;
  }
  if (snapshot.speed > 0 && config.dot.enabled)
  {
    return IndicatorMode::Dot;
  }
  return IndicatorMode::Hidden;
}

int SmoothScrollEffect::visualSizeForMode(const Config& config, IndicatorMode mode)
{
  if (mode == IndicatorMode::Arrow)
  {
    return config.arrow.size;
  }
  if (mode == IndicatorMode::AutoScroll)
  {
    return config.autoScroll.size;
  }
  if (mode == IndicatorMode::Passthrough)
  {
    return config.passthrough.size;
  }
  return config.dot.size;
}

QPoint SmoothScrollEffect::visualOffsetForMode(const Config& config, IndicatorMode mode)
{
  if (mode == IndicatorMode::Arrow)
  {
    return QPoint(config.arrow.offsetX, config.arrow.offsetY);
  }
  if (mode == IndicatorMode::AutoScroll)
  {
    return QPoint(config.autoScroll.offsetX, config.autoScroll.offsetY);
  }
  if (mode == IndicatorMode::Passthrough)
  {
    return QPoint(config.passthrough.offsetX, config.passthrough.offsetY);
  }
  return QPoint(config.dot.offsetX, config.dot.offsetY);
}

double SmoothScrollEffect::alphaForMode(const Config& config, IndicatorMode mode, uint32_t speed)
{
  if (mode == IndicatorMode::Passthrough)
  {
    return config.passthrough.alpha;
  }
  if (mode == IndicatorMode::Arrow)
  {
    return config.arrow.alpha;
  }
  if (mode == IndicatorMode::AutoScroll)
  {
    return config.autoScroll.alpha;
  }
  return alphaForSpeed(config.dot, speed);
}

double SmoothScrollEffect::alphaForSpeed(const DotVisualConfig& config, uint32_t speed)
{
  if (config.minAlphaSpeed == config.maxAlphaSpeed)
  {
    return speed >= static_cast<uint32_t>(config.maxAlphaSpeed) ? config.maxAlpha : config.minAlpha;
  }

  const double t = std::clamp(
      (static_cast<double>(speed) - config.minAlphaSpeed) / (config.maxAlphaSpeed - config.minAlphaSpeed), 0.0, 1.0);
  return std::clamp(config.minAlpha + (config.maxAlpha - config.minAlpha) * t, 0.0, 1.0);
}

void SmoothScrollEffect::updatePointerLeaveBrake(const IpcSnapshot& snapshot, KWin::EffectWindow* pointerWindow)
{
  if (!m_config.stopOnPointerLeaveWindow || snapshot.speed == 0)
  {
    if (snapshot.speed == 0)
    {
      resetScrollAnchor();
    }
    return;
  }

  const QString pointerWindowKey = infoForWindow(pointerWindow).key;
  if (m_lastSpeed == 0)
  {
    m_anchorWindowKey = pointerWindowKey;
    m_stopRequestedForAnchor = false;

    if (m_anchorWindowKey.isEmpty())
    {
      requestAnchorStop(snapshot);
    }
    return;
  }

  if (m_anchorWindowKey.isEmpty() || pointerWindowKey != m_anchorWindowKey)
  {
    requestAnchorStop(snapshot);
  }
}

void SmoothScrollEffect::requestAnchorStop(const IpcSnapshot& snapshot)
{
  if (m_stopRequestedForAnchor)
  {
    return;
  }

  if (m_ipc.requestStop(snapshot))
  {
    m_stopRequestedForAnchor = true;
  }
}

void SmoothScrollEffect::resetScrollAnchor()
{
  m_anchorWindowKey.clear();
  m_stopRequestedForAnchor = false;
}

void SmoothScrollEffect::resetState()
{
  resetScrollAnchor();
  setForcePassthrough(false);
  hideOverlay();
  m_lastSpeed = 0;
  m_lastPid = 0;
}

void SmoothScrollEffect::applyTimerInterval()
{
  m_timer.setInterval(std::clamp(m_config.pollIntervalMs, MinPollIntervalMs, MaxPollIntervalMs));
}

void SmoothScrollEffect::registerDbus()
{
  QDBusConnection bus = QDBusConnection::sessionBus();
  if (!bus.isConnected())
  {
    qCWarning(SMOOTH_SCROLL_KWIN) << "Session D-Bus is not connected";
    return;
  }

  const QString serviceName = QString::fromLatin1(DbusServiceName);
  const QString objectPath = QString::fromLatin1(DbusObjectPath);

  m_dbusServiceRegistered = bus.registerService(serviceName);
  if (!m_dbusServiceRegistered)
  {
    qCWarning(SMOOTH_SCROLL_KWIN) << "Failed to register D-Bus service" << DbusServiceName << bus.lastError().message();
    return;
  }

  m_dbusObjectRegistered = bus.registerObject(objectPath, this, QDBusConnection::ExportScriptableSlots);
  if (!m_dbusObjectRegistered)
  {
    qCWarning(SMOOTH_SCROLL_KWIN) << "Failed to register D-Bus object" << DbusObjectPath << bus.lastError().message();
    bus.unregisterService(serviceName);
    m_dbusServiceRegistered = false;
  }
}

void SmoothScrollEffect::unregisterDbus()
{
  QDBusConnection bus = QDBusConnection::sessionBus();
  const QString serviceName = QString::fromLatin1(DbusServiceName);
  const QString objectPath = QString::fromLatin1(DbusObjectPath);
  if (m_dbusObjectRegistered)
  {
    bus.unregisterObject(objectPath);
    m_dbusObjectRegistered = false;
  }
  if (m_dbusServiceRegistered)
  {
    bus.unregisterService(serviceName);
    m_dbusServiceRegistered = false;
  }
}

}  // namespace SmoothScrollKWin

class SmoothScrollEffectFactory : public KWin::EffectPluginFactory
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID EffectPluginFactory_iid FILE "metadata.json")
  Q_INTERFACES(KPluginFactory)

public:
  bool isSupported() const override
  {
    return true;
  }

  bool enabledByDefault() const override
  {
    return false;
  }

  KWin::Effect* createEffect() const override
  {
    return new SmoothScrollKWin::SmoothScrollEffect();
  }
};

#include "smooth_scroll_effect.moc"
