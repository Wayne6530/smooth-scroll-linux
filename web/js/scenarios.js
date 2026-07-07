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
      speed: {
        en: 'A native high-resolution wheel tick is 120 units. Adjust "Initial Speed" and "Min Deceleration" together to tune speed-mode distance and duration.',
        zh: '原生高分辨率滚轮单次为 120 单位。可同时调节“初始速度”和“最小减速度”来调整速度模式的距离和持续时间。',
      },
      distance: {
        en: 'Adjust "Min Deceleration", "Max Deceleration", and "Damping" to change distance-mode duration.',
        zh: '调节“最小减速度”、“最大减速度”和“滚动阻尼”可改变距离模式的持续时间。',
      },
    },
  },
  slow: {
    events: Array.from({ length: 5 }, (_, i) => ({ timeMs: i * 100, positive: true })),
    maxDurationMs: 2000,
    tips: {
      speed: {
        en: 'Try adjusting "Initial Speed" and "Speed Factor".',
        zh: '建议调节“初始速度”和“速度因子”。',
      },
      distance: {
        en: 'Try adjusting "Min Deceleration", "Max Deceleration", and "Damping".',
        zh: '建议调节“最小减速度”、“最大减速度”和“滚动阻尼”。',
      },
    },
  },
  fast: {
    events: Array.from({ length: 6 }, (_, i) => ({ timeMs: i * 15, positive: true })),
    maxDurationMs: 3000,
    tips: {
      speed: {
        en: 'Try adjusting "Damping", "Max Deceleration", "Speed Factor", and "Min Speed Change Upperbound".',
        zh: '建议调节“滚动阻尼”、“最大减速度”、“速度因子”和“速度变化上界的最小值”。',
      },
      distance: {
        en: 'Try adjusting "Damping" and "Max Deceleration".',
        zh: '建议调节“滚动阻尼”和“最大减速度”。',
      },
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
      speed: {
        en: 'Try adjusting "Max Speed Change Lowerbound" and "Min Speed Change Ratio".',
        zh: '建议调节“速度变化下界的最大值”和“最小速度变化比例”。',
      },
      distance: {
        en: 'Try adjusting "Damping", "Min Deceleration", and "Max Deceleration".',
        zh: '建议调节“滚动阻尼”、“最小减速度”和“最大减速度”。',
      },
    },
  },
};
