/** Presets load once; editor changes are never constrained by their source. */
const PresetUI = (() => {
  const t = (zh, en) => I18n.lang() === 'zh' ? zh : en;
  const local = value => value[I18n.lang()];
  function load(id) {
    Profiles.applyPreset(id);
    ConfigEditor.loadValues(Profiles.values());
    Profiles.captureBaseline();
    renderEditor();
  }
  function updateStatus() {
    if (!Profiles.ready()) return;
    const select = document.getElementById('preset-select');
    if (!select) return;
    const activeId = Profiles.presetId();
    const modified = Profiles.modified();
    const deviceSelected = ProfileUI.isDeviceSelected();
    for (const option of select.options) {
      option.textContent = local(ProfilePresets.get(option.value).name);
    }
    const reset = document.getElementById('btn-reset');
    if (reset) reset.hidden = deviceSelected || Profiles.selected() === 1 || !modified;
    select.title = local(ProfilePresets.get(activeId).description);
    const status = document.getElementById('config-status');
    if (status) {
      const presetName = local(ProfilePresets.get(activeId).name);
      status.textContent = deviceSelected
        ? t('设备设置适用于所有 Profile', 'Device settings apply to all profiles')
        : modified ? t(`Profile ${Profiles.selected()} · ${presetName} · 已修改`, `Profile ${Profiles.selected()} · ${presetName} · Modified`)
        : t('所有 Profile 将一并导出', 'Exports all profiles');
      status.title = '';
    }
  }

  function renderEditor() {
    const container = document.getElementById('preset-picker');
    if (!container || !Profiles.ready()) return;
    container.innerHTML = '';
    const profile = document.createElement('span');
    profile.className = 'preset-profile';
    profile.textContent = `Profile ${Profiles.selected()}`;
    const controls = document.createElement('div');
    controls.className = 'preset-controls';
    const label = document.createElement('label');
    label.htmlFor = 'preset-select';
    label.textContent = t('预设', 'Preset');
    const select = document.createElement('select');
    select.id = 'preset-select';
    select.setAttribute('aria-label', t('选择 Profile 预设', 'Choose a profile preset'));
    select.className = 'param-input select';
    for (const preset of ProfilePresets.items) {
      const option = document.createElement('option');
      option.value = preset.id;
      option.textContent = local(preset.name);
      select.appendChild(option);
    }
    select.value = Profiles.presetId();
    select.addEventListener('change', () => {
      load(select.value);
      document.getElementById('preset-select').focus();
    });
    controls.append(label, select);
    container.append(profile, controls);
    document.getElementById('btn-reset').textContent = 'Reset';
    document.getElementById('btn-reset').title = t(`恢复 Profile ${Profiles.selected()} 的预设参数`, `Restore Profile ${Profiles.selected()}’s starting values`);
    document.getElementById('btn-reset').setAttribute('aria-label', t(`重置 Profile ${Profiles.selected()}`, `Reset Profile ${Profiles.selected()}`));
    updateStatus();
  }
  function initEditor() {
    Profiles.init(ConfigPresets.values(new URLSearchParams(window.location.search).get('preset')));
    ConfigEditor.loadValues(Profiles.values());
    Profiles.captureBaseline();
    renderEditor();
  }
  function restore() {
    Profiles.restore();
    ConfigEditor.loadValues(Profiles.values());
    renderEditor();
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
        ConfigWriter.download(ConfigWriter.generateConfig(ConfigPresets.values(preset.id)), 'smooth-scroll.toml');
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
