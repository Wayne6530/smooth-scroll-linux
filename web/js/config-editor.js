/**
 * Renders the parameter editor UI from a static schema.
 * Supports key-name display for button/key codes and constraint validation.
 */
// eslint-disable-next-line no-unused-vars
const ConfigEditor = (() => {
  let currentValues = {};
  let onParamChange = null;
  let params = [];

  const GROUP_ORDER = ['device', 'scroll', 'braking', 'drag-view', 'advanced'];

  function getEffectiveValue(param) {
    const current = currentValues[param.key];
    return current !== undefined ? current : param.value;
  }

  const CONSTRAINTS = [
    {
      keys: ['min_deceleration', 'max_deceleration'],
      check: (v) => (v.min_deceleration ?? 0) <= (v.max_deceleration ?? Infinity),
      messageKey: 'validation.min-max-decel',
    },
    {
      keys: ['min_speed_change_ratio', 'max_speed_change_ratio'],
      check: (v) => (v.min_speed_change_ratio ?? 0) <= (v.max_speed_change_ratio ?? Infinity),
      messageKey: 'validation.min-max-ratio',
    },
  ];

  function init(schema, onChange) {
    params = schema.params;
    onParamChange = onChange;
    currentValues = {};
    for (const param of params) {
      currentValues[param.key] = param.commented ? undefined : param.value;
    }
    render();
  }

  function render() {
    const container = document.getElementById('editor-params');
    container.innerHTML = '';
    document.querySelectorAll('.param-tooltip').forEach(el => el.remove());

    // Config version badge
    const versionEl = document.createElement('div');
    versionEl.className = 'config-version';
    versionEl.id = 'config-version';
    versionEl.innerHTML = `${I18n.t('version.config')} <span class="version-badge">v${ParamSchema.CONFIG_VERSION}</span>`;
    container.appendChild(versionEl);

    const groups = ParamSchema.getGroups(params);
    const orderedGroups = GROUP_ORDER.filter(g => groups.has(g));

    for (const groupName of orderedGroups) {
      const groupParams = groups.get(groupName);
      const section = createGroupSection(groupName, groupParams);
      container.appendChild(section);
    }
  }

  function createGroupSection(groupName, groupParams) {
    const section = document.createElement('div');
    section.className = 'param-group';

    const header = document.createElement('div');
    header.className = 'group-header';
    header.textContent = I18n.t('group.' + groupName);
    header.addEventListener('click', () => {
      const content = section.querySelector('.group-content');
      content.classList.toggle('collapsed');
      header.classList.toggle('collapsed');
    });

    const content = document.createElement('div');
    content.className = 'group-content';
    if (groupName !== 'scroll') content.classList.add('collapsed');

    for (const param of groupParams) {
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
    const descText = I18n.desc(param);
    if (descText) {
      const helpBtn = document.createElement('span');
      helpBtn.className = 'param-help';
      helpBtn.textContent = '?';
      const tooltip = document.createElement('div');
      tooltip.className = 'param-tooltip';
      tooltip.textContent = descText;
      document.body.appendChild(tooltip);
      helpBtn.addEventListener('mouseenter', () => {
        const rect = helpBtn.getBoundingClientRect();
        tooltip.style.top = (rect.bottom + 6) + 'px';
        tooltip.style.left = Math.max(8, Math.min(rect.left, window.innerWidth - 256)) + 'px';
        tooltip.classList.add('visible');
      });
      helpBtn.addEventListener('mouseleave', () => tooltip.classList.remove('visible'));
      controlWrap.appendChild(helpBtn);
    }

    row.appendChild(labelEl);
    row.appendChild(controlWrap);

    updateParamState(row, param);
    return row;
  }

  function createControl(param) {
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
    input.placeholder = I18n.t('group.' + param.group) + ' (optional)';
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
    slider.min = param.min ?? 0;
    slider.max = param.max ?? 100;
    slider.step = param.step ?? 1;
    slider.value = getEffectiveValue(param);
    slider.dataset.key = param.key;

    const input = document.createElement('input');
    input.type = 'number';
    input.className = 'param-input number';
    input.min = param.min ?? 0;
    input.max = param.max ?? 10000;
    input.step = param.step ?? 1;
    input.value = getEffectiveValue(param);
    input.dataset.key = param.key;

    slider.addEventListener('input', () => {
      const val = Number(slider.value);
      input.value = val;
      currentValues[param.key] = val;
      fireChange(param.key);
    });

    input.addEventListener('input', () => {
      let val = Number(input.value);
      if (isNaN(val)) return;
      if (param.min !== undefined) val = Math.max(param.min, val);
      if (param.max !== undefined) val = Math.min(param.max, val);
      slider.value = val;
      currentValues[param.key] = val;
      fireChange(param.key);
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
      if (isNaN(code) || code < 0 || code >= ParamSchema.KEY_CNT) {
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
    return Number(currentValues.smooth_mode ?? 0) === 1 ? 'distance' : 'speed';
  }

  function isModeActive(param) {
    if (!param || !param.modes || param.modes.length === 0) return true;
    return param.modes.includes(currentModeName());
  }

  function isParamActive(param) {
    if (!isModeActive(param)) return false;
    const dependsOn = param['depends-on'];
    if (!dependsOn) return true;
    return !!currentValues[dependsOn];
  }

  function updateParamState(row, param) {
    const dependsOn = param['depends-on'];
    const modeDisabled = !isModeActive(param);
    const dependencyDisabled = dependsOn ? !currentValues[dependsOn] : false;
    const disabled = modeDisabled || dependencyDisabled;
    row.classList.toggle('disabled', disabled);
    const inputs = row.querySelectorAll('input, select');
    inputs.forEach(el => { el.disabled = disabled; });
  }

  function updateAllParamStates() {
    const rows = document.querySelectorAll('.param-row');
    rows.forEach(row => {
      const key = row.dataset.key;
      const param = params.find(p => p.key === key);
      if (param) updateParamState(row, param);
    });
  }

  function fireChange(key) {
    updateAllParamStates();
    validateConstraints();
    if (onParamChange) onParamChange(currentValues, key);
  }

  function validateConstraints() {
    // Clear existing warnings
    document.querySelectorAll('.validation-warning').forEach(el => el.remove());
    document.querySelectorAll('.param-row.has-warning').forEach(el => el.classList.remove('has-warning'));

    for (const constraint of CONSTRAINTS) {
      const constraintParams = constraint.keys.map(key => params.find(p => p.key === key));
      if (!constraintParams.every(param => param && isParamActive(param))) {
        continue;
      }

      if (!constraint.check(currentValues)) {
        for (const key of constraint.keys) {
          const row = document.querySelector(`.param-row[data-key="${key}"]`);
          if (row) {
            row.classList.add('has-warning');
            const warning = document.createElement('div');
            warning.className = 'validation-warning';
            warning.dataset.constraint = constraint.keys.join('-');
            warning.textContent = I18n.t(constraint.messageKey);
            row.after(warning);
          }
        }
      }
    }
  }

  function getValues() {
    return { ...currentValues };
  }

  function resetToDefaults() {
    for (const param of params) {
      currentValues[param.key] = param.commented ? undefined : (Array.isArray(param.defaultValue) ? [...param.defaultValue] : param.defaultValue);
    }
    render();
    updateAllParamStates();
    validateConstraints();
    if (onParamChange) onParamChange(currentValues, null);
  }

  return { init, render, getValues, resetToDefaults };
})();
