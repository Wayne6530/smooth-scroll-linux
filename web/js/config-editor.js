/**
 * Renders the parameter editor UI from a static schema.
 * Supports key-name display for button/key codes and constraint validation.
 */
// eslint-disable-next-line no-unused-vars
const ConfigEditor = (() => {
  let currentValues = {};
  let onParamChange = null;
  let params = [];
  let instant = false;
  let savedSmoothValues = null;

  const GROUP_ORDER = ['device', 'scroll', 'braking', 'drag-view', 'auto-scroll', 'advanced'];

  function getEffectiveValue(param) {
    const current = currentValues[param.key];
    return current !== undefined ? current : param.value;
  }

  function getControlValue(param) {
    const value = getEffectiveValue(param);
    return param.uiAbs ? Math.abs(Number(value)) : value;
  }

  function init(schema, onChange) {
    params = schema.params;
    onParamChange = onChange;
    currentValues = {};
    for (const param of params) {
      currentValues[param.key] = param.commented ? undefined : param.value;
    }
    instant = InstantScroll.matches(currentValues);
    savedSmoothValues = null;
    if (instant) InstantScroll.apply(currentValues);
    render();
  }

  function createScrollEffectControl() {
    const wrap = document.createElement('div');
    wrap.className = 'scroll-effect';
    const row = document.createElement('div');
    row.className = 'param-row';
    const label = document.createElement('label');
    label.className = 'param-label';
    label.htmlFor = 'scroll-effect';
    label.textContent = I18n.t('scroll.effect');
    const select = document.createElement('select');
    select.id = 'scroll-effect';
    select.className = 'param-input select';
    for (const value of ['smooth', 'instant']) {
      const option = document.createElement('option');
      option.value = value;
      option.textContent = I18n.t('scroll.' + value);
      select.appendChild(option);
    }
    select.value = instant ? 'instant' : 'smooth';
    select.disabled = Boolean(currentValues.wheel_passthrough);
    select.addEventListener('change', () => {
      instant = select.value === 'instant';
      if (instant) {
        savedSmoothValues = Object.fromEntries(InstantScroll.managedKeys.map(key => [key, currentValues[key]]));
        InstantScroll.apply(currentValues);
      } else {
        const fallback = ParamSchema.getDefaultValues();
        for (const key of InstantScroll.managedKeys) {
          currentValues[key] = savedSmoothValues ? savedSmoothValues[key] : fallback[key];
        }
        savedSmoothValues = null;
      }
      render();
      document.getElementById('scroll-effect').focus();
      fireChange(null);
    });
    const controlWrap = document.createElement('div');
    controlWrap.className = 'param-control';
    controlWrap.append(select, createHelpTooltip(I18n.t(instant ? 'scroll.instant-description' : 'scroll.smooth-description')));
    row.append(label, controlWrap);
    wrap.appendChild(row);
    return wrap;
  }

  function render() {
    const container = document.getElementById('editor-params');
    container.disabled = typeof Profiles !== 'undefined' && Profiles.ready() && Profiles.selected() === 1;
    const collapsed = new Map(Array.from(document.querySelectorAll('.param-group')).map(section =>
      [section.dataset.group, section.querySelector('.group-content').classList.contains('collapsed')]));
    container.innerHTML = '';
    const deviceSettings = document.getElementById('device-settings');
    if (deviceSettings) deviceSettings.replaceChildren();
    document.querySelectorAll('.param-tooltip').forEach(el => el.remove());

    // Config version badge
    const versionEl = document.createElement('div');
    versionEl.className = 'config-version';
    versionEl.id = 'config-version';
    versionEl.setAttribute('aria-label', I18n.t('version.config') + ' ' + ParamSchema.CONFIG_VERSION);
    const picker = document.createElement('div');
    picker.id = 'preset-picker';
    picker.className = 'preset-picker';
    versionEl.appendChild(picker);
    container.appendChild(versionEl);
    if (typeof PresetUI !== 'undefined') PresetUI.renderEditor();

    const groups = ParamSchema.getGroups(params);
    const orderedGroups = GROUP_ORDER.filter(g => groups.has(g));

    for (const groupName of orderedGroups) {
      const groupParams = groups.get(groupName).filter(param => !instant || !InstantScroll.disabledKeys.has(param.key));
      if (!groupParams.length) continue;
      const section = createGroupSection(groupName, groupParams);
      (groupName === 'device' && deviceSettings ? deviceSettings : container).appendChild(section);
      if (groupName === 'device' && deviceSettings) {
        section.querySelector('.group-header').remove();
        section.querySelector('.group-content').classList.remove('collapsed');
      } else if (collapsed.has(groupName)) {
        section.querySelector('.group-content').classList.toggle('collapsed', collapsed.get(groupName));
        section.querySelector('.group-header').classList.toggle('collapsed', collapsed.get(groupName));
        section.querySelector('.group-header').setAttribute('aria-expanded', String(!collapsed.get(groupName)));
      }
    }
    validateConstraints();
  }

  function createGroupSection(groupName, groupParams) {
    const section = document.createElement('div');
    section.className = 'param-group';
    section.dataset.group = groupName;

    const header = document.createElement('div');
    header.className = 'group-header';
    header.textContent = groupName === 'device'
      ? (I18n.lang() === 'zh' ? '设备设置 · 所有 Profile' : 'Device settings · All profiles')
      : I18n.t('group.' + groupName);
    header.tabIndex = 0;
    header.setAttribute('role', 'button');
    header.setAttribute('aria-expanded', String(groupName === 'scroll'));
    header.addEventListener('keydown', event => {
      if (event.key === 'Enter' || event.key === ' ') { event.preventDefault(); header.click(); }
    });
    header.addEventListener('click', () => {
      const content = section.querySelector('.group-content');
      content.classList.toggle('collapsed');
      header.classList.toggle('collapsed');
      header.setAttribute('aria-expanded', String(!content.classList.contains('collapsed')));
    });

    const content = document.createElement('div');
    content.className = 'group-content';
    if (groupName !== 'scroll') {
      content.classList.add('collapsed');
      header.classList.add('collapsed');
    }

    if (groupName === 'scroll') {
      content.appendChild(createParamRow(groupParams.find(param => param.key === 'wheel_passthrough')));
      content.appendChild(createScrollEffectControl());
    }
    for (const param of groupParams) {
      if (groupName === 'scroll' && param.key === 'wheel_passthrough') continue;
      content.appendChild(createParamRow(param));
    }

    section.appendChild(header);
    section.appendChild(content);
    return section;
  }

  function createParamRow(param) {
    const row = document.createElement('div');
    row.className = 'param-row';
    row.dataset.key = param.key;
    if (param['depends-on']) {
      row.dataset.dependsOn = param['depends-on'];
    }

    // Label
    const labelEl = document.createElement('label');
    labelEl.className = 'param-label';
    labelEl.textContent = I18n.label(param);
    labelEl.htmlFor = 'param-' + param.key;

    // Control container
    const controlWrap = document.createElement('div');
    controlWrap.className = 'param-control';

    const control = createControl(param);
    controlWrap.appendChild(control);

    // Unit suffix
    if (param.unit) {
      const unitSpan = document.createElement('span');
      unitSpan.className = 'param-unit';
      unitSpan.textContent = param.unit;
      controlWrap.appendChild(unitSpan);
    }

    // Description tooltip
    const descText = instant && InstantScroll.disabledKeys.has(param.key)
      ? I18n.t('scroll.preset-disabled') + ' ' + I18n.desc(param) : I18n.desc(param);
    if (descText) controlWrap.appendChild(createHelpTooltip(descText));

    row.appendChild(labelEl);
    row.appendChild(controlWrap);

    updateParamState(row, param);
    return row;
  }

  function createHelpTooltip(text) {
    const helpBtn = document.createElement('span');
    helpBtn.className = 'param-help';
    helpBtn.textContent = '?';
    const tooltip = document.createElement('div');
    tooltip.className = 'param-tooltip';
    tooltip.textContent = text;
    document.body.appendChild(tooltip);
    helpBtn.addEventListener('mouseenter', () => {
      const rect = helpBtn.getBoundingClientRect();
      tooltip.style.top = (rect.bottom + 6) + 'px';
      tooltip.style.left = Math.max(8, Math.min(rect.left, window.innerWidth - 256)) + 'px';
      tooltip.classList.add('visible');
    });
    helpBtn.addEventListener('mouseleave', () => tooltip.classList.remove('visible'));
    return helpBtn;
  }

  function createControl(param) {
    if (param.type === 'hid-device-list') return createHidDeviceControl(param);
    if (param.commented) return createCommentedControl(param);

    switch (param.type) {
      case 'bool': return createBoolControl(param);
      case 'int':
        if (param.enum) return createEnumControl(param);
        return createNumberControl(param);
      case 'float': return createNumberControl(param);
      case 'string': return createStringControl(param);
      case 'int-array':
        if (param.codeMap === 'key' && param.presets) return createKeySelectorControl(param);
        return createIntArrayControl(param);
      default: return createStringControl(param);
    }
  }

  function createCommentedControl(param) {
    const input = document.createElement('input');
    input.type = 'text';
    input.id = 'param-' + param.key;
    input.className = 'param-input text';
    input.placeholder = param.type === 'toml'
      ? param.defaultValue
      : I18n.t('group.' + param.group) + ' (optional)';
    input.dataset.key = param.key;
    if (currentValues[param.key] !== undefined) {
      input.value = currentValues[param.key];
    }
    input.addEventListener('input', () => {
      currentValues[param.key] = input.value || undefined;
      fireChange(param.key);
    });
    return input;
  }

  function createHidDeviceControl(param) {
    const container = document.createElement('div');
    container.className = 'hid-device-picker';
    container.id = 'param-' + param.key;
    container.dataset.key = param.key;

    const list = document.createElement('div');
    list.className = 'hid-device-list';

    const addButton = document.createElement('button');
    addButton.type = 'button';
    addButton.className = 'key-add-btn device-add-btn';
    addButton.textContent = I18n.t('device.add');
    addButton.title = I18n.t('device.select-ignore');
    addButton.setAttribute('aria-label', I18n.t('device.select-ignore'));

    const status = document.createElement('span');
    status.className = 'hid-device-status';
    status.textContent = 'hid' in navigator ? '' : I18n.t('device.webhid-unsupported');

    const getValue = () => Array.isArray(currentValues[param.key]) ? currentValues[param.key] : [];

    function roleLabel(roles) {
      if (roles.includes('mouse') && roles.includes('keyboard')) return I18n.t('device.role-composite');
      if (roles.includes('mouse')) return I18n.t('device.role-mouse');
      if (roles.includes('keyboard')) return I18n.t('device.role-keyboard');
      return I18n.t('device.role-hid');
    }

    function renderList() {
      list.innerHTML = '';
      const devices = getValue();

      for (const device of devices) {
        const row = document.createElement('div');
        row.className = 'key-chip custom selected hid-device-row';

        const remove = document.createElement('button');
        remove.type = 'button';
        remove.className = 'key-chip-remove';
        remove.textContent = '×';
        remove.title = I18n.t('device.remove-ignore');
        remove.setAttribute('aria-label', I18n.t('device.remove-ignore'));
        remove.addEventListener('click', () => {
          const remaining = getValue().filter(item =>
            item.vendorId !== device.vendorId || item.productId !== device.productId);
          currentValues[param.key] = remaining.length > 0 ? remaining : undefined;
          renderList();
          fireChange(param.key);
        });

        const identity = document.createElement('span');
        identity.className = 'hid-device-identity';

        const name = document.createElement('span');
        name.className = 'hid-device-name';
        name.textContent = device.productName || I18n.t('device.unnamed');

        const details = document.createElement('span');
        details.className = 'hid-device-details';
        details.textContent = `${roleLabel(device.roles || [])} · ${formatHidId(device.vendorId)}:${formatHidId(device.productId)}`;

        identity.append(name, details);
        row.title = `${name.textContent} · ${details.textContent}`;
        row.append(identity, remove);
        list.appendChild(row);
      }
    }

    addButton.addEventListener('click', async () => {
      if (!('hid' in navigator)) {
        status.textContent = I18n.t('device.webhid-unsupported');
        return;
      }

      try {
        const selected = await navigator.hid.requestDevice({
          // An empty filter list includes every WebHID-visible interface. This
          // is important for gaming mice whose configurable interface uses a
          // vendor-defined collection rather than the protected mouse report.
          filters: [],
        });
        if (selected.length === 0) {
          status.textContent = '';
          return;
        }

        const devices = getValue().map(device => ({ ...device, roles: [...(device.roles || [])] }));
        for (const hidDevice of selected) {
          const roles = hidDeviceRoles(hidDevice);
          const existing = devices.find(device =>
            device.vendorId === hidDevice.vendorId && device.productId === hidDevice.productId);
          if (existing) {
            existing.roles = Array.from(new Set([...existing.roles, ...roles]));
            if (!existing.productName && hidDevice.productName) existing.productName = hidDevice.productName;
          } else {
            devices.push({
              vendorId: hidDevice.vendorId,
              productId: hidDevice.productId,
              productName: hidDevice.productName || '',
              roles,
            });
          }
        }
        devices.sort((lhs, rhs) =>
          (lhs.productName || '').localeCompare(rhs.productName || '') ||
          lhs.vendorId - rhs.vendorId || lhs.productId - rhs.productId);
        currentValues[param.key] = devices;
        status.textContent = '';
        renderList();
        fireChange(param.key);
      } catch (error) {
        status.textContent = error.name === 'NotFoundError'
          ? ''
          : I18n.t('device.selection-failed');
      }
    });

    if (!('hid' in navigator)) {
      addButton.disabled = true;
    }

    status.setAttribute('role', 'status');
    container.append(list, addButton, status);
    renderList();
    return container;
  }

  function hidDeviceRoles(device) {
    const roles = new Set();
    const visit = (collections) => {
      for (const collection of collections || []) {
        if (collection.usagePage === 0x01 && collection.usage === 0x02) roles.add('mouse');
        if (collection.usagePage === 0x01 && (collection.usage === 0x06 || collection.usage === 0x07)) {
          roles.add('keyboard');
        }
        visit(collection.children);
      }
    };
    visit(device.collections);
    return Array.from(roles);
  }

  function formatHidId(value) {
    return Number(value).toString(16).padStart(4, '0').toUpperCase();
  }

  function createBoolControl(param) {
    const toggle = document.createElement('label');
    toggle.className = 'toggle';

    const checkbox = document.createElement('input');
    checkbox.type = 'checkbox';
    checkbox.id = 'param-' + param.key;
    checkbox.checked = getEffectiveValue(param);
    checkbox.dataset.key = param.key;
    checkbox.addEventListener('change', () => {
      currentValues[param.key] = checkbox.checked;
      fireChange(param.key);
    });

    const slider = document.createElement('span');
    slider.className = 'toggle-slider';

    toggle.appendChild(checkbox);
    toggle.appendChild(slider);
    return toggle;
  }

  function createNumberControl(param) {
    const wrap = document.createElement('div');
    wrap.className = 'slider-wrap';

    const slider = document.createElement('input');
    slider.type = 'range';
    slider.id = 'param-' + param.key;
    slider.className = 'param-slider';
    const controlMin = param.uiMin ?? param.min ?? 0;
    const controlMax = ['min_deceleration', 'max_deceleration'].includes(param.key) && getControlValue(param) > param.max
      ? Math.max(param.max, getControlValue(param)) : param.uiMax ?? param.max ?? 100;
    slider.min = controlMin;
    slider.max = controlMax;
    const controlStep = ['min_deceleration', 'max_deceleration'].includes(param.key) && getControlValue(param) > param.max
      ? 1 : param.step ?? 1;
    slider.step = controlStep;
    slider.value = getControlValue(param);
    slider.dataset.key = param.key;

    const input = document.createElement('input');
    input.type = 'number';
    input.className = 'param-input number';
    input.min = controlMin;
    input.max = controlMax;
    input.step = controlStep;
    input.value = getControlValue(param);
    input.dataset.key = param.key;

    slider.addEventListener('input', () => {
      const val = Number(slider.value);
      input.value = val;
      currentValues[param.key] = param.uiAbs ? Math.abs(val) : val;
      fireChange(param.key);
    });

    // Commit text edits only after editing ends, so transient prefixes do not
    // change validation, profile state, or the simulation.
    input.addEventListener('blur', () => {
      const value = input.value.trim() === '' ? NaN : Number(input.value);
      const val = Number.isFinite(value) ? Math.max(controlMin, Math.min(controlMax, value)) : NaN;
      if (Number.isFinite(val)) {
        input.value = val;
        slider.value = val;
      }
      currentValues[param.key] = param.uiAbs ? Math.abs(val) : val;
      fireChange(param.key);
    });
    input.addEventListener('keydown', event => {
      if (event.key === 'Enter') { event.preventDefault(); input.blur(); }
    });

    wrap.appendChild(slider);
    wrap.appendChild(input);
    return wrap;
  }

  function createEnumControl(param) {
    const select = document.createElement('select');
    select.id = 'param-' + param.key;
    select.className = 'param-input select';
    select.dataset.key = param.key;

    const codeMap = param.codeMap ? ParamSchema.getCodeMap(param.codeMap) : null;

    for (const val of param.enum) {
      const opt = document.createElement('option');
      opt.value = val;
      const localizedEnumLabel = getEnumLabel(param, val);
      if (localizedEnumLabel) {
        opt.textContent = localizedEnumLabel;
      } else if (codeMap && codeMap[val]) {
        opt.textContent = `${codeMap[val]} (${val})`;
      } else {
        opt.textContent = val;
      }
      if (Number(val) === Number(getEffectiveValue(param))) opt.selected = true;
      select.appendChild(opt);
    }

    select.addEventListener('change', () => {
      currentValues[param.key] = Number(select.value);
      fireChange(param.key);
    });

    return select;
  }

  function getEnumLabel(param, value) {
    if (!param.enumLabels) return '';
    const labels = param.enumLabels[I18n.lang()] || param.enumLabels.en || {};
    return labels[value] || labels[String(value)] || '';
  }

  function createStringControl(param) {
    const input = document.createElement('input');
    input.type = 'text';
    input.id = 'param-' + param.key;
    input.className = 'param-input text';
    input.value = getEffectiveValue(param) || '';
    input.dataset.key = param.key;
    input.addEventListener('input', () => {
      currentValues[param.key] = input.value;
      fireChange(param.key);
    });
    return input;
  }

  function createIntArrayControl(param) {
    const codeMap = param.codeMap ? ParamSchema.getCodeMap(param.codeMap) : null;

    const input = document.createElement('input');
    input.type = 'text';
    input.id = 'param-' + param.key;
    input.className = 'param-input text';
    input.value = Array.isArray(getEffectiveValue(param))
      ? getEffectiveValue(param).map(v => codeMap && codeMap[v] ? codeMap[v] : v).join(', ')
      : '';
    input.placeholder = codeMap
      ? Object.values(codeMap).slice(0, 2).join(', ')
      : 'e.g. 42, 54';
    input.dataset.key = param.key;
    input.addEventListener('input', () => {
      const arr = input.value.split(',').map(s => {
        const resolved = ParamSchema.resolveCode(param.codeMap, s);
        return resolved;
      }).filter(n => n !== null);
      currentValues[param.key] = arr;
      fireChange(param.key);
    });
    return input;
  }

  function createKeySelectorControl(param) {
    const container = document.createElement('div');
    container.className = 'key-chips';
    container.id = 'param-' + param.key;
    container.dataset.key = param.key;

    const presets = param.presets || [];
    const labels = ParamSchema.KEY_LABELS || {};
    const getValue = () => currentValues[param.key] || [];

    function syncValue() {
      const selected = container.querySelectorAll('.key-chip.selected');
      const arr = Array.from(selected).map(el => Number(el.dataset.code));
      currentValues[param.key] = arr;
      fireChange(param.key);
    }

    // Render preset chips
    for (const code of presets) {
      const chip = document.createElement('span');
      chip.className = 'key-chip';
      chip.dataset.code = code;
      chip.textContent = labels[code] || code;
      if (getValue().includes(code)) chip.classList.add('selected');
      chip.addEventListener('click', () => {
        chip.classList.toggle('selected');
        syncValue();
      });
      container.appendChild(chip);
    }

    // Render custom key chips (values not in presets)
    function renderCustomChips() {
      container.querySelectorAll('.key-chip.custom').forEach(el => el.remove());
      const presetSet = new Set(presets);
      for (const code of getValue()) {
        if (presetSet.has(code)) continue;
        const chip = document.createElement('span');
        chip.className = 'key-chip custom selected';
        chip.dataset.code = code;
        chip.textContent = labels[code] || code;
        const removeBtn = document.createElement('button');
        removeBtn.className = 'key-chip-remove';
        removeBtn.textContent = '×';
        removeBtn.addEventListener('click', (e) => {
          e.stopPropagation();
          chip.remove();
          syncValue();
        });
        chip.appendChild(removeBtn);
        // Insert before the add button/input
        const addEl = container.querySelector('.key-add-btn, .key-add-input');
        if (addEl) {
          container.insertBefore(chip, addEl);
        } else {
          container.appendChild(chip);
        }
      }
    }

    // Add button / inline input
    const addBtn = document.createElement('button');
    addBtn.className = 'key-add-btn';
    addBtn.textContent = I18n.t('key.add');

    const addWrap = document.createElement('span');
    addWrap.className = 'key-add-input';
    addWrap.style.display = 'none';
    const addInput = document.createElement('input');
    addInput.type = 'number';
    addInput.min = 0;
    addInput.max = ParamSchema.KEY_CNT - 1;
    addInput.placeholder = I18n.t('key.code-placeholder');
    const addOk = document.createElement('button');
    addOk.className = 'key-add-ok';
    addOk.textContent = '✓';
    addWrap.appendChild(addInput);
    addWrap.appendChild(addOk);

    function showAddInput() {
      addBtn.style.display = 'none';
      addWrap.style.display = 'inline-flex';
      addInput.value = '';
      addInput.classList.remove('error');
      addInput.focus();
    }

    function hideAddInput() {
      addWrap.style.display = 'none';
      addBtn.style.display = 'inline-flex';
      addInput.classList.remove('error');
    }

    function confirmAdd() {
      const code = parseInt(addInput.value, 10);
      if (isNaN(code) || code < 0 || code >= ParamSchema.KEY_CNT || ParamSchema.isPointerButton(code)) {
        addInput.classList.add('error');
        setTimeout(() => addInput.classList.remove('error'), 400);
        return;
      }
      const current = getValue();
      if (current.includes(code)) {
        addInput.classList.add('error');
        setTimeout(() => addInput.classList.remove('error'), 400);
        return;
      }
      // If it's a preset key, toggle it on
      if (presets.includes(code)) {
        const chip = container.querySelector(`.key-chip[data-code="${code}"]`);
        if (chip && !chip.classList.contains('selected')) {
          chip.classList.add('selected');
        }
        syncValue();
        hideAddInput();
        return;
      }
      // Add custom chip
      current.push(code);
      currentValues[param.key] = current;
      renderCustomChips();
      fireChange(param.key);
      hideAddInput();
    }

    addBtn.addEventListener('click', showAddInput);
    addOk.addEventListener('click', confirmAdd);
    addInput.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') { e.preventDefault(); confirmAdd(); }
      if (e.key === 'Escape') hideAddInput();
    });
    addInput.addEventListener('blur', () => {
      setTimeout(() => {
        if (addWrap.style.display !== 'none' && !addWrap.contains(document.activeElement)) {
          hideAddInput();
        }
      }, 150);
    });

    container.appendChild(addBtn);
    container.appendChild(addWrap);

    // Initial render of custom chips
    renderCustomChips();

    return container;
  }

  function currentModeName() {
    const mode = Number(currentValues.smooth_mode ?? 2);
    if (mode === 1) return 'distance';
    if (mode === 2) return 'hybrid';
    return 'speed';
  }

  function isModeActive(param) {
    if (!param || !param.modes || param.modes.length === 0) return true;
    return param.modes.includes(currentModeName());
  }

  function isParamActive(param) {
    if (!isModeActive(param) || (instant && InstantScroll.disabledKeys.has(param.key))) return false;
    return dependenciesMet(param);
  }

  function dependenciesMet(param) {
    if (param['disabled-when'] && currentValues[param['disabled-when']]) return false;
    if (param.group === 'drag-view' && param.key !== 'drag_view_button' && !currentValues.drag_view_button) return false;
    if (param.group === 'auto-scroll' && param.key !== 'auto_scroll_button' && !currentValues.auto_scroll_button) return false;
    return !param['depends-on'] || !!currentValues[param['depends-on']];
  }

  function updateParamState(row, param) {
    const modeDisabled = !isModeActive(param);
    const dependencyDisabled = !dependenciesMet(param);
    const disabled = modeDisabled || dependencyDisabled || (instant && InstantScroll.disabledKeys.has(param.key));
    row.classList.toggle('disabled', disabled);
    const inputs = row.querySelectorAll('input, select');
    inputs.forEach(el => { el.disabled = disabled; });
  }

  function updateAllParamStates() {
    const passthrough = Boolean(currentValues.wheel_passthrough);
    const effect = document.getElementById('scroll-effect');
    if (effect) effect.disabled = passthrough;
    const rows = document.querySelectorAll('.param-row');
    rows.forEach(row => {
      const key = row.dataset.key;
      const param = params.find(p => p.key === key);
      if (param) updateParamState(row, param);
    });
  }

  function fireChange(key) {
    if (instant) {
      InstantScroll.apply(currentValues);
      // Recompute without rebuilding controls while a distance/tick slider is dragged.
      for (const managedKey of InstantScroll.managedKeys) {
        document.querySelectorAll(`input[data-key="${managedKey}"], select[data-key="${managedKey}"]`).forEach(control => {
          if (['min_deceleration', 'max_deceleration'].includes(managedKey)) {
            control.max = currentValues[managedKey];
          }
          control.value = currentValues[managedKey];
        });
      }
    }
    const effect = document.getElementById('scroll-effect');
    if (effect) effect.value = instant ? 'instant' : 'smooth';
    updateAllParamStates();
    validateConstraints();
    if (onParamChange) onParamChange(getValues(), key, getEditorState());
  }

  function validateConstraints() {
    // Clear existing warnings
    document.querySelectorAll('.validation-warning').forEach(el => el.remove());
    document.querySelectorAll('.param-row.has-warning').forEach(el => el.classList.remove('has-warning'));

    const id = typeof Profiles !== 'undefined' && Profiles.ready() ? Profiles.selected() : 0;
    for (const issue of ConfigWriter.validateProfile(getValues(), id)) {
      for (const key of issue.keys) {
        const row = document.querySelector(`.param-row[data-key="${key}"]`);
        if (!row) continue;
        row.classList.add('has-warning');
        const warning = document.createElement('div');
        warning.className = 'validation-warning';
        warning.dataset.constraint = issue.keys.join('-');
        warning.textContent = I18n.t('validation.' + issue.kind);
        row.after(warning);
      }
    }
  }

  function getValues() {
    const values = { ...currentValues };
    if (values.drag_view_speed !== undefined) {
      const speed = Math.abs(Number(values.drag_view_speed));
      values.drag_view_speed = Number(values.drag_view_mode ?? 0) === 1 ? -speed : speed;
    }
    return values;
  }

  function resetToDefaults() {
    instant = false;
    savedSmoothValues = null;
    for (const param of params) {
      currentValues[param.key] = param.commented ? undefined : (Array.isArray(param.defaultValue) ? [...param.defaultValue] : param.defaultValue);
    }
    render();
    updateAllParamStates();
    validateConstraints();
    if (onParamChange) onParamChange(getValues(), null, getEditorState());
  }

  function getEditorState() {
    return { instant, savedSmoothValues: structuredClone(savedSmoothValues) };
  }

  function loadValues(values, editorState = null) {
    currentValues = structuredClone(values);
    instant = editorState?.instant ?? InstantScroll.matches(currentValues);
    savedSmoothValues = structuredClone(editorState?.savedSmoothValues ?? null);
    if (instant) InstantScroll.apply(currentValues);
    render();
    fireChange(null);
  }

  return { init, render, getValues, getEditorState, resetToDefaults, loadValues };
})();
