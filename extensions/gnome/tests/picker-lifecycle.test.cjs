const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const root = __dirname + '/../smooth-scroll@wayne6530/';
const sources = ['bridgeProtocol.js', 'shellBridge.js'].map(file => fs.readFileSync(root + file, 'utf8')
  .replace(/^import .*;$/gm, '').replace(/^export /gm, '')).join('\n');
let nextId = 1;
const timers = new Map();
const owners = new Map();
let grabs = 0, actors = 0, exportCount = 0, sessionChanged;
let legacySeatState = null;
const types = Object.fromEntries(['KEY_PRESS', 'KEY_RELEASE', 'BUTTON_PRESS', 'BUTTON_RELEASE', 'MOTION', 'SCROLL',
  'TOUCH_BEGIN', 'TOUCH_UPDATE', 'TOUCH_END', 'TOUCH_CANCEL', 'ENTER'].map((key, i) => [key, i]));
const Clutter = {EventType: types, KEY_Escape: 27, EVENT_STOP: 1, EVENT_PROPAGATE: 0, GrabState: {ALL: 3}};
const sessionMode = {isLocked: false, isGreeter: false,
  connect(_name, callback) { sessionChanged = callback; return 1; }, disconnect() { sessionChanged = null; }};
const context = vm.createContext({
  Clutter,
  Gio: {DBus: {session: {}}, BusNameWatcherFlags: {NONE: 0},
    DBusExportedObject: {wrapJSObject: () => ({export() { exportCount++; }, unexport() { exportCount--; }})},
    bus_watch_name_on_connection(_bus, _name, _flags, _appeared, vanished) { const id = nextId++; owners.set(id, vanished); return id; },
    bus_unwatch_name(id) { owners.delete(id); }},
  GLib: {PRIORITY_DEFAULT: 0, SOURCE_REMOVE: false,
    Variant: class { constructor(_type, value) { this.value = value; } },
    timeout_add(_priority, _ms, callback) { const id = nextId++; timers.set(id, callback); return id; },
    source_remove(id) { timers.delete(id); }},
  St: {Label: class {
    constructor() { actors++; } set_position() {} connect() {} destroy() { actors--; }
  }},
  Shell: {ActionMode: {NONE: 0}},
  Main: {sessionMode, layoutManager: {primaryMonitor: {x: 0, y: 0}, uiGroup: {add_child() {}}},
    pushModal() { grabs++; return legacySeatState === null ? {} : {get_seat_state: () => legacySeatState}; },
    popModal() { grabs--; }},
});
vm.runInContext(sources + '\nglobalThis.Bridge = ShellBridge;', context);
const info = {app: 'test.desktop', appIds: ['test.desktop'], title: 'Test'};
let shellUI = false;
const bridge = new context.Bridge({shellInteractionActive: () => shellUI, windowAt: () => ({}), infoForWindow: window => window ? info : null},
  () => ({valid: true, connected: true, requestedProfileId: 8, activeProfileId: 2}));
const invocation = (owner = ':1.10') => ({get_sender: () => owner, replies: [], return_value(value) { this.replies.push(value?.value ? JSON.parse(value.value[0]) : null); }});
const event = (type, extra = {}) => ({type: () => types[type], get_button: () => 1, get_coords: () => [100, 100], get_key_symbol: () => 27, ...extra});
function clean() {
  assert.equal(grabs, 0); assert.equal(actors, 0); assert.equal(timers.size, 0); assert.equal(owners.size, 0);
}
function start() { const call = invocation(); bridge.PickWindowAsync([], call); assert.equal(grabs, 1); return call; }
let call = start();
const competing = invocation(':1.11');
bridge.PickWindowAsync([], competing);
assert.ok(competing.replies[0].error);
bridge.CancelPickAsync([], invocation(':1.11'));
assert.equal(grabs, 1, 'Other clients cannot cancel the picker');
assert.equal(bridge._event(event('ENTER')), Clutter.EVENT_PROPAGATE);
assert.equal(bridge._event(event('BUTTON_PRESS')), Clutter.EVENT_STOP);
assert.equal(call.replies.length, 0, 'Hold grab until release');
assert.equal(bridge._event(event('BUTTON_RELEASE')), Clutter.EVENT_STOP);
assert.equal(call.replies[0].app, 'test.desktop'); clean();
const touch = (type, slot) => event(type, {get_event_sequence: () => ({get_slot: () => slot})});
call = start();
bridge._event(touch('TOUCH_BEGIN', 0));
bridge._event(touch('TOUCH_BEGIN', 1));
bridge._event(touch('TOUCH_END', 1));
assert.equal(call.replies.length, 0, 'Other fingers cannot finish the selection');
bridge._event(touch('TOUCH_END', 0));
assert.equal(call.replies[0].app, 'test.desktop'); clean();
call = start(); bridge._event(touch('TOUCH_BEGIN', 1)); bridge._event(touch('TOUCH_CANCEL', 1));
assert.ok(call.replies[0].cancelled); clean();
call = start(); bridge._event(event('KEY_PRESS')); assert.ok(call.replies[0].cancelled); clean();
call = start(); bridge.CancelPickAsync([], invocation()); assert.ok(call.replies[0].cancelled); clean();
call = start(); shellUI = true; bridge._event(event('BUTTON_PRESS')); bridge._event(event('BUTTON_RELEASE'));
assert.ok(call.replies[0].error); shellUI = false; clean();
call = start();
const [timerId, timeout] = [...timers][0]; timers.delete(timerId); timeout();
assert.ok(call.replies[0].error.includes('timed out')); clean();
call = start(); [...owners.values()][0](); assert.ok(call.replies[0].error); clean();
call = start(); sessionMode.isLocked = true; sessionChanged(); assert.ok(call.replies[0].error); clean(); sessionMode.isLocked = false;
legacySeatState = 0;
call = invocation(); bridge.PickWindowAsync([], call); assert.ok(call.replies[0].error); clean();
legacySeatState = 3;
call = start(); bridge.destroy(); assert.ok(call.replies[0].error.includes('disabled')); clean();
assert.equal(exportCount, 0); assert.equal(sessionChanged, null);
console.log('Picker click capture, owner isolation, legacy/current grabs, Escape, timeout, disconnect, lock and disable cleanup passed.');
