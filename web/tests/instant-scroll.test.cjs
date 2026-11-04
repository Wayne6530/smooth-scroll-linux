// Run with: node web/tests/instant-scroll.test.cjs
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const context = vm.createContext({});
for (const name of ['param-schema', 'instant-scroll', 'physics-engine', 'config-writer']) {
  vm.runInContext(fs.readFileSync(path.join(__dirname, '../js', name + '.js'), 'utf8'), context);
}
vm.runInContext(`globalThis.api = { ParamSchema, InstantScroll, PhysicsEngine, ConfigWriter };`, context);
const { ParamSchema, InstantScroll, PhysicsEngine, ConfigWriter } = context.api;
ConfigWriter.init(ParamSchema);
assert.equal(InstantScroll.matches(ParamSchema.getDefaultValues()), false);
let cases = 0;
for (const distance of [40, 120, 240]) {
  for (const tick of [1000, 2000, 10000]) {
    const values = { ...ParamSchema.getDefaultValues(), wheel_tick_distance: distance, tick_interval_microseconds: tick };
    InstantScroll.apply(values);
    assert.ok(InstantScroll.matches(values));
    const events = [0, 1, 15, 100].map(timeMs => ({ timeMs, positive: true }));
    const result = PhysicsEngine.simulate(values, { events, maxDurationMs: 1000 });
    assert.equal(result.truncated, false);
    assert.deepEqual(Array.from(result.timeline, point => point.timeMs), [0, 1, 15, 100]);
    assert.deepEqual(Array.from(result.timeline, point => point.emittedDelta), Array(4).fill(distance));
    assert.ok(result.timeline.every(point => point.speed === 0));
    const reversed = PhysicsEngine.simulate(values, {
      events: events.map((event, i) => ({ ...event, positive: i % 2 === 0 })), maxDurationMs: 1000,
    });
    assert.equal(reversed.timeline.length, 4, 'reverse input must not be absorbed');
    const toml = ConfigWriter.generate(values);
    assert.ok(toml.includes('smooth_mode = 1'));
    assert.ok(toml.includes('min_deceleration = ' + values.min_deceleration));
    assert.ok(!/^scroll_effect\s*=/m.test(toml));
    cases++;
  }
}
console.log('Passed ' + cases + ' distance/tick combinations: immediate emission, bursts, reversal, and export.');

// Exercise editor updates: changing distance/timing must retain instant mode and
// recalculate exported values, while independent feature controls stay editable.
const editorSource = fs.readFileSync(path.join(__dirname, '../js/config-editor.js'), 'utf8');
const controls = new Map(InstantScroll.managedKeys.map(key => [key, {}]));
const editorValues = { ...ParamSchema.getDefaultValues(), auto_scroll_button: 274, drag_view_button: 0 };
InstantScroll.apply(editorValues);
const editorContext = vm.createContext({
  InstantScroll, currentValues: editorValues, instant: true,
  document: {
    querySelectorAll: selector => {
      const key = selector.match(/data-key="([^"]+)"/)[1];
      return controls.has(key) ? [controls.get(key)] : [];
    },
    getElementById: () => ({}),
  },
  updateAllParamStates() {}, validateConstraints() {},
  onParamChange: null, getValues: () => editorValues,
  isModeActive: () => true, dependenciesMet: () => true,
});
for (const name of ['fireChange', 'isParamActive', 'updateParamState']) {
  const fn = editorSource.match(new RegExp('  function ' + name + '\\([^]*?\\n  \\}'))[0];
  vm.runInContext(fn, editorContext);
}
for (const [key, value] of [['wheel_tick_distance', 240], ['tick_interval_microseconds', 1000]]) {
  editorValues[key] = value;
  editorContext.fireChange(key);
  assert.equal(editorContext.instant, true);
  assert.ok(InstantScroll.matches(editorValues));
  assert.equal(controls.get('min_deceleration').value, editorValues.min_deceleration);
  assert.equal(controls.get('max_deceleration').max, editorValues.max_deceleration);
}
for (const key of InstantScroll.managedKeys) {
  const input = {};
  editorContext.updateParamState({ classList: { toggle() {} }, querySelectorAll: () => [input] }, { key });
  assert.equal(input.disabled, true, key + ' must be locked in instant mode');
  assert.equal(editorContext.isParamActive({ key }), false);
}
for (const key of ['auto_scroll_button', 'auto_scroll_activation_mode', 'drag_view_activation_mode',
  'wheel_tick_distance', 'tick_interval_microseconds']) {
  assert.equal(editorContext.isParamActive({ key }), true, key + ' remains configurable');
}
editorContext.instant = false;
editorValues.damping = 5;
editorContext.fireChange('damping');
assert.equal(editorValues.damping, 5);
assert.equal(editorContext.isParamActive({ key: 'damping' }), true);
console.log('Editor instant-mode recalculation, control locking and independent features passed.');
