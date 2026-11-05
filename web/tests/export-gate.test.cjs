const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const elements = new Map();
const context = vm.createContext({
  structuredClone,
  document: {
    addEventListener() {},
    getElementById(id) {
      if (!elements.has(id)) elements.set(id, {});
      return elements.get(id);
    },
  },
  localStorage: { getItem: () => 'en', setItem() {} },
});
for (const file of ['param-schema', 'profile-presets', 'config-presets', 'profiles', 'config-writer', 'i18n', 'app']) {
  vm.runInContext(fs.readFileSync(path.join(__dirname, '../js', file + '.js'), 'utf8'), context);
}
vm.runInContext(`
  ConfigWriter.init(ParamSchema);
  Profiles.init(ConfigPresets.values('inertia'));
  Profiles.update({ ...Profiles.values(), wheel_passthrough: true, min_deceleration: 7000, max_deceleration: 6000 });
`, context);
const validate = () => vm.runInContext('updateTomlPreview(Profiles.values())', context);
assert.equal(validate(), false);
assert.equal(elements.get('btn-preview').disabled, true);
assert.equal(elements.get('btn-download').disabled, true);
assert.equal(elements.get('export-error').hidden, false);
assert.match(elements.get('export-error-details').textContent, /Profile 0.*Minimum must not exceed maximum/);
// Errors in a different profile must still block the whole-config preview.
vm.runInContext('Profiles.select(1)', context);
assert.equal(validate(), false);
assert.equal(elements.get('btn-preview').disabled, true);
vm.runInContext('Profiles.select(0); Profiles.update({ ...Profiles.values(), min_deceleration: 1420 });', context);
assert.equal(validate(), true);
assert.equal(elements.get('btn-preview').disabled, false);
assert.equal(elements.get('btn-download').disabled, false);
assert.equal(elements.get('export-error').hidden, true);
console.log('Export errors block preview immediately, across profiles, and clear after correction.');

// Whole-config export errors must not gate an unrelated active profile's graph.
const graphUpdates = [];
context.Visualization = { update: (values, valid) => graphUpdates.push({values, valid}) };
context.PresetUI = { updateStatus() {} };
const appSource = fs.readFileSync(path.join(__dirname, '../js/app.js'), 'utf8');
const start = appSource.indexOf('  const onParamChange =');
const end = appSource.indexOf('  ConfigEditor.init', start);
vm.runInContext(appSource.slice(start, end).replace('const onParamChange', 'globalThis.change'), context);
vm.runInContext('Profiles.select(0); change({ ...Profiles.values(), min_deceleration: 7000, max_deceleration: 6000 });', context);
assert.equal(graphUpdates.at(-1).valid, false);
vm.runInContext('Profiles.select(1); change(Profiles.values());', context);
assert.equal(elements.get('btn-preview').disabled, true);
assert.equal(graphUpdates.at(-1).valid, true);
assert.equal(graphUpdates.at(-1).values.wheel_passthrough, true);
console.log('Active-profile simulation remains independent of whole-config export errors.');
