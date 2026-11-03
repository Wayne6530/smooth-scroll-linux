// Run with: node web/tests/auto-scroll-config.test.cjs
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const context = vm.createContext({});
for (const name of ['param-schema', 'instant-scroll', 'config-writer']) {
  vm.runInContext(fs.readFileSync(path.join(__dirname, '../js', name + '.js'), 'utf8'), context);
}
vm.runInContext('globalThis.api = { ParamSchema, ConfigWriter };', context);
const { ParamSchema, ConfigWriter } = context.api;
ConfigWriter.init(ParamSchema);
const values = ParamSchema.getDefaultValues();
assert.equal(values.auto_scroll_latch_on_release, true);
assert.equal(values.auto_scroll_speed_exponent, 1);
const exponent = ParamSchema.params.find(p => p.key === 'auto_scroll_speed_exponent');
assert.equal(exponent.min, 1);
assert.equal(exponent.max, 3);
assert.equal(exponent.step, 0.1);

values.auto_scroll_latch_on_release = false;
values.auto_scroll_speed_exponent = 2.2;
values.auto_scroll_wheel_action = 1;
values.auto_scroll_exit_button_mode = 0;
const exported = ConfigWriter.generate(values);
assert.match(exported, /^auto_scroll_latch_on_release = false$/m);
assert.match(exported, /^auto_scroll_speed_exponent = 2\.2$/m);
// Inapplicable controls retain their values in exported configurations.
assert.match(exported, /^auto_scroll_wheel_action = 1$/m);
assert.match(exported, /^auto_scroll_exit_button_mode = 0$/m);
for (const key of ['auto_scroll_wheel_action', 'auto_scroll_exit_button_mode']) {
  assert.equal(ParamSchema.params.find(p => p.key === key)['depends-on'], 'auto_scroll_latch_on_release');
}
const shipped = fs.readFileSync(path.join(__dirname, '../../config/smooth-scroll.toml'), 'utf8');
for (const key of ['auto_scroll_latch_on_release', 'auto_scroll_speed_exponent']) {
  const param = ParamSchema.params.find(p => p.key === key);
  const block = shipped.split('\n\n').find(block => block.includes('\n' + key + ' ='));
  for (const field of ['label-en', 'label-zh', 'desc-en', 'desc-zh', 'type', 'min', 'max', 'step', 'group']) {
    if (param[field] !== undefined) assert.ok(block.includes(`# @${field}: ${param[field]}`));
  }
}
console.log('Auto Scroll defaults, metadata, dependencies and TOML export passed.');

// Exercise the editor's dependency predicate with both the feature and latch gates.
const editor = fs.readFileSync(path.join(__dirname, '../js/config-editor.js'), 'utf8');
const dependencyFunction = editor.match(/  function dependenciesMet\(param\) \{[\s\S]*?\n  \}/)[0];
const dependencyContext = vm.createContext({ currentValues: values });
vm.runInContext(dependencyFunction, dependencyContext);
for (const button of [0, 275]) {
  for (const latch of [false, true]) {
    values.auto_scroll_button = button;
    values.auto_scroll_latch_on_release = latch;
    for (const key of ['auto_scroll_wheel_action', 'auto_scroll_exit_button_mode']) {
      const param = ParamSchema.params.find(p => p.key === key);
      assert.equal(dependencyContext.dependenciesMet(param), Boolean(button && latch));
    }
    assert.equal(dependencyContext.dependenciesMet(exponent), Boolean(button));
  }
}
console.log('Editor combines feature-button and latch dependencies correctly.');
