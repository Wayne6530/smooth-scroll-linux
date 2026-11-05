const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const context = vm.createContext({});
for (const file of ['param-schema', 'physics-engine', 'config-writer']) {
  vm.runInContext(fs.readFileSync(path.join(__dirname, '../js', file + '.js'), 'utf8'), context);
}
vm.runInContext('globalThis.api = { ParamSchema, PhysicsEngine, ConfigWriter };', context);
const { ParamSchema, PhysicsEngine, ConfigWriter } = context.api;
const values = { ...ParamSchema.getDefaultValues(), wheel_passthrough: true,
  auto_scroll_button: 275, auto_scroll_latch_on_release: true, auto_scroll_wheel_action: 1,
  drag_view_button: 274, drag_view_activation_mode: 1 };
const source = fs.readFileSync(path.join(__dirname, '../js/config-editor.js'), 'utf8');
const editor = vm.createContext({currentValues: values});
vm.runInContext(source.match(/  function dependenciesMet\(param\) \{[\s\S]*?\n  \}/)[0], editor);
const param = key => ParamSchema.params.find(p => p.key === key);
for (const key of ['free_spin_button', 'smooth_mode', 'wheel_tick_distance', 'damping',
  'use_reverse_scroll_braking', 'use_mouse_movement_braking', 'initial_speed',
  'speed_smooth_window_microseconds', 'auto_scroll_wheel_action']) {
  assert.equal(editor.dependenciesMet(param(key)), false, key);
}
for (const key of ['wheel_passthrough', 'keyboard_braking_keys', 'keyboard_passthrough_keys',
  'drag_view_button', 'drag_view_speed', 'auto_scroll_button', 'auto_scroll_speed_factor',
  'auto_scroll_exit_button_mode', 'tick_interval_microseconds']) {
  assert.equal(editor.dependenciesMet(param(key)), true, key);
}
values.wheel_passthrough = false;
assert.equal(editor.dependenciesMet(param('auto_scroll_wheel_action')), true);
values.auto_scroll_latch_on_release = false;
assert.equal(editor.dependenciesMet(param('auto_scroll_wheel_action')), false);
values.auto_scroll_button = 0;
assert.equal(editor.dependenciesMet(param('auto_scroll_speed_factor')), false);
values.wheel_passthrough = true;
ConfigWriter.init(ParamSchema);
const toml = ConfigWriter.generate(values);
assert.match(toml, /^auto_scroll_wheel_action = 1$/m, 'inactive settings retain their values');
const shipped = fs.readFileSync(path.join(__dirname, '../../config/smooth-scroll.toml'), 'utf8');
for (const p of ParamSchema.params.filter(p => p['disabled-when'])) {
  const block = shipped.split('\n\n').find(block => block.includes('\n' + p.key + ' ='));
  assert.ok(block.includes('# @disabled-when: wheel_passthrough'), p.key);
}
// Physical wheel ticks still pass through while Drag View owns pointer movement.
const scenario = {events: [{timeMs: 10}, {timeMs: 20, positive: false}], maxDurationMs: 100};
const features = {events: [{timeUs: 0, type: 'drag-view', value: 1}]};
const raw = PhysicsEngine.simulate(values, scenario, features);
assert.deepEqual(Array.from(raw.timeline.filter(p => p.emittedDelta), p => p.emittedDelta), [120, -120]);
const processed = PhysicsEngine.simulate({...values, wheel_passthrough: false}, scenario, features);
assert.ok(processed.timeline.every(p => !p.emittedDelta), 'Drag View consumes processed wheel input');
console.log('Wheel passthrough routing, feature dependencies and retained export values passed.');
