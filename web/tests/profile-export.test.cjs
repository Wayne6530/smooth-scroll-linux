const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const context = vm.createContext({structuredClone});
for (const file of ['param-schema', 'profile-presets', 'config-presets', 'instant-scroll', 'config-writer', 'physics-engine']) {
  vm.runInContext(fs.readFileSync(path.join(__dirname, '../js', file + '.js'), 'utf8'), context);
}
const {ConfigPresets, ProfilePresets, ParamSchema, ConfigWriter, PhysicsEngine} = vm.runInContext(
  '({ConfigPresets, ProfilePresets, ParamSchema, ConfigWriter, PhysicsEngine})', context);
ConfigWriter.init(ParamSchema);
const config = ConfigPresets.values('inertia');
const global = config.profiles[0].values;
global.wheel_passthrough = true;
global.damping = 7;
global.auto_scroll_button = 0;
global.auto_scroll_speed_factor = 123;
let output = ConfigWriter.generateConfig(config);
let sections = output.split(/^\[\[profiles\]\]$/m);
assert.match(sections[0], /^damping = 7$/m);
assert.match(sections[0], /^auto_scroll_speed_factor = 123$/m);
assert.match(sections[0], /# @label-en: Damping/);
assert.doesNotMatch(sections[1], /^(damping|smooth_mode|tick_interval_microseconds|auto_scroll_wheel_action|drag_view_speed|use_reverse_scroll_braking) =/m);
for (const key of ['free_spin_button', 'drag_view_button', 'auto_scroll_button']) assert.match(sections[1], new RegExp('^' + key + ' = 0$', 'm'));
assert.match(sections[1], /^keyboard_passthrough_keys = \[\]$/m);
const custom = {name:'Custom', values: ProfilePresets.values('responsive')};
config.profiles.push(custom);
custom.values.use_mouse_movement_braking = false;
custom.values.auto_scroll_button = 0;
custom.values.mouse_movement_window_milliseconds = -1; // Omitted; daemon gets its valid default.
custom.values.auto_scroll_speed_factor = -1;
custom.values.auto_scroll_wheel_action = 1;
const saved = JSON.stringify(config);
sections = ConfigWriter.generateConfig(config).split(/^\[\[profiles\]\]$/m);
assert.equal(JSON.stringify(config), saved, 'export must not change remembered settings');
assert.match(sections[2], /^use_reverse_scroll_braking = false$/m);
assert.match(sections[2], /^use_mouse_movement_braking = false$/m);
assert.doesNotMatch(sections[2], /^(max_reverse_scroll_braking_times|mouse_movement_window_milliseconds|auto_scroll_speed_factor|auto_scroll_wheel_action|initial_speed|speed_factor) =/m);
// Turning the feature back on exposes its invalid remembered value; do not replace it silently.
custom.values.auto_scroll_button = 275;
assert.ok(ConfigWriter.validateConfig(config).some(issue => issue.profileId === 2 && issue.keys.includes('auto_scroll_speed_factor')));
custom.values = ProfilePresets.values('auto-scroll');
custom.values.wheel_passthrough = true;
custom.values.auto_scroll_wheel_action = 1;
custom.values.auto_scroll_latch_on_release = false;
sections = ConfigWriter.generateConfig(config).split(/^\[\[profiles\]\]$/m);
assert.match(sections[2], /^tick_interval_microseconds = 2000$/m);
assert.match(sections[2], /^auto_scroll_speed_exponent = 2$/m);
assert.doesNotMatch(sections[2], /^(auto_scroll_wheel_action|auto_scroll_exit_button_mode|damping) =/m);
// Instant-mode values are disabled in the UI because they are derived, not irrelevant.
custom.values.wheel_passthrough = false;
sections = ConfigWriter.generateConfig(config).split(/^\[\[profiles\]\]$/m);
assert.match(sections[2], /^min_deceleration = 15150001$/m);
assert.match(sections[2], /^smooth_mode = 1$/m);
const loaded = ProfilePresets.defaults();
for (const line of sections[2].split('\n')) {
  const match = line.match(/^(\w+) = (.+)$/);
  if (match && match[1] !== 'name') loaded[match[1]] = JSON.parse(match[2]);
}
const scenario = {events: [{timeMs:0}, {timeMs:20, positive:false}], maxDurationMs:100};
const expected = PhysicsEngine.simulate(custom.values, scenario);
const actual = PhysicsEngine.simulate(loaded, scenario);
assert.deepEqual(JSON.parse(JSON.stringify(actual)), JSON.parse(JSON.stringify(expected)));
// Global settings are emitted even when inactive, so their constraints still apply.
global.min_deceleration = 10000;
global.max_deceleration = 6000;
assert.ok(ConfigWriter.validateConfig(config).some(issue => issue.profileId === 0 && issue.kind === 'order'));
assert.throws(() => ConfigWriter.generateConfig(config), /Invalid configuration/);
global.min_deceleration = 1420;
global.auto_scroll_speed_factor = 0;
assert.ok(ConfigWriter.validateConfig(config).some(issue => issue.profileId === 0 && issue.keys.includes('auto_scroll_speed_factor')));
global.auto_scroll_speed_factor = 123;
custom.values.auto_scroll_button = 273;
custom.values.free_spin_button = 273;
assert.ok(ConfigWriter.validateConfig(config).some(issue => issue.profileId === 2 && issue.kind === 'buttons'));
console.log('Compact profiles preserve semantics; global values and comments survive; invalid exports are blocked.');
