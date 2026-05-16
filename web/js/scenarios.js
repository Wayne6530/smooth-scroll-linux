/**
 * Simulation scenario presets for the physics engine.
 * Extracted into a separate file for easy manual editing.
 */
// eslint-disable-next-line no-unused-vars
const Scenarios = {
  single: {
    events: [{ timeMs: 0, positive: true }],
    maxDurationMs: 1000,
    tips: {
      en: 'Try setting total distance to 120, the native distance of a single tick. Adjust "Min Deceleration" and "Initial Speed" together to change duration.',
      zh: '建议调节滚动距离到 120，这是一次滚动的原始距离。通过同时调节”最小减速度”和”初始速度”来改变持续时间。',
    },
  },
  slow: {
    events: Array.from({ length: 5 }, (_, i) => ({ timeMs: i * 100, positive: true })),
    maxDurationMs: 2000,
    tips: {
      en: 'Try adjusting "Initial Speed" and "Speed Factor".',
      zh: '建议调节”初始速度”和”速度因子”。',
    },
  },
  fast: {
    events: Array.from({ length: 6 }, (_, i) => ({ timeMs: i * 15, positive: true })),
    maxDurationMs: 3000,
    tips: {
      en: 'Try adjusting "Damping", "Max Deceleration", "Speed Factor", and "Min Speed Change Upperbound".',
      zh: '建议调节”滚动阻尼”、”最大减速度”、”速度因子”和”速度变化上界的最小值”。',
    },
  },
  'multi-burst': {
    events: [
      ...Array.from({ length: 7 }, (_, i) => ({ timeMs: i * 8, positive: true })),
      ...Array.from({ length: 7 }, (_, i) => ({ timeMs: 300 + i * 8, positive: true })),
      ...Array.from({ length: 7 }, (_, i) => ({ timeMs: 600 + i * 8, positive: true })),
      ...Array.from({ length: 7 }, (_, i) => ({ timeMs: 900 + i * 8, positive: true })),
      ...Array.from({ length: 7 }, (_, i) => ({ timeMs: 1200 + i * 8, positive: true })),
    ],
    maxDurationMs: 10000,
    tips: {
      en: 'Try adjusting "Max Speed Change Lowerbound" and "Min Speed Change Ratio".',
      zh: '建议调节”速度变化下界的最大值”和”最小速度变化比例”。',
    },
  },
};
