const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const context = vm.createContext({structuredClone});
for (const file of ['param-schema', 'profile-presets', 'config-presets', 'instant-scroll', 'config-writer']) {
  vm.runInContext(fs.readFileSync(path.join(__dirname, '../js', file + '.js'), 'utf8'), context);
}
const {ParamSchema, ConfigPresets, ConfigWriter, InstantScroll} =
  vm.runInContext('({ParamSchema, ConfigPresets, ConfigWriter, InstantScroll})', context);
ConfigWriter.init(ParamSchema);
const defaults = () => ConfigPresets.values('inertia').profiles[0].values;
const validate = (values, id = 0) => ConfigWriter.validateProfile(values, id);
const buttons = ['free_spin_button', 'drag_view_button', 'auto_scroll_button'];
for (let a = 0; a < buttons.length; a++) {
  for (let b = a + 1; b < buttons.length; b++) {
    const values = {...defaults(), free_spin_button: 0, drag_view_button: 0, auto_scroll_button: 0};
    values[buttons[a]] = values[buttons[b]] = 274;
    assert.ok(validate(values).some(issue => issue.kind === 'buttons'));
    assert.ok(validate(values, 2).some(issue => issue.kind === 'buttons'));
  }
}
assert.equal(validate({...defaults(), free_spin_button: 0, drag_view_button: 0, auto_scroll_button: 0}).length, 0);
for (const ignored_devices of [42, [null], [{vendorId:-1, productId:1}], [{vendorId:1, productId:65536}], [{vendorId:1.5, productId:2}]]) {
  assert.ok(validate({...defaults(), ignored_devices}).some(issue => issue.kind === 'device'));
}
assert.equal(validate({...defaults(), ignored_devices:[{vendorId:0,productId:65535}]}).length, 0);
for (const value of [2147483648, NaN, Infinity, 1.5]) {
  assert.ok(validate({...defaults(), device_event_debounce_milliseconds:value}).some(issue => issue.keys.includes('device_event_debounce_milliseconds')));
}
assert.ok(validate({...defaults(), auto_scroll_max_speed:1e30}).some(issue => issue.kind === 'offset'));
assert.ok(validate({...defaults(), max_mouse_movement_distance:46341}).some(issue => issue.kind === 'value'));
// Omitted inactive settings are replaced by defaults; Global retains them.
const inactive = {...defaults(), auto_scroll_button:0, auto_scroll_max_speed:1e30};
assert.equal(validate(inactive, 2).length, 0);
const instant = defaults();
InstantScroll.apply(instant);
assert.ok(instant.min_deceleration > 10000);
assert.equal(validate(instant).length, 0);

// Exercise the number input handler with the real shared validator.
const source = fs.readFileSync(path.join(__dirname, '../js/config-editor.js'), 'utf8');
const values = defaults();
let issues;
const editor = vm.createContext({
  currentValues: values,
  getControlValue: param => values[param.key],
  fireChange: () => { issues = validate(values); },
  document: {createElement: () => ({
    dataset:{}, children:[], handlers:{},
    addEventListener(name, handler) { this.handlers[name] = handler; },
    appendChild(child) { this.children.push(child); },
  })},
});
vm.runInContext(source.match(/  function createNumberControl\([^]*?\n  \}/)[0], editor);
const row = editor.createNumberControl(ParamSchema.params.find(p => p.key === 'min_deceleration'));
const [slider, input] = row.children;
for (const text of ['', '1', '14', '142', '1420', '20000']) {
  input.value = text;
  input.handlers.input?.();
  assert.equal(input.value, text);
  assert.equal(values.min_deceleration, 1420);
  assert.equal(issues, undefined, 'Typing must not trigger validation or simulation updates');
}
input.handlers.blur();
assert.equal(values.min_deceleration, 10000);
assert.equal(Number(input.value), values.min_deceleration);
assert.equal(Number(slider.value), values.min_deceleration);
assert.ok(issues.some(issue => issue.kind === 'order'));
input.value = '';
input.handlers.blur();
assert.ok(Number.isNaN(values.min_deceleration));
assert.ok(issues.some(issue => issue.keys.includes('min_deceleration')));
input.value = '1420';
input.handlers.blur();
assert.equal(issues.length, 0);

const dampingRow = editor.createNumberControl(ParamSchema.params.find(p => p.key === 'damping'));
const decimal = dampingRow.children[1];
for (const text of ['0', '0.', '0.0', '0.05']) {
  decimal.value = text;
  decimal.handlers.input?.();
  assert.equal(decimal.value, text);
  assert.equal(values.damping, 3.1);
}
decimal.handlers.blur();
assert.equal(values.damping, 0.05);
slider.value = '1500';
slider.handlers.input();
assert.equal(values.min_deceleration, 1500, 'Sliders still commit immediately');
assert.equal(Number(input.value), 1500);
console.log('Shared validation and numeric commits run after editing and clamping; sliders remain immediate.');
