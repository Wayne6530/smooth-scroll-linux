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
