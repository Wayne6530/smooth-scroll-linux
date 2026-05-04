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

    const GROUP_ORDER = ['device', 'scroll', 'braking', 'drag-view', 'advanced'];
    const groups = ParamSchema.getGroups(schema.params);
    const orderedGroups = GROUP_ORDER.filter(g => groups.has(g));

    for (const groupName of orderedGroups) {
      lines.push(`# --- ${groupName.charAt(0).toUpperCase() + groupName.slice(1)} ---`);
      lines.push('');

      const groupParams = groups.get(groupName);
      for (const param of groupParams) {
        // Write metadata comments
        const metaKeys = ['label-en', 'label-zh', 'desc-en', 'desc-zh', 'type', 'min', 'max', 'step', 'group', 'unit', 'enum', 'depends-on'];
        for (const mk of metaKeys) {
          if (param[mk] !== undefined && param[mk] !== null && param[mk] !== '') {
            const val = Array.isArray(param[mk]) ? param[mk].join('|') : param[mk];
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

  function formatValue(value, type) {
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

  async function copyToClipboard(text) {
    try {
      await navigator.clipboard.writeText(text);
      return true;
    } catch {
      // Fallback
      const ta = document.createElement('textarea');
      ta.value = text;
      ta.style.position = 'fixed';
      ta.style.opacity = '0';
      document.body.appendChild(ta);
      ta.select();
      document.execCommand('copy');
      document.body.removeChild(ta);
      return true;
    }
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

  return { init, generate, copyToClipboard, download };
})();
