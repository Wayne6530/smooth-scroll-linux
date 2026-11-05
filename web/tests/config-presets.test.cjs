const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const context = vm.createContext({ structuredClone });
for (const name of ['param-schema', 'instant-scroll', 'profile-presets', 'config-presets', 'profiles', 'config-writer']) {
  vm.runInContext(fs.readFileSync(path.join(__dirname, '../js', name + '.js'), 'utf8'), context);
}
const { ConfigPresets, ProfilePresets, Profiles, ConfigWriter, ParamSchema, InstantScroll } = vm.runInContext(
  '({ ConfigPresets, ProfilePresets, Profiles, ConfigWriter, ParamSchema, InstantScroll })', context);
ConfigWriter.init(ParamSchema);
assert.equal(JSON.stringify(ProfilePresets.defaults()), JSON.stringify(ProfilePresets.values('inertia')));
assert.equal(JSON.stringify(ProfilePresets.values('inertia')), JSON.stringify(ProfilePresets.pick(ParamSchema.getDefaultValues())));
assert.equal(JSON.stringify(ConfigPresets.values('unknown')), JSON.stringify(ConfigPresets.values('inertia')));
const responsive = ProfilePresets.values('responsive');
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
const auto = ProfilePresets.values('auto-scroll');
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
assert.equal(ProfilePresets.values('auto-scroll').keyboard_braking_keys.includes(1), false);
for (const preset of ConfigPresets.items) {
  const toml = ConfigWriter.generateConfig(ConfigPresets.values(preset.id));
  assert.ok(!toml.includes('undefined'));
  assert.match(toml, /^smooth_mode = [012]$/m);
}
console.log('Preset defaults, independent edits, Auto Scroll bindings and export passed.');

// Full configurations contain device options once and compatibility in slot 1.
for (const preset of ConfigPresets.items) {
  const config = ConfigPresets.values(preset.id);
  assert.equal(config.profiles.length, 2);
  assert.equal(config.profiles[1].values.wheel_passthrough, true);
  assert.ok('ignored_devices' in config.globals);
  for (const profile of config.profiles) assert.equal('ignored_devices' in profile.values, false);
  const toml = ConfigWriter.generateConfig(config);
  assert.equal((toml.match(/^\[\[profiles\]\]$/gm) || []).length, 1);
  assert.equal((toml.match(/^ignored_devices =/gm) || []).length, 1);
  assert.match(toml.split(/^\[\[profiles\]\]$/m)[1], /^wheel_passthrough = true$/m);
}
Profiles.init(ConfigPresets.values('responsive'));
Profiles.update({...Profiles.values(), device_event_debounce_milliseconds: 1234});
assert.equal(Profiles.modified(), false, 'device edits are outside profile preset tracking');
const compatibility = JSON.stringify(Profiles.all()[1]);
Profiles.applyPreset('auto-scroll');
assert.equal(Profiles.values().device_event_debounce_milliseconds, 1234);
assert.equal(JSON.stringify(Profiles.all()[1]), compatibility);
Profiles.update({...Profiles.values(), damping: 4});
assert.equal(Profiles.modified(), true);
Profiles.select(1);
assert.equal(Profiles.presetId(), 'compatibility');
assert.equal(Profiles.modified(), false);
Profiles.select(0);
assert.equal(Profiles.presetId(), 'auto-scroll');
assert.equal(Profiles.modified(), true);
Profiles.restore();
assert.equal(Profiles.modified(), false);
assert.equal(Profiles.values().device_event_debounce_milliseconds, 1234);
Profiles.add(); Profiles.setName('Reading'); Profiles.applyPreset('responsive');
Profiles.add(); Profiles.setName('Work'); Profiles.applyPreset('auto-scroll');
Profiles.select(2); Profiles.remove();
assert.equal(Profiles.all()[2].name, 'Work');
assert.equal(Profiles.presetId(), 'auto-scroll', 'preset identity follows the profile after deletion');
assert.equal(Profiles.modified(), false);
console.log('Config/profile preset scope, compatibility export and per-profile reset passed.');
