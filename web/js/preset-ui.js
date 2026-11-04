/** Presets load once; editor changes are never constrained by their source. */
const PresetUI = (() => {
  let activeId = 'inertia';
  let baseline = null;
  const t = (zh, en) => I18n.lang() === 'zh' ? zh : en;
  const local = value => value[I18n.lang()];
  function dirty() {
    if (!baseline) return false;
    const values = ConfigEditor.getValues();
    return ParamSchema.params.some(p => JSON.stringify(values[p.key]) !== JSON.stringify(baseline[p.key]));
  }
  function load(id) {
    activeId = ConfigPresets.get(id).id;
    baseline = null;
    ConfigEditor.loadValues(ConfigPresets.values(activeId));
    baseline = structuredClone(ConfigEditor.getValues());
    const url = new URL(window.location.href);
    url.searchParams.set('preset', activeId);
    window.history.replaceState(null, '', url);
    renderEditor();
  }
  function updateStatus() {
    const select = document.getElementById('preset-select');
    if (!select) return;
    const modified = dirty();
    for (const option of select.options) {
      option.textContent = local(ConfigPresets.get(option.value).name);
    }
    const reset = document.getElementById('btn-reset');
    if (reset) reset.hidden = !modified;
    select.title = local(ConfigPresets.get(activeId).description);
    const status = document.getElementById('config-status');
    if (status) {
      status.textContent = t('预设：', 'Preset: ') + local(ConfigPresets.get(activeId).name)
        + (modified ? t(' · 已修改', ' · Modified') : '');
      status.title = t('基于：', 'Based on: ') + local(ConfigPresets.get(activeId).name);
    }
  }

  function renderEditor() {
    const container = document.getElementById('preset-picker');
    if (!container) return;
    container.innerHTML = '';
    const label = document.createElement('label');
    label.htmlFor = 'preset-select';
    label.textContent = t('预设', 'Preset');
    const select = document.createElement('select');
    select.id = 'preset-select';
    select.setAttribute('aria-label', t('选择预设', 'Choose a preset'));
    select.className = 'param-input select';
    for (const preset of ConfigPresets.items) {
      const option = document.createElement('option');
      option.value = preset.id;
      option.textContent = local(preset.name);
      select.appendChild(option);
    }
    select.value = activeId;
    select.addEventListener('change', () => {
      load(select.value);
      document.getElementById('preset-select').focus();
    });
    container.append(label, select);
    document.getElementById('btn-reset').textContent = t('重置', 'Reset');
    updateStatus();
  }
  function initEditor() {
    load(new URLSearchParams(window.location.search).get('preset'));
  }
  function restore() {
    load(activeId);
    document.getElementById('preset-select').focus();
  }
  function renderQuickstart() {
    const container = document.getElementById('preset-list');
    container.innerHTML = '';
    for (const preset of ConfigPresets.items) {
      const article = document.createElement('article');
      article.className = 'preset-option';
      const title = document.createElement('h3');
      title.textContent = local(preset.name);
      const description = document.createElement('p');
      description.textContent = local(preset.description);
      const actions = document.createElement('div');
      actions.className = 'preset-actions';
      const download = document.createElement('button');
      download.className = 'btn btn-primary';
      download.textContent = I18n.t('btn.download');
      download.addEventListener('click', () => {
        ConfigWriter.download(ConfigWriter.generate(ConfigPresets.values(preset.id)), 'smooth-scroll.toml');
      });
      actions.appendChild(download);
      article.append(title, description, actions);
      container.appendChild(article);
    }
  }
  function initQuickstart() {
    ConfigWriter.init(ParamSchema);
    renderQuickstart();
    document.getElementById('lang-toggle').addEventListener('click', renderQuickstart);
  }
  return { initEditor, renderEditor, updateStatus, restore, initQuickstart };
})();
