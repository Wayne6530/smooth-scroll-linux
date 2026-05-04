/**
 * Canvas-based chart renderer for the physics simulation output.
 * Supports overlaying a default-values curve alongside the current curve.
 */
// eslint-disable-next-line no-unused-vars
const ChartRenderer = (() => {

  const COLORS = {
    speed: { line: '#4a90d9', fill: 'rgba(74, 144, 217, 0.15)' },
    displacement: { line: '#2ecc71', fill: 'rgba(46, 204, 113, 0.15)' },
    delta: { line: '#e67e22', fill: 'rgba(230, 126, 34, 0.15)' },
    grid: '#e8e8e8',
    axis: '#999',
    text: '#666',
  };

  const OVERLAY_COLORS = {
    speed: { line: '#b0c4de', fill: 'rgba(176, 196, 222, 0.06)' },
    displacement: { line: '#a8d8b9', fill: 'rgba(168, 216, 185, 0.06)' },
    delta: { line: '#f0c8a0', fill: 'rgba(240, 200, 160, 0.06)' },
  };

  function setupCanvas(canvas) {
    const dpr = window.devicePixelRatio || 1;
    const rect = canvas.getBoundingClientRect();
    const w = rect.width || canvas.offsetWidth || 400;
    const h = rect.height || canvas.offsetHeight || 180;
    canvas.width = w * dpr;
    canvas.height = h * dpr;
    const ctx = canvas.getContext('2d');
    ctx.scale(dpr, dpr);
    return { ctx, width: w, height: h };
  }

  function drawChart(canvas, data, options, overlayData) {
    if (!data || data.length === 0) return;

    const { ctx, width, height } = setupCanvas(canvas);
    const padding = { top: 10, right: 16, bottom: 28, left: 50 };
    const chartW = width - padding.left - padding.right;
    const chartH = height - padding.top - padding.bottom;

    ctx.clearRect(0, 0, width, height);

    // Compute shared axis ranges
    const xMin = 0;
    const xMax = Math.max(data[data.length - 1].x, overlayData ? overlayData[overlayData.length - 1].x : 0, 1);
    let yMax = Math.max(...data.map(d => d.y), 1);
    if (overlayData && overlayData.length > 0) {
      yMax = Math.max(yMax, ...overlayData.map(d => d.y));
    }
    if (yMax > 0) yMax *= 1.1;

    const scaleX = (x) => padding.left + (x - xMin) / (xMax - xMin) * chartW;
    const scaleY = (y) => padding.top + chartH - y / yMax * chartH;

    // Grid lines
    ctx.strokeStyle = COLORS.grid;
    ctx.lineWidth = 1;

    const yTicks = niceTicks(0, yMax, 5);
    for (const tick of yTicks) {
      const y = scaleY(tick);
      ctx.beginPath();
      ctx.moveTo(padding.left, y);
      ctx.lineTo(width - padding.right, y);
      ctx.stroke();

      ctx.fillStyle = COLORS.text;
      ctx.font = '10px sans-serif';
      ctx.textAlign = 'right';
      ctx.fillText(formatNumber(tick), padding.left - 6, y + 3);
    }

    const xTicks = niceTicks(xMin, xMax, 6);
    for (const tick of xTicks) {
      const x = scaleX(tick);
      ctx.beginPath();
      ctx.moveTo(x, padding.top);
      ctx.lineTo(x, height - padding.bottom);
      ctx.stroke();

      ctx.fillStyle = COLORS.text;
      ctx.font = '10px sans-serif';
      ctx.textAlign = 'center';
      ctx.fillText(formatNumber(tick), x, height - padding.bottom + 14);
    }

    // Axis label
    ctx.fillStyle = COLORS.axis;
    ctx.font = '11px sans-serif';
    ctx.textAlign = 'center';
    ctx.fillText(options.xLabel || '', padding.left + chartW / 2, height - 2);

    const colors = COLORS[options.colorKey] || COLORS.speed;

    // Draw overlay (default) curve first
    if (overlayData && overlayData.length > 0) {
      const overlayColors = OVERLAY_COLORS[options.colorKey] || OVERLAY_COLORS.speed;

      // Light fill
      ctx.beginPath();
      ctx.moveTo(scaleX(overlayData[0].x), scaleY(0));
      for (const d of overlayData) ctx.lineTo(scaleX(d.x), scaleY(d.y));
      ctx.lineTo(scaleX(overlayData[overlayData.length - 1].x), scaleY(0));
      ctx.closePath();
      ctx.fillStyle = overlayColors.fill;
      ctx.fill();

      // Dashed line
      ctx.beginPath();
      ctx.setLineDash([6, 4]);
      ctx.moveTo(scaleX(overlayData[0].x), scaleY(overlayData[0].y));
      for (let i = 1; i < overlayData.length; i++) {
        ctx.lineTo(scaleX(overlayData[i].x), scaleY(overlayData[i].y));
      }
      ctx.strokeStyle = overlayColors.line;
      ctx.lineWidth = 1.2;
      ctx.stroke();
      ctx.setLineDash([]);
    }

    // Current curve — area fill
    ctx.beginPath();
    ctx.moveTo(scaleX(data[0].x), scaleY(0));
    for (const d of data) ctx.lineTo(scaleX(d.x), scaleY(d.y));
    ctx.lineTo(scaleX(data[data.length - 1].x), scaleY(0));
    ctx.closePath();
    ctx.fillStyle = colors.fill;
    ctx.fill();

    // Current curve — solid line
    ctx.beginPath();
    ctx.moveTo(scaleX(data[0].x), scaleY(data[0].y));
    for (let i = 1; i < data.length; i++) {
      ctx.lineTo(scaleX(data[i].x), scaleY(data[i].y));
    }
    ctx.strokeStyle = colors.line;
    ctx.lineWidth = 1.5;
    ctx.stroke();

    // Legend
    if (overlayData && overlayData.length > 0) {
      drawLegend(ctx, width, padding, colors);
    }
  }

  function drawBarChart(canvas, data, options, overlayData) {
    if (!data || data.length === 0) return;

    const { ctx, width, height } = setupCanvas(canvas);
    const padding = { top: 10, right: 16, bottom: 28, left: 50 };
    const chartW = width - padding.left - padding.right;
    const chartH = height - padding.top - padding.bottom;

    ctx.clearRect(0, 0, width, height);

    const xMin = 0;
    const xMax = Math.max(data[data.length - 1].x, overlayData ? overlayData[overlayData.length - 1].x : 0, 1);
    let yMax = Math.max(...data.map(d => Math.abs(d.y)), 1);
    if (overlayData && overlayData.length > 0) {
      yMax = Math.max(yMax, ...overlayData.map(d => Math.abs(d.y)));
    }
    if (yMax > 0) yMax *= 1.1;

    const scaleX = (x) => padding.left + (x - xMin) / (xMax - xMin) * chartW;
    const scaleY = (y) => padding.top + chartH - y / yMax * chartH;

    // Grid
    ctx.strokeStyle = COLORS.grid;
    ctx.lineWidth = 1;

    const yTicks = niceTicks(0, yMax, 5);
    for (const tick of yTicks) {
      const y = scaleY(tick);
      ctx.beginPath();
      ctx.moveTo(padding.left, y);
      ctx.lineTo(width - padding.right, y);
      ctx.stroke();

      ctx.fillStyle = COLORS.text;
      ctx.font = '10px sans-serif';
      ctx.textAlign = 'right';
      ctx.fillText(formatNumber(tick), padding.left - 6, y + 3);
    }

    const xTicks = niceTicks(xMin, xMax, 6);
    for (const tick of xTicks) {
      const x = scaleX(tick);
      ctx.fillStyle = COLORS.text;
      ctx.font = '10px sans-serif';
      ctx.textAlign = 'center';
      ctx.fillText(formatNumber(tick), x, height - padding.bottom + 14);
    }

    const colors = COLORS[options.colorKey] || COLORS.delta;
    const barWidth = Math.max(1, chartW / data.length * 0.8);

    // Draw overlay bars (outlined, no fill)
    if (overlayData && overlayData.length > 0) {
      const overlayColors = OVERLAY_COLORS[options.colorKey] || OVERLAY_COLORS.delta;
      for (const d of overlayData) {
        const x = scaleX(d.x) - barWidth / 2;
        const y = scaleY(Math.abs(d.y));
        const h = padding.top + chartH - y;

        ctx.strokeStyle = overlayColors.line;
        ctx.lineWidth = 1;
        ctx.strokeRect(x, y, barWidth, h);
      }
    }

    // Current bars (filled)
    for (const d of data) {
      const x = scaleX(d.x) - barWidth / 2;
      const y = scaleY(Math.abs(d.y));
      const h = padding.top + chartH - y;

      ctx.fillStyle = colors.line;
      ctx.fillRect(x, y, barWidth, h);
    }

    // Axis label
    ctx.fillStyle = COLORS.axis;
    ctx.font = '11px sans-serif';
    ctx.textAlign = 'center';
    ctx.fillText(options.xLabel || '', padding.left + chartW / 2, height - 2);

    // Legend
    if (overlayData && overlayData.length > 0) {
      drawLegend(ctx, width, padding, colors);
    }
  }

  function drawLegend(ctx, width, padding, colors) {
    const legendX = width - padding.right - 80;
    const legendY = padding.top + 4;
    ctx.font = '10px sans-serif';

    // Default line sample
    ctx.beginPath();
    ctx.setLineDash([4, 3]);
    ctx.moveTo(legendX, legendY + 5);
    ctx.lineTo(legendX + 16, legendY + 5);
    ctx.strokeStyle = '#999';
    ctx.lineWidth = 1.2;
    ctx.stroke();
    ctx.setLineDash([]);
    ctx.fillStyle = '#666';
    ctx.textAlign = 'left';
    ctx.fillText(I18n.t('chart.legend.default'), legendX + 20, legendY + 9);

    // Current line sample
    ctx.beginPath();
    ctx.moveTo(legendX, legendY + 18);
    ctx.lineTo(legendX + 16, legendY + 18);
    ctx.strokeStyle = colors.line;
    ctx.lineWidth = 1.5;
    ctx.stroke();
    ctx.fillText(I18n.t('chart.legend.current'), legendX + 20, legendY + 22);
  }

  function niceTicks(min, max, count) {
    if (max === min) return [min];
    const range = max - min;
    const roughStep = range / count;
    const mag = Math.pow(10, Math.floor(Math.log10(roughStep)));
    const normalized = roughStep / mag;

    let step;
    if (normalized <= 1) step = mag;
    else if (normalized <= 2) step = 2 * mag;
    else if (normalized <= 5) step = 5 * mag;
    else step = 10 * mag;

    const ticks = [];
    let tick = Math.ceil(min / step) * step;
    while (tick <= max) {
      ticks.push(tick);
      tick += step;
    }
    return ticks;
  }

  function formatNumber(n) {
    if (Math.abs(n) >= 1000) return n.toFixed(0);
    if (Math.abs(n) >= 1) return n.toFixed(1);
    if (Math.abs(n) >= 0.01) return n.toFixed(2);
    return n.toFixed(4);
  }

  return { drawChart, drawBarChart };
})();
