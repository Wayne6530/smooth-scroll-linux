const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const element = () => ({
  children: [], handlers: {}, append(...nodes) { this.children.push(...nodes); },
  appendChild(node) { this.children.push(node); },
  addEventListener(name, fn) { this.handlers[name] = fn; },
  focus() {},
});
const context = vm.createContext({
  structuredClone,
  document: { createElement: element, getElementById: element, querySelectorAll: () => [] },
  I18n: {t: key => key},
  createHelpTooltip: element,
  render() {}, validateConstraints() {}, updateAllParamStates() {},
});
for (const file of ['param-schema', 'instant-scroll', 'profile-presets', 'config-presets', 'profiles']) {
  vm.runInContext(fs.readFileSync(path.join(__dirname, '../js', file + '.js'), 'utf8'), context);
}
const source = fs.readFileSync(path.join(__dirname, '../js/config-editor.js'), 'utf8');
for (const name of ['createScrollEffectControl', 'getValues', 'getEditorState', 'loadValues', 'fireChange']) {
  vm.runInContext(source.match(new RegExp('  function ' + name + '\\([^]*?\\n  \\}'))[0], context);
}
vm.runInContext(`
  let currentValues = {}, instant = false, savedSmoothValues = null;
  const onParamChange = (values, key, state) => Profiles.update(values, state);
  Profiles.init(ConfigPresets.values('inertia'));
  const load = () => loadValues(Profiles.values(), Profiles.editorState());
  const effect = value => {
    const wrap = createScrollEffectControl();
    const select = wrap.children[0].children[1].children[0];
    select.value = value;
    select.handlers.change();
  };
  load();
  currentValues.damping = 7;
  fireChange('damping');
  effect('instant');
  load(); // Device navigation reloads the same profile.
  Profiles.add(); load();
  currentValues.damping = 9;
  fireChange('damping');
  effect('instant');
  Profiles.select(0); load(); effect('smooth');
`, context);
assert.equal(vm.runInContext('currentValues.damping', context), 7);
vm.runInContext("Profiles.select(2); load(); effect('smooth');", context);
assert.equal(vm.runInContext('currentValues.damping', context), 9);
vm.runInContext("effect('instant'); Profiles.applyPreset('responsive'); load();", context);
assert.equal(vm.runInContext('savedSmoothValues', context), null);
assert.equal(vm.runInContext('currentValues.damping', context), 10);
vm.runInContext("effect('instant'); Profiles.restore(); load();", context);
assert.equal(vm.runInContext('savedSmoothValues', context), null);
assert.equal(vm.runInContext('currentValues.damping', context), 10);
assert.ok(!vm.runInContext('JSON.stringify(Profiles.config())', context).includes('savedSmoothValues'));
console.log('Instant smooth snapshots survive Device/profile navigation and reset with presets without leaking into config.');
