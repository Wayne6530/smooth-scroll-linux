import {ExtensionPreferences} from 'resource:///org/gnome/Shell/Extensions/js/extensions/prefs.js';
import {Preferences} from './preferences.js';

export default class SmoothScrollPreferences extends ExtensionPreferences {
    fillPreferencesWindow(window) {
        window._smoothScrollPreferences = new Preferences(window, this.getSettings());
    }
}
