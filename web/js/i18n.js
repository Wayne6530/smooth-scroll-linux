/**
 * Simple i18n module. Language is stored in localStorage.
 * Labels and descriptions come from the static param schema.
 */
// eslint-disable-next-line no-unused-vars
const I18n = (() => {
  let currentLang = localStorage.getItem('lang') || 'zh';

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
      'group.device': { en: 'Device', zh: '设备' },
      'group.scroll': { en: 'Scroll', zh: '滚动' },
      'group.braking': { en: 'Braking', zh: '制动' },
      'group.drag-view': { en: 'Drag View', zh: '拖拽视图' },
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
      'chart.legend.current': { en: 'Current', zh: '当前' },
      'unit.ms': { en: 'ms', zh: 'ms' },
      'validation.native-distance': { en: 'Single-tick distance differs from native 120. This changes the base scroll amount.', zh: '单次滚动距离不等于原生值 120。这会改变基础滚动量。' },
      'validation.min-max-decel': { en: 'Min deceleration must be <= max deceleration', zh: '最小减速度必须 <= 最大减速度' },
      'validation.min-max-ratio': { en: 'Min speed change ratio must be <= max speed change ratio', zh: '最小速度变化比例必须 <= 最大速度变化比例' },
      'nav.feature': { en: 'Feature', zh: '功能' },
      'nav.quickstart': { en: 'Quick Start', zh: '快速上手' },
      'nav.config': { en: 'Customization', zh: '自定义' },
      'nav.faq': { en: 'FAQ', zh: 'FAQ' },
      'version.config': { en: 'Config Version', zh: '配置版本' },
      'key.add': { en: '+ Add key', zh: '+ 添加按键' },
      'key.code-placeholder': { en: 'code', zh: '键码' },
      'summary.truncated': { en: 'Simulation exceeded max time. Actual duration/distance may be longer.', zh: '模拟超过最大时间。实际持续时间/距离可能更长。' },
      'tips.label': { en: 'Tips:', zh: '提示：' },
      'next-steps.title': { en: 'Next Steps', zh: '后续步骤' },
      'next-steps.step1': { en: 'Copy the TOML config above (or download the .toml file).', zh: '复制上方的 TOML 配置（或下载 .toml 文件）。' },
      'next-steps.step2': { en: 'Edit or replace the config file at /etc/smooth-scroll/smooth-scroll.toml', zh: '编辑或替换配置文件 /etc/smooth-scroll/smooth-scroll.toml' },
      'next-steps.step3': { en: 'Restart the smooth-scroll daemon to apply changes.', zh: '重启 smooth-scroll 守护进程以应用更改。' },
    };
    const entry = translations[key];
    if (!entry) return key;
    return currentLang === 'zh' ? (entry.zh || entry.en) : (entry.en || entry.zh);
  }

  return { lang, setLang, label, desc, t };
})();
