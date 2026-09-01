/**
 * Simple i18n module. Language is stored in localStorage.
 * Labels and descriptions come from the static param schema.
 */
// eslint-disable-next-line no-unused-vars
const I18n = (() => {
  let currentLang = localStorage.getItem('lang') || 'en';

  function lang() {
    return currentLang;
  }

  function setLang(lang) {
    currentLang = lang;
    localStorage.setItem('lang', lang);
  }

  function label(param) {
    return currentLang === 'zh' ? (param['label-zh'] || param['label-en'] || param.key) : (param['label-en'] || param.key);
  }

  function desc(param) {
    return currentLang === 'zh' ? (param['desc-zh'] || param['desc-en'] || '') : (param['desc-en'] || '');
  }

  function t(key) {
    const translations = {
      'app.title': { en: 'Smooth Scroll Configurator', zh: 'Smooth Scroll 配置器' },
      'btn.copy': { en: 'Copy TOML', zh: '复制 TOML' },
      'btn.download': { en: 'Download .toml', zh: '下载 .toml' },
      'btn.reset': { en: 'Reset to Defaults', zh: '恢复默认值' },
      'btn.copied': { en: 'Copied!', zh: '已复制！' },
      'toml.preview': { en: 'TOML Preview', zh: 'TOML 预览' },
      'group.device': { en: 'Device', zh: '设备' },
      'group.scroll': { en: 'Scroll', zh: '滚动' },
      'group.braking': { en: 'Braking', zh: '制动' },
      'group.drag-view': { en: 'Drag View', zh: '拖拽视图' },
      'group.auto-scroll': { en: 'Auto Scroll', zh: '自动滚动' },
      'group.advanced': { en: 'Advanced', zh: '高级' },
      'scenario.title': { en: 'Simulation Scenario', zh: '模拟场景' },
      'scenario.single': { en: 'Single Tick', zh: '单次滚动' },
      'scenario.slow': { en: 'Slow Scroll', zh: '慢速滚动' },
      'scenario.fast': { en: 'Fast Scroll', zh: '快速滚动' },
      'scenario.multi-burst': { en: 'Multi-Burst', zh: '多组快速' },
      'summary.distance': { en: 'Total Distance', zh: '总距离' },
      'summary.duration': { en: 'Duration', zh: '持续时间' },
      'summary.default': { en: 'default', zh: '默认' },
      'summary.current': { en: 'current', zh: '当前' },
      'chart.speed': { en: 'Speed vs Time', zh: '速度-时间曲线' },
      'chart.displacement': { en: 'Displacement vs Time', zh: '位移-时间曲线' },
      'chart.legend.default': { en: 'Default', zh: '默认' },
      'chart.legend.default-speed': { en: 'Default speed', zh: '默认速度模式' },
      'chart.legend.default-distance': { en: 'Default distance', zh: '默认距离模式' },
      'chart.legend.default-hybrid': { en: 'Default hybrid', zh: '默认混合模式' },
      'chart.legend.current': { en: 'Current', zh: '当前' },
      'unit.ms': { en: 'ms', zh: 'ms' },
      'validation.native-distance': { en: 'Single-tick distance differs from the native 120-unit wheel tick.', zh: '单次滚动距离不等于原生 120 单位滚轮距离。' },
      'validation.configured-distance': { en: 'Single-tick distance differs from the configured Wheel Tick Distance.', zh: '单次滚动距离不等于配置的单次滚轮距离。' },
      'validation.min-max-decel': { en: 'Min deceleration must be <= max deceleration', zh: '最小减速度必须 <= 最大减速度' },
      'validation.min-max-ratio': { en: 'Min speed change ratio must be <= max speed change ratio', zh: '最小速度变化比例必须 <= 最大速度变化比例' },
      'validation.mode-button-conflict': { en: 'Auto Scroll must use a different button from Free Spin and Drag View', zh: 'Auto Scroll 不得与 Free Spin 或 Drag View 使用相同按键' },
      'nav.feature': { en: 'Feature', zh: '功能' },
      'nav.quickstart': { en: 'Quick Start', zh: '快速上手' },
      'nav.config': { en: 'Customization', zh: '自定义' },
      'nav.faq': { en: 'FAQ', zh: 'FAQ' },
      'version.config': { en: 'Config Version', zh: '配置版本' },
      'key.add': { en: '+ Add key', zh: '+ 添加按键' },
      'key.code-placeholder': { en: 'code', zh: '键码' },
      'device.select-ignore': { en: '+ Select device to ignore', zh: '+ 选择要忽略的设备' },
      'device.none-ignored': { en: 'No models ignored — automatic discovery remains enabled.', zh: '未忽略任何型号——保持自动发现。' },
      'device.remove-ignore': { en: 'Stop ignoring this device model', zh: '不再忽略此设备型号' },
      'device.webhid-unsupported': { en: 'WebHID requires a supported Chromium browser.', zh: 'WebHID 需要受支持的 Chromium 浏览器。' },
      'device.webhid-limit': { en: 'Only devices exposed by WebHID are listed; standard-only mice and keyboards may be hidden.', zh: '仅显示 WebHID 可见的设备；只有标准接口的鼠标和键盘可能不会出现。' },
      'device.selection-failed': { en: 'Could not read the selected device.', zh: '无法读取所选设备。' },
      'device.unnamed': { en: 'Unnamed HID device', zh: '未命名 HID 设备' },
      'device.role-mouse': { en: 'Mouse', zh: '鼠标' },
      'device.role-keyboard': { en: 'Keyboard', zh: '键盘' },
      'device.role-composite': { en: 'Mouse + Keyboard', zh: '鼠标 + 键盘' },
      'device.role-hid': { en: 'HID', zh: 'HID' },
      'summary.truncated': { en: 'Simulation exceeded max time. Actual duration/distance may be longer.', zh: '模拟超过最大时间。实际持续时间/距离可能更长。' },
      'tips.label': { en: 'Tips:', zh: '提示：' },
      'next-steps.title': { en: 'Next Steps', zh: '后续步骤' },
      'next-steps.step1': { en: 'Copy the TOML config above (or download the .toml file).', zh: '复制上方的 TOML 配置（或下载 .toml 文件）。' },
      'next-steps.step2': { en: 'Edit or replace the config file at /etc/smooth-scroll/smooth-scroll.toml', zh: '编辑或替换配置文件 /etc/smooth-scroll/smooth-scroll.toml' },
      'next-steps.step3': { en: 'Restart the smooth-scroll daemon to apply changes.', zh: '重启 smooth-scroll 守护进程以应用更改。' },

      // Feature page
      'feature.hero.note': { en: 'System-wide smooth scrolling for Linux', zh: '适用于整个 Linux 桌面的平滑滚动' },
      'feature.hero.title': { en: 'One wheel tick should not feel like a jump.', zh: '滚轮每动一下，都不该像在跳格子。' },
      'feature.hero.subtitle': { en: 'Smooth Scroll turns raw mouse wheel ticks into continuous motion with a lightweight daemon and a real physics engine.', zh: 'Smooth Scroll 用轻量守护进程和真实物理引擎，把离散滚轮事件转换成连续、自然的滚动。' },
      'feature.smooth.title': { en: 'Smooth Scrolling', zh: '平滑滚动' },
      'feature.smooth.desc': { en: 'Raw wheel input usually arrives in hard 120-unit steps. Smooth Scroll can adapt travel to speed or preserve a configured distance while emitting smaller, timed steps.', zh: '原始滚轮输入通常是一段段 120 单位的跳变。Smooth Scroll 可以按速度调整行程，也可以保留配置的距离，并输出更细、更有节奏的滚动事件。' },
      'feature.smooth.off': { en: 'Native wheel', zh: '原生滚轮' },
      'feature.smooth.on': { en: 'Smooth Scroll on', zh: '开启 Smooth Scroll' },
      'feature.adaptive.kicker': { en: 'It reacts to your hand', zh: '速度跟随你的手感' },
      'feature.adaptive.title': { en: 'Slow means precise. Fast means far.', zh: '慢时精细，快时走得更远。' },
      'feature.adaptive.desc': { en: 'Speed mode measures how quickly wheel events arrive. Gentle scrolling stays controlled; quick bursts build speed, while Distance mode keeps each wheel tick at the configured distance.', zh: '速度模式会根据滚轮事件的间隔判断速度。轻轻滚动时更可控，快速连滚时会建立速度；距离模式则让每次滚轮保持配置的距离。' },
      'feature.adaptive.slow': { en: 'Slow roll', zh: '慢速滚动' },
      'feature.adaptive.fast': { en: 'Fast roll', zh: '快速滚动' },
      'feature.adaptive.burst': { en: 'Repeated burst', zh: '连续快速滚动' },
      'feature.adaptive.distance': { en: 'Distance', zh: '总距离' },
      'feature.adaptive.events': { en: 'Ticks', zh: '滚轮事件' },
      'feature.adaptive.avg': { en: 'Per tick', zh: '单次均值' },
      'feature.adaptive.mode': { en: 'Mode', zh: '模式' },
      'feature.freespin.kicker': { en: 'Let momentum carry the page', zh: '让页面自己保持惯性' },
      'feature.freespin.title': { en: 'Free Spin holds a clean cruise.', zh: '自由旋转，让滚动保持巡航。' },
      'feature.freespin.desc': { en: 'Press the Free Spin key while scrolling and the page keeps moving at a steady speed. Release it when you want normal damping back.', zh: '滚动时按下自由旋转按键，页面会以稳定速度继续移动。松开后恢复正常阻尼。' },
      'feature.freespin.with': { en: 'Free Spin', zh: '自由旋转' },
      'feature.dragview.kicker': { en: 'For wide panes and long views', zh: '为宽内容和长视图准备' },
      'feature.dragview.title': { en: 'Drag View turns movement into scrolling.', zh: '拖拽视图，把鼠标移动变成滚动。' },
      'feature.dragview.desc': { en: 'Hold a key, move the mouse, and scan content beyond the viewport without wrestling tiny scrollbars or trackpad gestures.', zh: '按住指定按键并移动鼠标，就能浏览视口之外的内容，不必和细小滚动条或触控板手势较劲。' },
      'feature.more.kicker': { en: 'Built for daily desktop use', zh: '为日常桌面使用而构建' },
      'feature.more.title': { en: 'Small daemon, system-wide feel.', zh: '小守护进程，改变整个桌面的手感。' },
      'feature.more.desc': { en: 'Smooth Scroll stays close to Linux input plumbing: configurable, desktop-agnostic, and light enough to leave running.', zh: 'Smooth Scroll 贴近 Linux 输入层工作：可配置、不挑桌面环境，也足够轻量，适合长期运行。' },
      'feature.more.config.title': { en: 'Tune the physics', zh: '调整物理参数' },
      'feature.more.config.desc': { en: 'Edit parameters in the web configurator, preview the motion, then export TOML.', zh: '在 Web 配置器中编辑参数，预览滚动效果，然后导出 TOML。' },
      'feature.more.compat.title': { en: 'Works across desktops', zh: '跨桌面环境可用' },
      'feature.more.compat.desc': { en: 'GNOME, KDE, Xfce, X11, and Wayland are handled through libevdev and uinput.', zh: '通过 libevdev 与 uinput 支持 GNOME、KDE、Xfce、X11 和 Wayland。' },
      'feature.more.light.title': { en: 'Light by design', zh: '设计上保持轻量' },
      'feature.more.light.desc': { en: 'A dedicated real-time C++ input loop, a tiny binary, low CPU usage, and no runtime dependency stack.', zh: '独立的 C++ 实时输入循环，二进制体积小，CPU 占用低，无复杂运行时依赖。' },
      'feature.more.ipc.title': { en: 'Ready for integrations', zh: '可接入桌面集成' },
      'feature.more.ipc.desc': { en: 'Shared-memory IPC exposes daemon state for desktop controls and visualization plugins.', zh: '通过共享内存 IPC 暴露守护进程状态，便于桌面控制与可视化插件集成。' },
      'feature.timeline.scroll-up': { en: 'Wheel up', zh: '滚轮向上' },
      'feature.timeline.scroll-down': { en: 'Wheel tick', zh: '滚轮事件' },
      'feature.timeline.scroll-right': { en: 'Right scroll', zh: '向右滚动' },
      'feature.timeline.scroll-left': { en: 'Left scroll', zh: '向左滚动' },
      'feature.timeline.free-spin': { en: 'Hold Free Spin', zh: '按住自由旋转' },
      'feature.timeline.free-spin-release': { en: 'Release', zh: '松开按键' },
      'feature.timeline.drag-view': { en: 'Hold Drag View', zh: '按住拖拽视图' },
      'feature.timeline.drag-view-release': { en: 'Release', zh: '松开按键' },
      'feature.timeline.mouse-move': { en: 'Move mouse', zh: '移动鼠标' },
    };
    const entry = translations[key];
    if (!entry) return key;
    return currentLang === 'zh' ? (entry.zh || entry.en) : (entry.en || entry.zh);
  }

  return { lang, setLang, label, desc, t };
})();
