const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync(__dirname + '/../smooth-scroll@wayne6530/extension.js', 'utf8');
const context = vm.createContext({Extension: class {}, Clutter: {EventType: {}}, console: {warn() {}},
  global: {get_pointer: () => [0, 0]}});
vm.runInContext(source.replace(/^import .*;$/gm, '').replace('export default class ', 'class ') + `
  globalThis.api = {normalizeConfig, profileForWindow, SmoothScrollIpcCompanionExtension};
`, context);
const {normalizeConfig, profileForWindow, SmoothScrollIpcCompanionExtension} = context.api;
const config = normalizeConfig(JSON.parse(JSON.stringify({shell_profile_id: 1, profile_rules: [
  {app: 'disabled', enabled: false, profile_id: 9},
  {app: 'browser', profile_id: 2, titles: [
    {title: '[', profile_id: 8},
    {title: 'Private', enabled: false, profile_id: 7},
    {title: '^Private', profile_id: 3},
    {title: '^Inherited'},
  ]},
]})));
assert.equal(profileForWindow(config.profile_rules, null), 0);
assert.equal(profileForWindow(config.profile_rules, {appIds: ['disabled'], title: ''}), 0);
assert.equal(profileForWindow(config.profile_rules, {appIds: ['browser'], title: 'Normal'}), 2);
assert.equal(profileForWindow(config.profile_rules, {appIds: ['browser'], title: 'Private tab'}), 3);
assert.equal(profileForWindow(config.profile_rules, {appIds: ['browser'], title: 'Inherited tab'}), 2);
assert.equal(profileForWindow(config.profile_rules, {appIds: ['other'], title: 'Private tab'}), 0);

const extension = Object.create(SmoothScrollIpcCompanionExtension.prototype);
extension._config = config;
extension._resolver = {shellInteractionActive: () => false};
const info = {appIds: ['browser'], title: 'Normal'};
assert.equal(extension._requestedProfileForWindow(null, null, 0, 0), 1);
assert.equal(extension._requestedProfileForWindow({}, info, 0, 0), 2);
extension._resolver.shellInteractionActive = () => true;
assert.equal(extension._requestedProfileForWindow({}, info, 0, 0), 1);
assert.equal(extension._isWheelPassthrough({requestedProfileId: 1, activeProfileId: 0}), false);
assert.equal(extension._isWheelPassthrough({requestedProfileId: 0, activeProfileId: 1}), true);

// Failed writes remain retryable; successful identical requests are deduplicated.
const requests = [];
let writable = false;
let snapshot = {valid: true, pid: 10, speed: 0};
extension._ipc = {setRequestedProfile(id) {requests.push(id); return writable;},
  readSnapshot: () => snapshot};
extension._setRequestedProfile(2);
assert.notEqual(extension._lastRequestedProfile, 2);
writable = true;
extension._setRequestedProfile(2);
extension._setRequestedProfile(2);
assert.deepEqual(requests, [2, 2]);
extension._lastPid = 10;
extension._hideDot = () => {};
extension._handlePointerState = () => extension._setRequestedProfile(2);
extension._tick();
assert.equal(requests.length, 2);
snapshot = {valid: true, pid: 11, speed: 0};
extension._tick();
assert.deepEqual(requests, [2, 2, 2]); // New daemon receives the same desired profile again.
snapshot = {valid: false};
writable = false;
extension._tick();
assert.equal(extension._lastPid, 0);
snapshot = {valid: true, pid: 12, speed: 0};
writable = true;
extension._tick();
assert.equal(extension._lastRequestedProfile, 2);
assert.deepEqual(requests, [2, 2, 2, 0, 2]);
console.log('GNOME profile rules, shell selection, pending state and daemon reconnect passed.');

// Leaving during Free Spin may send a brake, then further wheel input keeps
// scrolling alive. Releasing Free Spin must still refresh the indicator.
const indicatorExtension = Object.create(SmoothScrollIpcCompanionExtension.prototype);
indicatorExtension._config = config;
indicatorExtension._lastSpeed = 600;
indicatorExtension._anchorWindowKey = 'app';
indicatorExtension._stopRequestedForAnchor = false;
let pointerWindow = {key: 'app'};
indicatorExtension._resolver = {
  windowAt: () => pointerWindow,
  infoForWindow: () => info,
  keyForWindow: window => window?.key ?? null,
  shellInteractionActive: () => !pointerWindow,
};
let brakes = 0;
indicatorExtension._ipc = {setRequestedProfile: () => true, requestStop() {brakes++; return true;}};
const rendered = [];
indicatorExtension._updateDot = snapshot => rendered.push([snapshot.freeSpin, snapshot.speed]);
indicatorExtension._hideDot = () => {};
const held = {connected: true, activeProfileId: 0, requestedProfileId: 0,
  freeSpin: true, dragView: false, autoScroll: false, speed: 600};
indicatorExtension._handlePointerState(held, 0, 0);
pointerWindow = null;
indicatorExtension._handlePointerState({...held, requestedProfileId: 1}, 0, 0);
assert.equal(brakes, 1);
pointerWindow = {key: 'app'};
indicatorExtension._handlePointerState(held, 0, 0);
indicatorExtension._handlePointerState({...held, freeSpin: false, speed: 500}, 0, 0);
indicatorExtension._handlePointerState({...held, freeSpin: false, speed: 250}, 0, 0);
assert.deepEqual(rendered, [[true, 600], [true, 600], [true, 600], [false, 500], [false, 250]]);
assert.equal(brakes, 1);
console.log('GNOME indicator refreshes after Free Spin release despite an earlier pointer-leave brake.');
