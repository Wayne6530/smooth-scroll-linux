/** Behavior-only starting points for a single profile. */
const ProfilePresets = (() => {
  const items = [
    { id: 'inertia', name: { zh: 'Default', en: 'Default' },
      description: { zh: '最充分的平滑体验与灵活控制。需要一些时间适应速度驱动的滚动距离和惯性制动，熟悉后更得心应手。', en: 'The fullest smooth-scrolling experience with flexible control. Speed-dependent travel and inertia braking take some practice, but feel natural once learned.' } },
    { id: 'responsive', name: { zh: 'Responsive', en: 'Responsive' },
      description: { zh: '接近传统鼠标滚轮的操作习惯，简单易上手。以固定距离的惯性滚动搭配 Free Spin，兼顾跟手感与连续浏览。', en: 'Familiar wheel behavior with little to learn. Fixed-distance inertial scrolling and Free Spin balance responsive control with continuous browsing.' },
      overrides: {
        smooth_mode: 1,
        damping: 10,
        min_deceleration: 4200,
        max_deceleration: 30000,
        use_reverse_scroll_braking: false,
        use_mouse_movement_braking: false,
        drag_view_button: 0,
      } },
    { id: 'auto-scroll', name: { zh: 'Auto Scroll', en: 'Auto Scroll' },
      description: { zh: '专为只需要 Auto Scroll 的用户设计。保留普通滚轮的即时反馈，按需使用自动滚动。', en: 'Designed for users who only want Auto Scroll. Keeps ordinary wheel input immediate, with automatic scrolling when needed.' },
      overrides: {
        free_spin_button: 0,
        smooth_mode: 1,
        min_deceleration: 15150001,
        max_deceleration: 15150001,
        use_reverse_scroll_braking: false,
        use_mouse_movement_braking: false,
        drag_view_button: 0,
        drag_view_activation_mode: 1,
        auto_scroll_button: 274,
        auto_scroll_activation_mode: 1,
        auto_scroll_speed_exponent: 2,
      } },
  ];
  items.push({ id: 'compatibility', name: { zh: 'Compatibility', en: 'Compatibility' },
    description: { zh: '原样转发滚轮，不启用特殊按键。', en: 'Raw wheel input with special buttons disabled.' } });
  const keys = ParamSchema.params.filter(param => param.group !== 'device').map(param => param.key);
  const pick = values => Object.fromEntries(keys.filter(key => key in values).map(key => [key, structuredClone(values[key])]));
  function defaults() {
    return pick(ParamSchema.getDefaultValues());
  }
  function get(id) { return items.find(item => item.id === id) || items[0]; }
  function values(id) {
    const preset = get(id);
    if (preset.id === 'compatibility') return Object.assign(defaults(), {
      wheel_passthrough: true, free_spin_button: 0, drag_view_button: 0, auto_scroll_button: 0,
      keyboard_braking_keys: [], keyboard_passthrough_keys: [],
    });
    const result = pick(ParamSchema.getDefaultValues());
    return Object.assign(result, structuredClone(preset.overrides || {}));
  }
  return { items, get, values, defaults, pick };
})();
