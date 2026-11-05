/** Tabs expose shared device settings and editable profiles. Explanations appear only where an action needs them. */
const ProfileUI = (() => {
  let deviceSelected = false;
  const t = (zh, en) => I18n.lang() === 'zh' ? zh : en;
  const nameOf = item => item.name === 'Compatibility' ? t('兼容透传', 'Compatibility')
    : item.name || (item.id === 0 ? t('全局', 'Global') : t('未命名', 'Untitled'));
  function load() {
    ConfigEditor.loadValues(Profiles.values(), Profiles.editorState());
    render();
  }
  function focusTab() {
    document.getElementById(`profile-tab-${deviceSelected ? 'device' : Profiles.selected()}`)?.focus();
  }
  function activate(id) {
    deviceSelected = id === 'device';
    if (!deviceSelected) Profiles.select(id);
    load();
    focusTab();
  }
  function iconButton(symbol, label, action) {
    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'profile-icon';
    if (symbol === '✎' || symbol === '+') {
      const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
      const size = symbol === '+' ? '20' : '15';
      svg.setAttribute('viewBox', '0 0 24 24'); svg.setAttribute('width', size); svg.setAttribute('height', size);
      svg.setAttribute('aria-hidden', 'true'); svg.setAttribute('fill', 'none'); svg.setAttribute('stroke', 'currentColor');
      svg.setAttribute('stroke-width', '1.7'); svg.setAttribute('stroke-linejoin', 'round');
      const path = document.createElementNS('http://www.w3.org/2000/svg', 'path');
      path.setAttribute('d', symbol === '+' ? 'M12 5v14M5 12h14' : 'M15 5l4 4M4 20l5-1L20 8a2.8 2.8 0 0 0-4-4L5 15z');
      svg.append(path); button.append(svg);
    } else button.textContent = symbol;
    button.title = label;
    button.setAttribute('aria-label', label);
    button.addEventListener('click', action);
    return button;
  }
  function rename(item) {
    if (item.id < 2) return;
    const wrap = document.getElementById(`profile-tab-${item.id}`).parentElement;
    const input = document.createElement('input');
    input.className = 'profile-rename';
    input.setAttribute('aria-label', t('Profile 名称', 'Profile name'));
    input.value = nameOf(item);
    let done = false;
    const finish = save => {
      if (done) return;
      done = true;
      if (save) Profiles.setName(input.value.trim());
      render();
      updateTomlPreview(ConfigEditor.getValues());
      focusTab();
    };
    input.addEventListener('blur', () => finish(true));
    input.addEventListener('keydown', event => {
      if (event.key === 'Enter' || event.key === 'Escape') {
        event.preventDefault();
        finish(event.key === 'Enter');
      }
    });
    wrap.append(input);
    input.focus();
    input.select();
  }
  function render() {
    const root = document.getElementById('profile-picker');
    if (!root) return;
    root.replaceChildren();
    const rail = document.createElement('div'); rail.className = 'profile-rail';
    const tabs = document.createElement('div'); tabs.className = 'profile-tabs';
    tabs.setAttribute('role', 'tablist'); tabs.setAttribute('aria-label', t('配置', 'Configuration'));
    const items = [{ id: 'device', name: t('设备', 'Device') }, ...Profiles.all()];
    for (const item of items) {
      const device = item.id === 'device';
      const active = device ? deviceSelected : !deviceSelected && item.id === Profiles.selected();
      const wrap = document.createElement('div'); wrap.className = 'profile-tab-wrap';
      wrap.classList.toggle('active', active); wrap.setAttribute('role', 'presentation');
      const tab = document.createElement('button'); tab.type = 'button';
      tab.className = 'profile-tab'; tab.id = `profile-tab-${item.id}`;
      tab.setAttribute('role', 'tab'); tab.setAttribute('aria-selected', String(active));
      tab.setAttribute('aria-controls', device ? 'device-settings' : 'editor-params'); tab.tabIndex = active ? 0 : -1;
      const badge = document.createElement('span'); badge.className = 'profile-id'; badge.textContent = item.id;
      const title = document.createElement('span'); title.className = 'profile-tab-name'; title.textContent = nameOf(item);
      if (!device) tab.append(badge);
      tab.append(title);
      tab.addEventListener('click', () => { if (!active) activate(item.id); });
      if (item.id > 1) tab.addEventListener('dblclick', () => rename(Profiles.all()[Profiles.selected()]));
      tab.addEventListener('keydown', event => {
        const index = items.indexOf(item);
        const next = event.key === 'ArrowRight' ? (index + 1) % items.length
          : event.key === 'ArrowLeft' ? (index + items.length - 1) % items.length
          : event.key === 'Home' ? 0 : event.key === 'End' ? items.length - 1 : null;
        if (next !== null) { event.preventDefault(); activate(items[next].id); }
        if (event.key === 'F2' && item.id > 1) { event.preventDefault(); rename(item); }
      });
      wrap.append(tab);
      if (active && item.id > 1) wrap.append(iconButton('✎', t('重命名 Profile', 'Rename profile'), () => rename(item)));
      if (active && item.id > 1) wrap.append(iconButton('×', t('删除 Profile', 'Remove profile'), () => { Profiles.remove(); load(); focusTab(); }));
      tabs.append(wrap);
    }
    rail.append(tabs, iconButton('+', t('新增 Profile', 'Add profile'), () => {
      deviceSelected = false;
      Profiles.add(); load(); rename(Profiles.all()[Profiles.selected()]);
    }));
    root.append(rail);
    tabs.querySelector('[aria-selected="true"]')?.scrollIntoView({ block: 'nearest', inline: 'nearest' });
    const panel = document.getElementById('editor-params');
    panel.hidden = deviceSelected;
    document.getElementById('device-settings').hidden = !deviceSelected;
    PresetUI.updateStatus();
    panel.setAttribute('role', 'tabpanel'); panel.setAttribute('aria-labelledby', `profile-tab-${Profiles.selected()}`);
  }
  return { render, isDeviceSelected: () => deviceSelected };
})();
