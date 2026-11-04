const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const context = vm.createContext({ structuredClone });
for (const name of ['param-schema', 'instant-scroll', 'config-presets', 'config-writer']) {
  vm.runInContext(fs.readFileSync(path.join(__dirname, '../js', name + '.js'), 'utf8'), context);
}
const { ConfigPresets, ConfigWriter, ParamSchema, InstantScroll } = vm.runInContext(
  '({ ConfigPresets, ConfigWriter, ParamSchema, InstantScroll })', context);
ConfigWriter.init(ParamSchema);
assert.equal(JSON.stringify(ConfigPresets.values('inertia')), JSON.stringify(ParamSchema.getDefaultValues()));
assert.equal(JSON.stringify(ConfigPresets.values('unknown')), JSON.stringify(ConfigPresets.values('inertia')));
const responsive = ConfigPresets.values('responsive');
assert.equal(responsive.smooth_mode, 1);
assert.equal(responsive.damping, 10);
assert.equal(responsive.min_deceleration, 4200);
assert.equal(responsive.max_deceleration, 30000);
assert.equal(responsive.free_spin_button, 273);
assert.equal(responsive.drag_view_button, 0);
assert.equal(responsive.auto_scroll_button, 0);
assert.equal(responsive.use_reverse_scroll_braking, false);
assert.equal(responsive.use_mouse_movement_braking, false);
assert.equal(InstantScroll.matches(responsive), false);
const auto = ConfigPresets.values('auto-scroll');
assert.equal(InstantScroll.matches(auto), true);
assert.equal(auto.auto_scroll_button, 274);
assert.equal(auto.auto_scroll_activation_mode, 1);
assert.equal(auto.auto_scroll_speed_exponent, 2);
assert.equal(auto.drag_view_activation_mode, 1);
assert.equal(auto.min_deceleration, 15150001);
assert.equal(auto.max_deceleration, 15150001);
assert.notEqual(auto.auto_scroll_button, auto.drag_view_button);
assert.notEqual(auto.auto_scroll_button, auto.free_spin_button);
// Applying the wheel shortcut must not overwrite independent feature choices.
auto.free_spin_button = 273;
auto.auto_scroll_activation_mode = 0;
InstantScroll.apply(auto);
assert.equal(auto.free_spin_button, 273);
assert.equal(auto.auto_scroll_activation_mode, 0);
auto.keyboard_braking_keys.push(1);
assert.equal(ConfigPresets.values('auto-scroll').keyboard_braking_keys.includes(1), false);
for (const preset of ConfigPresets.items) {
  const toml = ConfigWriter.generate(ConfigPresets.values(preset.id));
  assert.ok(!toml.includes('undefined'));
  assert.match(toml, /^smooth_mode = [012]$/m);
}
console.log('Preset defaults, independent edits, Auto Scroll bindings and export passed.');
