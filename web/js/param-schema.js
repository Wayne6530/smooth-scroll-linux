/**
 * Static parameter schema for the Smooth Scroll configurator.
 * Replaces runtime TOML parsing — all parameter definitions and metadata
 * are internalized in this module.
 */
// eslint-disable-next-line no-unused-vars
const ParamSchema = (() => {

  const CONFIG_VERSION = 2;

  const BUTTON_CODES = {
    0: 'DISABLED',
    272: 'BTN_LEFT',
    273: 'BTN_RIGHT',
    274: 'BTN_MIDDLE',
    275: 'BTN_SIDE',
    276: 'BTN_EXTRA',
  };

  const KEY_CNT = 768;

  const KEY_CODES = {
    29: 'KEY_LEFTCTRL',
    42: 'KEY_LEFTSHIFT',
    54: 'KEY_RIGHTSHIFT',
    56: 'KEY_LEFTALT',
    97: 'KEY_RIGHTCTRL',
    100: 'KEY_RIGHTALT',
    125: 'KEY_LEFTMETA',
    126: 'KEY_RIGHTMETA',
    1: 'KEY_ESC',
    14: 'KEY_BACKSPACE',
    15: 'KEY_TAB',
    28: 'KEY_ENTER',
    57: 'KEY_SPACE',
    58: 'KEY_CAPSLOCK',
    69: 'KEY_NUMLOCK',
    70: 'KEY_SCROLLLOCK',
    59: 'KEY_F1',
    60: 'KEY_F2',
    61: 'KEY_F3',
    62: 'KEY_F4',
    63: 'KEY_F5',
    64: 'KEY_F6',
    65: 'KEY_F7',
    66: 'KEY_F8',
    67: 'KEY_F9',
    68: 'KEY_F10',
    87: 'KEY_F11',
    88: 'KEY_F12',
    102: 'KEY_HOME',
    103: 'KEY_UP',
    104: 'KEY_PAGEUP',
    105: 'KEY_LEFT',
    106: 'KEY_RIGHT',
    107: 'KEY_END',
    108: 'KEY_DOWN',
    109: 'KEY_PAGEDOWN',
    110: 'KEY_INSERT',
    111: 'KEY_DELETE',
    113: 'KEY_MUTE',
    114: 'KEY_VOLUMEDOWN',
    115: 'KEY_VOLUMEUP',
    119: 'KEY_PAUSE',
    116: 'KEY_POWER',
    99: 'KEY_SYSRQ',
  };

  const KEY_LABELS = {
    29: 'L Ctrl',
    42: 'L Shift',
    54: 'R Shift',
    56: 'L Alt',
    97: 'R Ctrl',
    100: 'R Alt',
    125: 'L Meta',
    126: 'R Meta',
    1: 'Esc',
    14: 'Backspace',
    15: 'Tab',
    28: 'Enter',
    57: 'Space',
    58: 'CapsLk',
    69: 'NumLk',
    70: 'ScrLk',
    59: 'F1',
    60: 'F2',
    61: 'F3',
    62: 'F4',
    63: 'F5',
    64: 'F6',
    65: 'F7',
    66: 'F8',
    67: 'F9',
    68: 'F10',
    87: 'F11',
    88: 'F12',
    102: 'Home',
    103: 'Up',
    104: 'PgUp',
    105: 'Left',
    106: 'Right',
    107: 'End',
    108: 'Down',
    109: 'PgDn',
    110: 'Ins',
    111: 'Del',
    113: 'Mute',
    114: 'Vol-',
    115: 'Vol+',
    116: 'Power',
    119: 'Pause',
    99: 'SysRq',
  };

  const KEY_PRESETS = [29, 42, 54, 97, 125, 126];

  const REFERENCES = [
    '## Reference: Button Codes\n' +
    '# DISABLED    0\n' +
    '# BTN_LEFT    272\n' +
    '# BTN_RIGHT   273\n' +
    '# BTN_MIDDLE  274\n' +
    '# BTN_SIDE    275\n' +
    '# BTN_EXTRA   276',

    '## Reference: Key Codes\n' +
    '# KEY_ESC           1\n' +
    '# KEY_BACKSPACE     14\n' +
    '# KEY_TAB           15\n' +
    '# KEY_ENTER         28\n' +
    '# KEY_LEFTCTRL      29\n' +
    '# KEY_LEFTSHIFT     42\n' +
    '# KEY_LEFTALT       56\n' +
    '# KEY_SPACE         57\n' +
    '# KEY_CAPSLOCK      58\n' +
    '# KEY_F1-F10        59-68\n' +
    '# KEY_NUMLOCK       69\n' +
    '# KEY_SCROLLLOCK    70\n' +
    '# KEY_RIGHTSHIFT    54\n' +
    '# KEY_RIGHTCTRL     97\n' +
    '# KEY_RIGHTALT      100\n' +
    '# KEY_F11           87\n' +
    '# KEY_F12           88\n' +
    '# KEY_HOME          102\n' +
    '# KEY_UP            103\n' +
    '# KEY_PAGEUP        104\n' +
    '# KEY_LEFT          105\n' +
    '# KEY_RIGHT         106\n' +
    '# KEY_END           107\n' +
    '# KEY_DOWN          108\n' +
    '# KEY_PAGEDOWN      109\n' +
    '# KEY_INSERT        110\n' +
    '# KEY_DELETE        111\n' +
    '# KEY_MUTE          113\n' +
    '# KEY_VOLUMEDOWN    114\n' +
    '# KEY_VOLUMEUP      115\n' +
    '# KEY_LEFTMETA      125\n' +
    '# KEY_RIGHTMETA     126\n' +
    '# Full list: https://github.com/torvalds/linux/blob/master/include/uapi/linux/input-event-codes.h',
  ];

  const params = [
    // --- Device ---
    {
      key: 'device',
      value: '/dev/input/event9',
      defaultValue: '/dev/input/event9',
      commented: true,
      'label-en': 'Device Path',
      'label-zh': '设备路径',
      'desc-en': 'Input device path, e.g. /dev/input/event9. If omitted, auto-detects the first active mouse.',
      'desc-zh': '输入设备路径，例如 /dev/input/event9。如果省略，将自动检测第一个活跃的鼠标设备。',
      type: 'string',
      group: 'device',
    },
    {
      key: 'free_spin_button',
      value: 273,
      defaultValue: 273,
      'label-en': 'Free Spin Button',
      'label-zh': 'Free Spin 按键',
      'desc-en': 'Mouse button code for Free Spin mode. Set to 0 to disable. When held during scrolling, inertia continues without auto-deceleration.',
      'desc-zh': 'Free Spin 模式的鼠标按键代码。设为 0 以禁用。滚动时按住此键，惯性将持续而不自动减速。',
      type: 'int',
      min: 0,
      max: 276,
      step: 1,
      group: 'device',
      enum: [0, 272, 273, 274, 275, 276],
      codeMap: 'button',
    },
    {
      key: 'drag_view_button',
      value: 272,
      defaultValue: 272,
      'label-en': 'Drag View Button',
      'label-zh': 'Drag View 按键',
      'desc-en': 'Mouse button code for Drag View mode. Set to 0 to disable. When held during scrolling, stops scroll and remaps mouse movement to panning.',
      'desc-zh': 'Drag View 模式的鼠标按键代码。设为 0 以禁用。滚动时按住此键，将停止滚动并将鼠标移动映射为平移。',
      type: 'int',
      min: 0,
      max: 276,
      step: 1,
      group: 'device',
      enum: [0, 272, 273, 274, 275, 276],
      codeMap: 'button',
    },
    // --- Scroll ---
    {
      key: 'smooth_mode',
      value: 0,
      defaultValue: 0,
      'label-en': 'Smooth Mode',
      'label-zh': '平滑模式',
      'desc-en': 'Selects the smoothing model. Speed mode adapts travel distance to wheel speed. Distance mode preserves wheel_tick_distance units per accepted wheel event.',
      'desc-zh': '选择平滑模型。速度模式会根据滚轮速度改变行程；距离模式会为每次有效滚轮事件保留 wheel_tick_distance 指定的距离。',
      type: 'int',
      min: 0,
      max: 1,
      step: 1,
      group: 'scroll',
      enum: [0, 1],
      'enum-labels-en': '0=Speed mode|1=Distance mode',
      'enum-labels-zh': '0=速度模式|1=距离模式',
      enumLabels: {
        en: { 0: 'Speed mode', 1: 'Distance mode' },
        zh: { 0: '速度模式', 1: '距离模式' },
      },
    },
    {
      key: 'wheel_tick_distance',
      value: 120,
      defaultValue: 120,
      'label-en': 'Wheel Tick Distance',
      'label-zh': '单次滚轮距离',
      'desc-en': 'Distance budget added by each accepted wheel event in Distance mode. Native high-resolution wheel distance is usually 120.',
      'desc-zh': '距离模式下每次有效滚轮事件增加的距离预算。原生高分辨率滚轮距离通常为 120。',
      type: 'int',
      min: 40,
      max: 240,
      step: 1,
      group: 'scroll',
      modes: ['distance'],
    },
    {
      key: 'damping',
      value: 3.1,
      defaultValue: 3.1,
      'label-en': 'Damping',
      'label-zh': '滚动阻尼',
      'desc-en': 'Controls how quickly speed decays over time. Higher values decelerate faster. If 0, only min_deceleration applies.',
      'desc-zh': '控制滚动速度随时间衰减的快慢。值越大减速越快。为 0 时仅受 min_deceleration 影响。',
      type: 'float',
      min: 0,
      max: 6,
      step: 0.1,
      group: 'scroll',
    },
    {
      key: 'min_deceleration',
      value: 1420,
      defaultValue: 1420,
      'label-en': 'Min Deceleration',
      'label-zh': '最小减速度',
      'desc-en': 'Minimum deceleration rate. Higher values decelerate faster at low speeds. Ensures scrolling always slows down. Must be <= max_deceleration.',
      'desc-zh': '最小减速度。值越大在低速时减速越快。确保滚动始终会减速。必须 <= max_deceleration。',
      type: 'float',
      min: 100,
      max: 10000,
      step: 10,
      group: 'scroll',
    },
    {
      key: 'max_deceleration',
      value: 6000,
      defaultValue: 6000,
      'label-en': 'Max Deceleration',
      'label-zh': '最大减速度',
      'desc-en': 'Maximum deceleration rate. Smaller values reduce deceleration at high speeds, making fast scrolls last longer and travel farther. Must be >= min_deceleration.',
      'desc-zh': '最大减速度。值越小高速时减速越弱，使快速滚动持续更久、行程更远。必须 >= min_deceleration。',
      type: 'float',
      min: 100,
      max: 30000,
      step: 10,
      group: 'scroll',
    },
    {
      key: 'initial_speed',
      value: 600,
      defaultValue: 600,
      'label-en': 'Initial Speed',
      'label-zh': '初始速度',
      'desc-en': 'Speed when scroll starts. Higher values increase distance per wheel tick. Combine with min_deceleration to tune feel.',
      'desc-zh': '滚动开始时的初始速度。值越大每次滚轮行程越远。可与 min_deceleration 配合调整手感。',
      type: 'float',
      min: 200,
      max: 1200,
      step: 10,
      group: 'scroll',
      modes: ['speed'],
    },
    {
      key: 'speed_factor',
      value: 40,
      defaultValue: 40,
      'label-en': 'Speed Factor',
      'label-zh': '速度因子',
      'desc-en': 'Multiplier for scroll speed. Higher values increase speed. Works with speed_smooth_window to compute speed from event intervals.',
      'desc-zh': '滚动速度的乘数。值越大速度越快。与 speed_smooth_window 配合，根据事件间隔计算速度。',
      type: 'float',
      min: 20,
      max: 120,
      step: 1,
      group: 'scroll',
      modes: ['speed'],
    },
    // --- Braking ---
    {
      key: 'use_reverse_scroll_braking',
      value: true,
      defaultValue: true,
      'label-en': 'Reverse Scroll Braking',
      'label-zh': '反向滚动制动',
      'desc-en': 'Enable braking when scroll direction reverses. Immediately stops current scroll when the opposite direction is detected.',
      'desc-zh': '启用反向滚动制动。检测到相反方向时立即停止当前滚动。',
      type: 'bool',
      group: 'braking',
    },
    {
      key: 'max_reverse_scroll_braking_microseconds',
      value: 100000,
      defaultValue: 100000,
      'label-en': 'Max Reverse Brake Time',
      'label-zh': '反向制动时间窗口',
      'desc-en': 'Time window (in microseconds) after a reverse brake. Opposite-direction events in this window are absorbed by reverse braking instead of emitting scroll immediately.',
      'desc-zh': '反向制动后的时间窗口（微秒）。此窗口内的反向事件会被反向制动吸收，不会立即输出滚动。',
      type: 'int',
      min: 0,
      max: 200000,
      step: 10000,
      group: 'braking',
      unit: 'us',
      'depends-on': 'use_reverse_scroll_braking',
    },
    {
      key: 'max_reverse_scroll_braking_times',
      value: 3,
      defaultValue: 3,
      'label-en': 'Max Reverse Brake Count',
      'label-zh': '反向制动最大次数',
      'desc-en': 'Maximum number of reverse scroll braking events before resuming normal scrolling.',
      'desc-zh': '恢复正常滚动前的最大反向制动次数。',
      type: 'int',
      min: 1,
      max: 5,
      step: 1,
      group: 'braking',
      'depends-on': 'use_reverse_scroll_braking',
    },
    {
      key: 'use_mouse_movement_braking',
      value: true,
      defaultValue: true,
      'label-en': 'Mouse Movement Braking',
      'label-zh': '鼠标移动制动',
      'desc-en': 'Enable braking when the mouse is moved during scrolling. Moving the mouse beyond the threshold distance stops the scroll.',
      'desc-zh': '启用鼠标移动制动。滚动期间移动鼠标超过阈值距离时停止滚动。',
      type: 'bool',
      group: 'braking',
    },
    {
      key: 'max_mouse_movement_distance',
      value: 30,
      defaultValue: 30,
      'label-en': 'Max Mouse Movement Distance',
      'label-zh': '鼠标移动距离阈值',
      'desc-en': 'Maximum 2D mouse movement distance within the time window before scrolling stops. Lower values make braking more sensitive.',
      'desc-zh': '在时间窗口内的最大鼠标二维移动距离，超过则停止滚动。值越小制动越灵敏。',
      type: 'int',
      min: 0,
      max: 200,
      step: 1,
      group: 'braking',
      'depends-on': 'use_mouse_movement_braking',
    },
    {
      key: 'mouse_movement_window_milliseconds',
      value: 20,
      defaultValue: 20,
      'label-en': 'Mouse Movement Window',
      'label-zh': '鼠标移动检测窗口',
      'desc-en': 'Sliding time window (in milliseconds) for tracking recent mouse movements used for braking detection.',
      'desc-zh': '用于制动检测的滑动时间窗口（毫秒），跟踪最近的鼠标移动。',
      type: 'int',
      min: 10,
      max: 50,
      step: 10,
      group: 'braking',
      unit: 'ms',
      'depends-on': 'use_mouse_movement_braking',
    },
    {
      key: 'mouse_movement_delay_microseconds',
      value: 100000,
      defaultValue: 100000,
      'label-en': 'Mouse Movement Delay',
      'label-zh': '鼠标移动检测延迟',
      'desc-en': 'Delay (in microseconds) after the last wheel event before mouse movements begin accumulating for braking detection.',
      'desc-zh': '最后一次滚轮事件后的延迟（微秒），之后鼠标移动才开始累积用于制动检测。',
      type: 'int',
      min: 0,
      max: 200000,
      step: 10000,
      group: 'braking',
      unit: 'us',
      'depends-on': 'use_mouse_movement_braking',
    },
    {
      key: 'keyboard_braking_keys',
      value: [42, 54],
      defaultValue: [42, 54],
      'label-en': 'Keyboard Braking Keys',
      'label-zh': '键盘制动键',
      'desc-en': 'Key codes that immediately stop scrolling when pressed or released. Default: Left Shift (42) and Right Shift (54).',
      'desc-zh': '按下或释放时立即停止滚动的键码。默认：左 Shift (42) 和右 Shift (54)。',
      type: 'int-array',
      group: 'braking',
      codeMap: 'key',
      presets: KEY_PRESETS,
    },
    {
      key: 'keyboard_passthrough_keys',
      value: [29, 97, 125, 126],
      defaultValue: [29, 97, 125, 126],
      'label-en': 'Keyboard Passthrough Keys',
      'label-zh': '键盘直通键',
      'desc-en': 'Key codes that enable passthrough mode while held. Scroll events pass through unmodified when these keys are active. Default: Ctrl and Super keys.',
      'desc-zh': '按住时启用直通模式的键码。这些键激活时滚轮事件将原样通过。默认：Ctrl 和 Super 键。',
      type: 'int-array',
      group: 'braking',
      codeMap: 'key',
      presets: KEY_PRESETS,
    },
    // --- Drag View ---
    {
      key: 'drag_view_speed',
      value: 3,
      defaultValue: 3,
      'label-en': 'Drag View Speed',
      'label-zh': 'Drag View 速度',
      'desc-en': 'Speed multiplier for panning in Drag View mode. Higher values make panning faster relative to mouse movement.',
      'desc-zh': 'Drag View 模式下的平移速度倍数。值越大相对于鼠标移动的平移越快。',
      type: 'int',
      min: 1,
      max: 10,
      step: 1,
      group: 'drag-view',
    },
    // --- Advanced ---
    {
      key: 'tick_interval_microseconds',
      value: 2000,
      defaultValue: 2000,
      'label-en': 'Tick Interval',
      'label-zh': '滴答间隔',
      'desc-en': 'Interval between synthetic scroll event generation, in microseconds. Smaller values produce smoother output but increase CPU usage.',
      'desc-zh': '合成滚动事件的生成间隔（微秒）。值越小输出越平滑但 CPU 占用越高。',
      type: 'int',
      min: 1000,
      max: 10000,
      step: 100,
      group: 'advanced',
      unit: 'us',
    },
    {
      key: 'speed_smooth_window_microseconds',
      value: 200000,
      defaultValue: 200000,
      'label-en': 'Speed Smooth Window',
      'label-zh': '速度平滑窗口',
      'desc-en': 'Sliding time window (in microseconds) for computing average event interval for speed estimation. Larger values smooth out speed fluctuations.',
      'desc-zh': '用于计算平均事件间隔以估计速度的滑动时间窗口（微秒）。值越大速度波动越平滑。',
      type: 'int',
      min: 0,
      max: 250000,
      step: 10000,
      group: 'advanced',
      unit: 'us',
      modes: ['speed'],
    },
    {
      key: 'max_speed_change_lowerbound',
      value: 512,
      defaultValue: 512,
      'label-en': 'Max Speed Change Lowerbound',
      'label-zh': '速度变化下界的最大值',
      'desc-en': 'Limits speed lowerbound increase to at most this much.',
      'desc-zh': '限制速度下界最多增长此值。',
      type: 'float',
      min: 0,
      max: 5000,
      step: 10,
      group: 'advanced',
      modes: ['speed'],
    },
    {
      key: 'min_speed_change_upperbound',
      value: 512,
      defaultValue: 512,
      'label-en': 'Min Speed Change Upperbound',
      'label-zh': '速度变化上界的最小值',
      'desc-en': 'Limits speed upperbound increase to at least this much.',
      'desc-zh': '限制速度上界最少增长此值。',
      type: 'float',
      min: 0,
      max: 5000,
      step: 10,
      group: 'advanced',
      modes: ['speed'],
    },
    {
      key: 'min_speed_change_ratio',
      value: 0.0625,
      defaultValue: 0.0625,
      'label-en': 'Min Speed Change Ratio',
      'label-zh': '最小速度变化比例',
      'desc-en': 'Minimum speed change ratio per wheel event. The actual change is min(speed * ratio, lowerbound). Must be <= max_speed_change_ratio.',
      'desc-zh': '每次滚轮事件的最小速度变化比例。实际变化为 min(speed * ratio, lowerbound)。必须 <= max_speed_change_ratio。',
      type: 'float',
      min: 0,
      max: 0.25,
      step: 0.01,
      group: 'advanced',
      modes: ['speed'],
    },
    {
      key: 'max_speed_change_ratio',
      value: 0.5,
      defaultValue: 0.5,
      'label-en': 'Max Speed Change Ratio',
      'label-zh': '最大速度变化比例',
      'desc-en': 'Maximum speed change ratio per wheel event. The actual change is max(speed * ratio, upperbound). Must be >= min_speed_change_ratio.',
      'desc-zh': '每次滚轮事件的最大速度变化比例。实际变化为 max(speed * ratio, upperbound)。必须 >= min_speed_change_ratio。',
      type: 'float',
      min: 0.25,
      max: 1,
      step: 0.01,
      group: 'advanced',
      modes: ['speed'],
    },
  ];

  function getCodeMap(name) {
    if (name === 'button') return BUTTON_CODES;
    if (name === 'key') return KEY_CODES;
    return null;
  }

  function resolveCode(codeMapName, token) {
    const trimmed = token.trim();
    const map = getCodeMap(codeMapName);
    if (map) {
      for (const [code, name] of Object.entries(map)) {
        if (name === trimmed) return Number(code);
      }
    }
    const n = parseInt(trimmed, 10);
    return isNaN(n) ? null : n;
  }

  function getGroups(paramsList) {
    const groups = new Map();
    for (const param of (paramsList || params)) {
      const group = param.group || 'other';
      if (!groups.has(group)) groups.set(group, []);
      groups.get(group).push(param);
    }
    return groups;
  }

  function getDefaultValues() {
    const defaults = {};
    for (const param of params) {
      if (!param.commented) {
        defaults[param.key] = Array.isArray(param.defaultValue) ? [...param.defaultValue] : param.defaultValue;
      }
    }
    return defaults;
  }

  return { params, references: REFERENCES, getGroups, getCodeMap, resolveCode, getDefaultValues, BUTTON_CODES, KEY_CODES, KEY_LABELS, KEY_PRESETS, KEY_CNT, CONFIG_VERSION };
})();
