import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import {SETTINGS, DEFAULT_CONFIG} from '../smooth-scroll@wayne6530/settings.js';
import {ConfigManager, readRules, writeRules, readConfig} from '../smooth-scroll@wayne6530/settingsStore.js';
import {profileForWindow} from '../smooth-scroll@wayne6530/rules.js';

function assert(value, message) {
    if (!value)
        throw new Error(message);
}
const settings = new Gio.Settings({schema_id: 'org.gnome.shell.extensions.smooth-scroll'});
for (const entry of SETTINGS) {
    assert(settings.get_value(entry.key).get_type_string() === entry.type, `Type: ${entry.key}`);
    assert(settings.get_value(entry.key).deepUnpack() === entry.default, `Schema default: ${entry.key}`);
    assert(DEFAULT_CONFIG[entry.group][entry.field] === entry.default, `Runtime default: ${entry.key}`);
    if (entry.type === 'd') {
        const schemaKey = settings.settings_schema.get_key(entry.key);
        for (const value of [entry.min, entry.max])
            assert(schemaKey.range_check(new GLib.Variant('d', value)), `Boundary: ${entry.key}`);
        assert(!schemaKey.range_check(new GLib.Variant('d', entry.min - 1)), `Lower range: ${entry.key}`);
        assert(!schemaKey.range_check(new GLib.Variant('d', entry.max + 1)), `Upper range: ${entry.key}`);
    }
}
let notifications = 0;
const manager = new ConfigManager(settings, () => notifications++);
manager.start();
settings.set_double('dot-size', 17);
assert(manager.config.dot.size === 17 && notifications === 1, 'Settings changes reach runtime');
const rules = [{app: 'test.desktop', name: 'Test', icon: '', enabled: true, profile_id: 0xffffffff,
    titles: [{enabled: true, mode: 'contains', title: 'a.b', ignore_case: true, profile_id: 8}]}];
writeRules(settings, rules);
assert(JSON.stringify(readRules(settings)) === JSON.stringify(rules), 'GVariant round-trip');
assert(profileForWindow(manager.config.profile_rules, {appIds: ['test.desktop'], title: 'A.B'}) === 8, 'Compiled runtime matching');
assert(profileForWindow(manager.config.profile_rules, {appIds: ['test.desktop'], title: 'aXb'}) === 0xffffffff, 'Literal fallback');
assert(manager.config.default_profile_id === 0, 'Default application profile starts at zero');
settings.set_uint('default-profile-id', 12);
assert(manager.config.default_profile_id === 12, 'Application fallback updates live');
settings.set_uint('shell-profile-id', 0xffffffff);
assert(readConfig(settings).shell_profile_id === 0xffffffff, 'Shell profile range');
assert(readConfig(settings).auto_scroll.size === 24, 'Unified Auto Scroll default');
manager.stop();
const before = notifications;
settings.reset('dot-size');
assert(notifications === before, 'Manager disconnects on disable');
print('GSettings schema defaults/ranges, runtime notifications, rule persistence and cleanup passed.');
