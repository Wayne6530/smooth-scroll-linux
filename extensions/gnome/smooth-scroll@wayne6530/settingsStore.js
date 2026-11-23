import GLib from 'gi://GLib';
import {SETTINGS, DEFAULT_CONFIG} from './settings.js';
import {RULES_TYPE, rulesFromVariant, rulesToVariant, normalizeRules} from './rules.js';

export function readRules(settings) {
    return rulesFromVariant(settings.get_value('profile-rules').deepUnpack());
}

export function writeRules(settings, rules) {
    if (!settings.set_value('profile-rules', new GLib.Variant(RULES_TYPE, rulesToVariant(rules))))
        throw new Error('Application rules could not be saved. Settings may be locked.');
}

export function readConfig(settings) {
    const config = JSON.parse(JSON.stringify(DEFAULT_CONFIG));
    for (const entry of SETTINGS)
        config[entry.group][entry.field] = settings.get_value(entry.key).deepUnpack();
    config.default_profile_id = settings.get_uint('default-profile-id');
    config.shell_profile_id = settings.get_uint('shell-profile-id');
    config.profile_rules = normalizeRules(readRules(settings));
    return config;
}

export class ConfigManager {
    constructor(settings, onReload) {
        this._settings = settings;
        this._onReload = onReload;
        this.config = null;
        this._changedId = 0;
    }

    start() {
        this._changedId = this._settings.connect('changed', () => {
            this.config = readConfig(this._settings);
            this._onReload(this.config);
        });
        this.config = readConfig(this._settings);
    }

    stop() {
        if (this._changedId)
            this._settings.disconnect(this._changedId);
        this._changedId = 0;
        this._settings = null;
    }
}
