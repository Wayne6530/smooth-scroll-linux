/**
 * Canvas-based chart renderer for the physics simulation output.
 * Supports overlaying a default-values curve alongside the current curve.
 */
// eslint-disable-next-line no-unused-vars
const ChartRenderer = (() => {

  const COLORS = {
    speed: { line: '#3680d9', fill: 'rgba(54, 128, 217, 0.15)', cursor: '#3680d9' },
    displacement: { line: '#b07040', fill: 'rgba(176, 112, 64, 0.15)', cursor: '#b07040' },
    delta: { line: '#b07040', fill: 'rgba(176, 112, 64, 0.15)', cursor: '#b07040' },
    grid: '#e5e9ef',
    axis: '#9ca3af',
    text: '#6b7280',
  };

  const OVERLAY_COLORS = {
    speed: { line: '#b0c4de', fill: 'rgba(176, 196, 222, 0.06)' },
    displacement: { line: '#d9b89a', fill: 'rgba(217, 184, 154, 0.06)' },
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
    const padding = options.padding || { top: 10, right: 16, bottom: 28, left: 50 };
    const chartW = width - padding.left - padding.right;
    const chartH = height - padding.top - padding.bottom;

    ctx.clearRect(0, 0, width, height);

    // When animating, use full data for axis ranges but only draw up to progressMs
    const progressMs = options.progressMs;
    const drawData = progressMs != null ? data.filter(d => d.x <= progressMs) : data;

    // Compute shared axis ranges (always from full data for stable axes)
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
      const overlayDraw = progressMs != null ? overlayData.filter(d => d.x <= progressMs) : overlayData;
      const overlayColors = OVERLAY_COLORS[options.colorKey] || OVERLAY_COLORS.speed;

      if (overlayDraw.length > 0) {
        // Light fill
        ctx.beginPath();
        ctx.moveTo(scaleX(overlayDraw[0].x), scaleY(0));
        for (const d of overlayDraw) ctx.lineTo(scaleX(d.x), scaleY(d.y));
        ctx.lineTo(scaleX(overlayDraw[overlayDraw.length - 1].x), scaleY(0));
        ctx.closePath();
        ctx.fillStyle = overlayColors.fill;
        ctx.fill();

        // Dashed line
        ctx.beginPath();
        ctx.setLineDash([6, 4]);
        ctx.moveTo(scaleX(overlayDraw[0].x), scaleY(overlayDraw[0].y));
        for (let i = 1; i < overlayDraw.length; i++) {
          ctx.lineTo(scaleX(overlayDraw[i].x), scaleY(overlayDraw[i].y));
        }
        ctx.strokeStyle = overlayColors.line;
        ctx.lineWidth = 1.2;
        ctx.stroke();
        ctx.setLineDash([]);
      }
    }

    // Current curve area fill, using drawData for progressive reveal.
    if (drawData.length > 0) {
      ctx.beginPath();
      ctx.moveTo(scaleX(drawData[0].x), scaleY(0));
      for (const d of drawData) ctx.lineTo(scaleX(d.x), scaleY(d.y));
      ctx.lineTo(scaleX(drawData[drawData.length - 1].x), scaleY(0));
      ctx.closePath();
      ctx.fillStyle = colors.fill;
      ctx.fill();

      // Current curve solid line.
      ctx.beginPath();
      ctx.moveTo(scaleX(drawData[0].x), scaleY(drawData[0].y));
      for (let i = 1; i < drawData.length; i++) {
        ctx.lineTo(scaleX(drawData[i].x), scaleY(drawData[i].y));
      }
      ctx.strokeStyle = colors.line;
      ctx.lineWidth = 1.5;
      ctx.stroke();
    }

    // Progress cursor line
    if (progressMs != null) {
      const cursorX = scaleX(Math.min(progressMs, xMax));
      ctx.beginPath();
      ctx.moveTo(cursorX, padding.top);
      ctx.lineTo(cursorX, height - padding.bottom);
      ctx.strokeStyle = colors.cursor;
      ctx.lineWidth = 1;
      ctx.setLineDash([3, 2]);
      ctx.stroke();
      ctx.setLineDash([]);

      // Dot at curve intersection
      if (drawData.length > 0) {
        const lastPt = drawData[drawData.length - 1];
        ctx.beginPath();
        ctx.arc(cursorX, scaleY(lastPt.y), 3, 0, Math.PI * 2);
        ctx.fillStyle = colors.cursor;
        ctx.fill();
      }
    }

    // Legend
    if (overlayData && overlayData.length > 0) {
      drawLegend(ctx, width, padding, colors, options);
    }
  }

  function drawBarChart(canvas, data, options, overlayData) {
    if (!data || data.length === 0) return;

    const { ctx, width, height } = setupCanvas(canvas);
    const padding = options.padding || { top: 10, right: 16, bottom: 28, left: 50 };
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
      drawLegend(ctx, width, padding, colors, options);
    }
  }

  function drawLegend(ctx, width, padding, colors, options) {
    const defaultLabel = options.defaultLabel || I18n.t('chart.legend.default');
    const currentLabel = options.currentLabel || I18n.t('chart.legend.current');
    const legendWidth = Math.max(80, defaultLabel.length * 6 + 24, currentLabel.length * 6 + 24);
    const legendX = width - padding.right - legendWidth;
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
    ctx.fillText(defaultLabel, legendX + 20, legendY + 9);

    // Current line sample
    ctx.beginPath();
    ctx.moveTo(legendX, legendY + 18);
    ctx.lineTo(legendX + 16, legendY + 18);
    ctx.strokeStyle = colors.line;
    ctx.lineWidth = 1.5;
    ctx.stroke();
    ctx.fillText(currentLabel, legendX + 20, legendY + 22);
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
