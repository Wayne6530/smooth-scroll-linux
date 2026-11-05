const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const context = vm.createContext({ structuredClone });
for (const file of ['param-schema', 'profile-presets', 'config-presets', 'profiles', 'config-writer', 'physics-engine']) {
  vm.runInContext(fs.readFileSync(path.join(__dirname, '../js', file + '.js'), 'utf8'), context);
}
vm.runInContext('globalThis.api = { ParamSchema, ConfigPresets, Profiles, ConfigWriter, PhysicsEngine };', context);
const { ParamSchema, ConfigPresets, Profiles, ConfigWriter, PhysicsEngine } = context.api;
const base = ParamSchema.getDefaultValues();
base.initial_speed = 999;
base.keyboard_passthrough_keys = [29];
const config = ConfigPresets.values('inertia');
config.profiles[0].values = base;
Profiles.init(config);
assert.equal(Profiles.add(), 2);
assert.equal(Profiles.values().initial_speed, 600);
assert.equal(Profiles.presetId(), 'inertia');
assert.equal(Profiles.modified(), false);
assert.equal(Profiles.values().min_speed_change_upperbound, 512);
assert.deepEqual([...Profiles.values().keyboard_passthrough_keys], [29, 97, 125, 126]);

Profiles.setName('An "independent" profile');
Profiles.update({ ...Profiles.values(), keyboard_passthrough_keys: [42, 29], damping: 8 });
assert.deepEqual([...new Set(Profiles.all().flatMap(item => item.values.keyboard_passthrough_keys))].sort((a,b) => a-b), [29, 42]);
Profiles.select(0);
assert.equal(Profiles.values().initial_speed, 999);
assert.equal(Profiles.values().damping, base.damping);
Profiles.select(1);
const fixed = JSON.stringify(Profiles.all()[1]);
Profiles.setName('Changed');
Profiles.update({ ...Profiles.values(), wheel_passthrough: false, free_spin_button: 273,
  device_event_debounce_milliseconds: 750 });
Profiles.applyPreset('responsive');
Profiles.restore();
assert.equal(JSON.stringify(Profiles.all()[1]), fixed);
assert.equal(Profiles.config().globals.device_event_debounce_milliseconds, 750);
assert.equal(Profiles.values().wheel_passthrough, true);
assert.equal(Profiles.values().drag_view_button, 0);
Profiles.remove();
assert.equal(Profiles.selected(), 1);
assert.equal(Profiles.all().length, 3);
ConfigWriter.init(ParamSchema);
const toml = ConfigWriter.generateConfig(Profiles.config());
assert.equal((toml.match(/^\[\[profiles\]\]$/gm) || []).length, 2);
assert.equal((toml.match(/^ignored_devices =/gm) || []).length, 1);
const sections = toml.split(/^\[\[profiles\]\]$/m);
assert.doesNotMatch(toml, /^id =/m);
assert.match(sections[2], /^name = "An \\"independent\\" profile"$/m);
for (const section of [sections[0]]) {
  for (const param of ParamSchema.params) {
    if (param.uiOnly || Profiles.globalKeys.includes(param.key)) continue;
    assert.match(section, new RegExp('^' + param.key + ' = ', 'm'), param.key);
  }
}
assert.match(sections[1], /^wheel_passthrough = true$/m);
const result = PhysicsEngine.simulate({ ...Profiles.defaults(), wheel_passthrough: true }, {
  events: [{ timeMs: 0 }, { timeMs: 100, positive: false }], maxDurationMs: 500,
});
assert.deepEqual(Array.from(result.timeline, point => point.emittedDelta), [120, -120]);
assert.ok(result.timeline.every(point => point.speed === 0));
assert.equal(result.truncated, false);
Profiles.select(2);
Profiles.remove();
assert.equal(Profiles.selected(), 1);
Profiles.remove();
assert.equal(Profiles.all().length, 2);
console.log('Independent profiles, IDs, union, global export and raw wheel simulation passed.');

Profiles.add(); Profiles.add(); Profiles.select(2); Profiles.remove();
assert.deepEqual(Array.from(Profiles.all(), item => item.id), [0, 1, 2]);
