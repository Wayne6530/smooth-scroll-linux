/**
 * Generates TOML output from current parameter values and schema metadata.
 */
// eslint-disable-next-line no-unused-vars
const ConfigWriter = (() => {
  let schema = null;

  function init(schemaRef) {
    schema = schemaRef;
  }

  function generate(values) {
    if (!schema) return '';

    const lines = [];
    lines.push('# Smooth Scroll for Linux - Configuration File');
    lines.push('# https://github.com/Wayne6530/smooth-scroll-linux');
    lines.push('');
    lines.push(`# @config_version: ${schema.CONFIG_VERSION}`);
    lines.push('');

    // Reference comments
    for (const ref of schema.references) {
      lines.push(ref);
      lines.push('');
    }

    const GROUP_ORDER = ['device', 'scroll', 'braking', 'drag-view', 'auto-scroll', 'advanced'];
    const groups = ParamSchema.getGroups(schema.params);
    const orderedGroups = GROUP_ORDER.filter(g => groups.has(g));

    for (const groupName of orderedGroups) {
      lines.push(`# --- ${groupName.charAt(0).toUpperCase() + groupName.slice(1)} ---`);
      lines.push('');

      const groupParams = groups.get(groupName);
      for (const param of groupParams) {
        if (param.uiOnly) continue;

        // Write metadata comments
        const metaKeys = ['label-en', 'label-zh', 'desc-en', 'desc-zh', 'type', 'min', 'max', 'step', 'group', 'unit', 'enum', 'enum-labels-en', 'enum-labels-zh', 'depends-on', 'disabled-when'];
        for (const mk of metaKeys) {
          if (param[mk] !== undefined && param[mk] !== null && param[mk] !== '') {
            let val = Array.isArray(param[mk]) ? param[mk].join('|') : param[mk];
            // These preset-generated values exceed the ordinary tuning slider range.
            if (mk === 'max' && ['min_deceleration', 'max_deceleration'].includes(param.key) &&
                Number(values[param.key]) > Number(val)) {
              val = Math.max(Number(val), values[param.key]);
            }
            lines.push(`# @${mk}: ${val}`);
          }
        }

        // Write value line
        const value = values[param.key];
        if (param.commented) {
          if (value !== undefined && value !== '') {
            lines.push(`${param.key} = ${formatValue(value, param.type)}`);
          } else {
            lines.push(`# ${param.key} = ${formatValue(param.defaultValue, param.type)}`);
          }
        } else {
          lines.push(`${param.key} = ${formatValue(value !== undefined ? value : param.defaultValue, param.type)}`);
        }
        lines.push('');
      }
    }

    return lines.join('\n');
  }

  // Export relevance differs from UI editability: computed Instant scrolling
  // values remain necessary, and explicit button/key bindings prevent defaults
  // from unexpectedly enabling a feature or colliding with another button.
  function needed(param, values) {
    if (param.uiOnly || param.group === 'device') return false;
    if (['free_spin_button', 'drag_view_button', 'auto_scroll_button',
         'keyboard_braking_keys', 'keyboard_passthrough_keys'].includes(param.key)) return true;
    if (param['disabled-when'] && values[param['disabled-when']]) return false;
    if (param.group === 'drag-view' && !values.drag_view_button) return false;
    if (param.group === 'auto-scroll' && !values.auto_scroll_button) return false;
    if (param['depends-on'] && !values[param['depends-on']]) return false;
    if (param.key === 'tick_interval_microseconds' && values.wheel_passthrough && !values.auto_scroll_button) return false;
    const mode = ['speed', 'distance', 'hybrid'][values.smooth_mode];
    return !param.modes || param.modes.includes(mode);
  }

  function complete(values) {
    return Object.fromEntries(schema.params.map(param => [param.key, values[param.key] ?? param.defaultValue]));
  }

  function exportValues(values, id) {
    const all = complete(values);
    return Object.fromEntries(schema.params.filter(param => !param.uiOnly &&
      (id === 0 || needed(param, all))).map(param => [param.key, all[param.key]]));
  }

  // Shared by field hints, the preview gate, and TOML generation.
  function validateProfile(input, id = 0) {
    const issues = [];
    const values = exportValues(input, id);
    const add = (keys, kind) => issues.push({ profileId: id, keys, kind });
    for (const param of schema.params) {
      if (!(param.key in values)) continue;
      const value = values[param.key];
      if (param.type === 'int' || param.type === 'float') {
        const checked = param.uiAbs ? Math.abs(value) : value;
        if (!Number.isFinite(value) ||
            (param.type === 'int' && (!Number.isInteger(value) || value < -2147483648 || value > 2147483647)) ||
            (param.min !== undefined && checked < param.min) ||
            (param.enum && !param.enum.includes(value))) add([param.key], 'value');
      } else if (param.type === 'bool' && typeof value !== 'boolean') {
        add([param.key], 'value');
      } else if (param.type === 'int-array' && (!Array.isArray(value) || value.some(code =>
        !Number.isInteger(code) || code < 0 || code >= ParamSchema.KEY_CNT || ParamSchema.isPointerButton(code)))) {
        add([param.key], 'value');
      } else if (param.type === 'hid-device-list' && (!Array.isArray(value) || value.some(device =>
        !device || !['vendorId', 'productId'].every(key =>
          Number.isInteger(device[key]) && device[key] >= 0 && device[key] <= 65535)))) {
        add([param.key], 'device');
      }
    }
    for (const [min, max] of [['min_deceleration', 'max_deceleration'], ['min_speed_change_ratio', 'max_speed_change_ratio']]) {
      if (min in values && max in values && values[min] > values[max]) add([min, max], 'order');
    }
    const buttons = ['free_spin_button', 'drag_view_button', 'auto_scroll_button'];
    const conflicting = buttons.filter(key => values[key] &&
      buttons.some(other => other !== key && values[key] === values[other]));
    if (conflicting.length) add(conflicting, 'buttons');

    // The daemon computes this bound even when Auto Scroll is inactive.
    const effective = complete(values);
    const offsetKeys = ['auto_scroll_deadzone', 'auto_scroll_max_speed', 'auto_scroll_speed_factor'];
    if (!issues.some(issue => issue.keys.some(key => offsetKeys.includes(key)))) {
      const offset = Math.ceil(effective.auto_scroll_max_speed / effective.auto_scroll_speed_factor);
      const maxOffset = 9223372036854775807n - 2147483647n;
      if (!Number.isFinite(offset) || BigInt(offset) + BigInt(effective.auto_scroll_deadzone) > maxOffset) {
        add(offsetKeys.filter(key => key in values), 'offset');
      }
    }
    // Squaring this distance uses the daemon's signed int representation.
    if (values.max_mouse_movement_distance > 46340) add(['max_mouse_movement_distance'], 'value');
    return issues;
  }

  function errors(profiles) {
    return profiles.flatMap((profile, id) => validateProfile(profile.values, id));
  }

  function flatten(config) {
    return config.profiles.map(profile => ({ ...profile, values: { ...profile.values, ...config.globals } }));
  }

  function validateConfig(config) { return errors(flatten(config)); }

  function generateConfig(config) { return generateProfiles(flatten(config)); }

  function generateProfiles(profiles) {
    if (!schema || !profiles.length) return '';
    if (errors(profiles).length) throw new Error('Invalid configuration; resolve export validation errors first.');
    const lines = [generate(complete(profiles[0].values))];
    profiles.slice(1).forEach((profile, index) => {
      lines.push('', `# Profile ${index + 1}`, '[[profiles]]', `name = ${JSON.stringify(profile.name)}`);
      const values = exportValues(profile.values, index + 1);
      for (const param of schema.params) {
        if (param.key in values) lines.push(`${param.key} = ${formatValue(values[param.key], param.type)}`);
      }
    });
    return lines.join('\n');
  }

  function formatValue(value, type) {
    if (type === 'hid-device-list') return formatDeviceModels(value);
    if (typeof value === 'boolean') return value ? 'true' : 'false';
    if (type === 'int-array' || Array.isArray(value)) {
      return '[' + (Array.isArray(value) ? value.join(', ') : value) + ']';
    }
    if (typeof value === 'string') {
      if (value.startsWith('/')) return `"${value}"`;
      return `"${value}"`;
    }
    if (typeof value === 'number') {
      // Preserve .0 for floats that the original had it
      return String(value);
    }
    return String(value);
  }

  function formatDeviceModels(value) {
    if (!Array.isArray(value) || value.length === 0) return '[]';
    const lines = value.map(device => {
      const vendor = formatHexId(device.vendorId);
      const product = formatHexId(device.productId);
      return `  { vendor_id = ${vendor}, product_id = ${product} },`;
    });
    return '[\n' + lines.join('\n') + '\n]';
  }

  function formatHexId(value) {
    return '0x' + Number(value).toString(16).padStart(4, '0');
  }

  function download(text, filename) {
    const blob = new Blob([text], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    a.click();
    URL.revokeObjectURL(url);
  }

  return { init, generate, generateProfiles, generateConfig, validateConfig, validateProfile, download };
})();
