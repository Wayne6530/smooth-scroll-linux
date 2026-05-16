/**
 * Orchestrates the physics simulation and chart rendering.
 * Connects parameter changes to simulation re-runs and chart updates.
 * Always overlays default-config curves alongside current values.
 */
// eslint-disable-next-line no-unused-vars
const Visualization = (() => {
  let currentScenario = 'single';
  let currentValues = {};

  function init(values) {
    currentValues = values;
    renderLabels();

    let resizeTimer;
    window.addEventListener('resize', () => {
      clearTimeout(resizeTimer);
      resizeTimer = setTimeout(() => run(), 100);
    });
  }

  function update(values) {
    currentValues = values;
    run();
  }

  function setScenario(name) {
    currentScenario = name;
    run();
  }

  function run() {
    const scenario = Scenarios[currentScenario];
    if (!scenario) return;

    const currentOptions = buildOptions(currentValues);
    const defaultOptions = buildDefaultOptions();

    const currentResult = PhysicsEngine.simulate(currentOptions, scenario);
    const defaultResult = PhysicsEngine.simulate(defaultOptions, scenario);

    renderCharts(currentResult.timeline, defaultResult.timeline);
    renderSummary(currentResult.timeline, defaultResult.timeline, currentResult.truncated);
    renderTips();
  }

  function buildOptions(values) {
    return {
      tick_interval_microseconds: values.tick_interval_microseconds ?? 2000,
      min_deceleration: values.min_deceleration ?? 1420,
      max_deceleration: values.max_deceleration ?? 6000,
      initial_speed: values.initial_speed ?? 600,
      speed_factor: values.speed_factor ?? 40,
      speed_smooth_window_microseconds: values.speed_smooth_window_microseconds ?? 200000,
      max_speed_change_lowerbound: values.max_speed_change_lowerbound ?? 512,
      min_speed_change_upperbound: values.min_speed_change_upperbound ?? 512,
      min_speed_change_ratio: values.min_speed_change_ratio ?? 0.0625,
      max_speed_change_ratio: values.max_speed_change_ratio ?? 0.5,
      damping: values.damping ?? 3.1,
      use_reverse_scroll_braking: values.use_reverse_scroll_braking ?? true,
      max_reverse_scroll_braking_microseconds: values.max_reverse_scroll_braking_microseconds ?? 100000,
      max_reverse_scroll_braking_times: values.max_reverse_scroll_braking_times ?? 3,
      use_mouse_movement_braking: values.use_mouse_movement_braking ?? true,
      max_mouse_movement_distance: values.max_mouse_movement_distance ?? 30,
      mouse_movement_window_milliseconds: values.mouse_movement_window_milliseconds ?? 20,
      mouse_movement_delay_microseconds: values.mouse_movement_delay_microseconds ?? 100000,
      drag_view_speed: values.drag_view_speed ?? 3,
    };
  }

  function buildDefaultOptions() {
    const defaults = ParamSchema.getDefaultValues();
    return buildOptions(defaults);
  }

  function renderCharts(currentTimeline, defaultTimeline) {
    if (currentTimeline.length === 0) return;

    const xLabel = I18n.lang() === 'zh' ? '时间 (ms)' : 'Time (ms)';

    // Speed chart
    const speedData = currentTimeline.map(d => ({ x: d.timeMs, y: d.speed }));
    const defaultSpeedData = defaultTimeline.map(d => ({ x: d.timeMs, y: d.speed }));
    ChartRenderer.drawChart(
      document.getElementById('chart-speed'),
      speedData,
      { colorKey: 'speed', xLabel },
      defaultSpeedData
    );

    // Displacement chart
    const dispData = currentTimeline.map(d => ({ x: d.timeMs, y: d.totalDelta }));
    const defaultDispData = defaultTimeline.map(d => ({ x: d.timeMs, y: d.totalDelta }));
    ChartRenderer.drawChart(
      document.getElementById('chart-displacement'),
      dispData,
      { colorKey: 'displacement', xLabel },
      defaultDispData
    );
  }

  function renderSummary(currentTimeline, defaultTimeline, truncated) {
    if (currentTimeline.length === 0) {
      document.getElementById('summary-distance').textContent = '--';
      document.getElementById('summary-duration').textContent = '--';
      document.getElementById('summary-distance-hint').style.display = 'none';
      document.getElementById('summary-truncated').style.display = 'none';
      return;
    }

    const totalDistance = Math.round(currentTimeline[currentTimeline.length - 1].totalDelta);
    const duration = currentTimeline[currentTimeline.length - 1].timeMs;
    const defaultDistance = defaultTimeline.length > 0 ? Math.round(defaultTimeline[defaultTimeline.length - 1].totalDelta) : null;
    const defaultDuration = defaultTimeline.length > 0 ? defaultTimeline[defaultTimeline.length - 1].timeMs : null;

    document.getElementById('summary-distance').textContent = formatComparison(totalDistance, defaultDistance);
    document.getElementById('summary-duration').textContent = formatComparison(duration.toFixed(0) + ' ms', defaultDuration !== null ? defaultDuration.toFixed(0) + ' ms' : null);

    // Native distance hint, only for single-tick scenario.
    const hintEl = document.getElementById('summary-distance-hint');
    if (currentScenario === 'single' && Math.abs(totalDistance - 120) > 0.5) {
      hintEl.textContent = I18n.t('validation.native-distance');
      hintEl.style.display = 'block';
    } else {
      hintEl.style.display = 'none';
    }

    const truncEl = document.getElementById('summary-truncated');
    if (truncated) {
      truncEl.textContent = I18n.t('summary.truncated');
      truncEl.style.display = 'block';
    } else {
      truncEl.style.display = 'none';
    }
  }

  function formatComparison(current, defaultVal) {
    if (defaultVal === null || defaultVal === undefined || current === defaultVal || current == defaultVal) {
      return String(current);
    }
    return `${defaultVal} (${I18n.t('summary.default')}) -> ${current} (${I18n.t('summary.current')})`;
  }

  function renderTips() {
    const scenario = Scenarios[currentScenario];
    const tipsEl = document.getElementById('scenario-tips');
    if (!scenario || !scenario.tips) {
      tipsEl.textContent = '';
      return;
    }
    const lang = I18n.lang();
    const tipText = lang === 'zh' ? (scenario.tips.zh || scenario.tips.en) : (scenario.tips.en || scenario.tips.zh);
    if (!tipText) {
      tipsEl.innerHTML = '';
      return;
    }
    tipsEl.innerHTML = '<strong>' + I18n.t('tips.label') + '</strong> ' + tipText;
  }

  function renderLabels() {
    document.querySelector('.viz-panel h2').textContent = I18n.t('scenario.title');
    document.querySelectorAll('.chart-label').forEach((el, i) => {
      const keys = ['chart.speed', 'chart.displacement'];
      if (keys[i]) el.textContent = I18n.t(keys[i]);
    });
    document.querySelectorAll('.summary-label').forEach((el, i) => {
      const keys = ['summary.distance', 'summary.duration'];
      if (keys[i]) el.textContent = I18n.t(keys[i]);
    });
    document.querySelectorAll('.scenario-btn').forEach(btn => {
      const key = 'scenario.' + btn.dataset.scenario;
      btn.textContent = I18n.t(key);
    });
    renderTips();
    run();
  }

  return { init, update, setScenario, renderLabels };
})();
