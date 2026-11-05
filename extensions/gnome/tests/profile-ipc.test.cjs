// node extensions/gnome/tests/profile-ipc.test.cjs
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync(__dirname + '/../smooth-scroll@wayne6530/extension.js', 'utf8');
const bytes = new Uint8Array(32);
const view = new DataView(bytes.buffer);
let offset = 0;
let writes = 0;
const file = {
  query_exists: () => true,
  read: () => ({can_seek: () => true, seek: () => true, close() {},
    read_bytes: () => ({get_data: () => bytes})}),
  open_readwrite: () => ({can_seek: () => true, seek(value) { offset = value; return true; }, close() {},
    get_output_stream: () => ({write_all(value) { bytes.set(value, offset); writes++; }, flush() {}})}),
};
const context = vm.createContext({Gio: {File: {new_for_path: () => file}}, GLib: {SeekType: {SET: 0}},
  viewForBytes: data => new DataView(data.buffer, data.byteOffset, data.byteLength)});
vm.runInContext(source.slice(source.indexOf('const IPC_PATH'), source.indexOf('const CONFIG_DIR_NAME')) +
  source.slice(source.indexOf('class SmoothScrollIpcClient'), source.indexOf('class ConfigManager')) +
  '\nglobalThis.client = new SmoothScrollIpcClient("test");', context);
const client = context.client;
view.setUint32(0, 0x53530002, true);
view.setUint32(4, 123, true);
assert.equal(client.setRequestedProfile(42), false);
assert.equal(writes, 0);
view.setUint32(0, 0x53530003, true);
view.setUint32(0x18, 7, true);
view.setUint32(8, 0, true);
assert.equal(client.setRequestedProfile(42), true);
assert.equal(view.getUint32(0x10, true), 42);
const state = client.readSnapshot();
assert.equal(state.requestedProfileId, 42);
assert.equal(state.activeProfileId, 7);
assert.equal('wheelPassthrough' in state, false);
view.setUint32(4, 0, true);
assert.equal(client.setRequestedProfile(0), false);
assert.equal(writes, 1);
console.log('GNOME profile IPC fields and incompatible/stopped-daemon write guards passed.');
