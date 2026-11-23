import Adw from 'gi://Adw';
import Gdk from 'gi://Gdk';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import Gtk from 'gi://Gtk?version=4.0';
import {SETTING_GROUPS, SETTINGS} from './settings.js';
import {readRules, writeRules} from './settingsStore.js';
import {MATCH_MODES, titleMatcher} from './rules.js';
import {BUS_NAME, BUS_PATH, BUS_IFACE} from './bridgeProtocol.js';

const MODE_LABELS = ['Contains', 'Equals', 'Starts with', 'Ends with', 'Regular expression (advanced)'];
const copy = value => JSON.parse(JSON.stringify(value));

function button(label, callback, icon = null) {
    const widget = icon ? new Gtk.Button({icon_name: icon, tooltip_text: label}) : new Gtk.Button({label});
    widget.valign = Gtk.Align.CENTER;
    widget.connect('clicked', callback);
    return widget;
}

// Editors commit a draft, unlike the main preferences window's live settings.
function editorWindow(parent, title, width, height) {
    const window = new Adw.Window({
        title, transient_for: parent, modal: true,
        default_width: width, default_height: height,
    });
    const header = new Adw.HeaderBar({
        show_start_title_buttons: false, show_end_title_buttons: false,
        title_widget: new Adw.WindowTitle({title}),
    });
    header.pack_start(button('Cancel', () => window.close()));
    const page = new Adw.PreferencesPage();
    const toolbar = new Adw.ToolbarView({content: page});
    toolbar.add_top_bar(header);
    const overlay = new Adw.ToastOverlay({child: toolbar});
    window.set_content(overlay);
    window.add_toast = toast => overlay.add_toast(toast);
    const keys = new Gtk.EventControllerKey({propagation_phase: Gtk.PropagationPhase.CAPTURE});
    keys.connect('key-pressed', (_controller, key) => {
        if (key !== Gdk.KEY_Escape)
            return false;
        window.close();
        return true;
    });
    window.add_controller(keys);
    return {window, page, header};
}

function editorSave(window, header, label, callback) {
    const save = button(label, callback);
    save.add_css_class('suggested-action');
    header.pack_end(save);
    window.set_default_widget(save);
    return save;
}

function spin(value, min = 0, max = 0xffffffff, step = 1) {
    return new Gtk.SpinButton({
        adjustment: new Gtk.Adjustment({lower: min, upper: max, step_increment: step, page_increment: step * 10}),
        value, digits: step < 1 ? 2 : 0, numeric: true, valign: Gtk.Align.CENTER,
    });
}

function action(group, title, control, subtitle = null) {
    const row = new Adw.ActionRow({title, subtitle: subtitle || '', use_markup: false});
    row.add_suffix(control);
    row.activatable_widget = control;
    group.add(row);
    return row;
}

function toggle(group, title, active, onChange) {
    const control = new Gtk.Switch({active, valign: Gtk.Align.CENTER});
    action(group, title, control);
    control.connect('notify::active', () => onChange(control.active));
    return control;
}

function enableSwitch(label, active, onChange) {
    const control = new Gtk.Switch({active, valign: Gtk.Align.CENTER, tooltip_text: label});
    control.update_property([Gtk.AccessibleProperty.LABEL], [label]);
    control.connect('notify::active', () => onChange(control.active));
    return control;
}

function appIcon(rule) {
    try {
        if (rule.icon)
            return new Gtk.Image({gicon: Gio.Icon.new_for_string(rule.icon), pixel_size: 32});
    } catch (_error) {
        // A removed application's icon may no longer be usable.
    }
    return new Gtk.Image({icon_name: 'application-x-executable', pixel_size: 32});
}

export class Preferences {
    constructor(window, settings) {
        this.window = window;
        this.settings = settings;
        this.closed = false;
        this.picking = false;
        this.editor = null;
        this._statusBusy = false;
        this._cancellable = new Gio.Cancellable();
        window.set_default_size(780, 720);
        window.search_enabled = true;
        this._buildRulesPage();
        this._buildSettingsPages();
        this._statusTimer = GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, 2, () => {
            this._refreshStatus();
            return GLib.SOURCE_CONTINUE;
        });
        this._refreshStatus();
        window.connect('close-request', () => {
            this.destroy();
            return false;
        });
    }

    toast(message) {
        if (!this.closed)
            this.window.add_toast(new Adw.Toast({title: message, timeout: 6}));
    }

    async _call(method, timeout = 3000, cancellable = this._cancellable) {
        return new Promise((resolve, reject) => {
            Gio.DBus.session.call(BUS_NAME, BUS_PATH, BUS_IFACE, method, null,
                method === 'CancelPick' ? null : new GLib.VariantType('(s)'),
                Gio.DBusCallFlags.NO_AUTO_START, timeout, cancellable, (connection, result) => {
                    try {
                        const value = connection.call_finish(result);
                        resolve(method === 'CancelPick' ? null : JSON.parse(value.deepUnpack()[0]));
                    } catch (error) {
                        reject(error);
                    }
                });
        });
    }

    async _refreshStatus() {
        if (this.closed || this._statusBusy)
            return;
        this._statusBusy = true;
        try {
            const status = await this._call('GetStatus');
            if (this.closed)
                return;
            this.addButton.sensitive = !this.picking;
            this.statusRow.title = status.available && status.connected
                ? `Requested: ${status.requested}  ·  Active: ${status.active}`
                : 'Requested: —  ·  Active: —';
            this.statusRow.subtitle = !status.available
                ? 'Daemon unavailable or incompatible IPC protocol'
                : !status.connected ? 'Daemon has no connected input device'
                    : status.requested !== status.active ? 'Not yet applied' : '';
        } catch (_error) {
            if (!this.closed) {
                this.addButton.sensitive = false;
                this.statusRow.title = 'Requested: —  ·  Active: —';
                this.statusRow.subtitle = 'Enable the Smooth Scroll extension to select windows and read status.';
            }
        } finally {
            this._statusBusy = false;
        }
    }

    async pickWindow(parent = this.window) {
        if (this.picking || this.closed)
            return null;
        this.picking = true;
        this.addButton.sensitive = false;
        let parentClosed = false;
        const parentCloseId = parent.connect('close-request', () => {
            parentClosed = true;
            this._call('CancelPick', 1000, null).catch(() => {});
            return false;
        });
        // Check availability before hiding the UI.
        try {
            await this._call('GetStatus');
            if (this.closed || parentClosed)
                return null;
            parent.set_visible(false);
            this.editor?.set_visible(false);
            if (parent !== this.window)
                this.window.set_visible(false);
            // Allow the compositor to process the GTK unmap before capturing input.
            await new Promise(resolve => {
                this._hideWaitResolve = resolve;
                this._hideWait = GLib.timeout_add(GLib.PRIORITY_DEFAULT, 200, () => {
                    this._hideWait = 0;
                    this._hideWaitResolve = null;
                    resolve();
                    return GLib.SOURCE_REMOVE;
                });
            });
            if (this.closed || parentClosed)
                return null;
            const info = await this._call('PickWindow', 35000);
            if (info.error)
                throw new Error(info.error);
            return info.cancelled ? null : info;
        } catch (error) {
            // Also cancels a server grab if the client timed out.
            this._call('CancelPick', 1000, null).catch(() => {});
            if (!this.closed && !parentClosed)
                parent.add_toast(new Adw.Toast({title: `Window selection failed: ${error.message}`, timeout: 6}));
            return null;
        } finally {
            if (!parentClosed)
                parent.disconnect(parentCloseId);
            this.picking = false;
            if (!this.closed) {
                this.window.present();
                this.editor?.present();
                if (parent !== this.window && !parentClosed)
                    parent.present();
                this._refreshStatus();
            }
        }
    }

    _buildRulesPage() {
        const page = new Adw.PreferencesPage({title: 'Application rules', icon_name: 'view-list-symbolic'});
        this.window.add(page);
        const intro = new Adw.PreferencesGroup({
            description: 'Use the profile IDs from the Web configurator to choose a default profile and assign profiles to applications.',
        });
        this.statusRow = new Adw.ActionRow({title: 'Requested: —  ·  Active: —', subtitle: 'Connecting…', use_markup: false});
        intro.add(this.statusRow);
        page.add(intro);
        const defaults = new Adw.PreferencesGroup({title: 'Default profiles'});
        const fallback = spin(this.settings.get_uint('default-profile-id'));
        action(defaults, 'Default Profile ID', fallback, 'Used for applications without an enabled rule.');
        this.settings.bind('default-profile-id', fallback, 'value', Gio.SettingsBindFlags.DEFAULT);
        const id = spin(this.settings.get_uint('shell-profile-id'));
        action(defaults, 'Shell Profile ID', id, 'Used over the desktop, dock, overview and Shell menus. Default: 1.');
        this.settings.bind('shell-profile-id', id, 'value', Gio.SettingsBindFlags.DEFAULT);
        page.add(defaults);
        this.rulesGroup = new Adw.PreferencesGroup({
            title: 'Applications',
            description: 'Select an open window to add an application rule.',
        });
        this.addButton = button('Add application rule', async () => {
            const info = await this.pickWindow();
            if (!info || this.closed)
                return;
            const existing = readRules(this.settings).find(rule => rule.app === info.app);
            this._editRule(existing || {
                app: info.app, name: info.name, icon: info.icon, enabled: true, profile_id: this.settings.get_uint('default-profile-id'), titles: [],
            }, !existing, info);
        }, 'list-add-symbolic');
        this.addButton.sensitive = false;
        this.rulesGroup.header_suffix = this.addButton;
        page.add(this.rulesGroup);
        this._ruleRows = [];
        this._rulesChanged = this.settings.connect('changed::profile-rules', () => this._renderRules());
        this._renderRules();
    }

    _renderRules() {
        for (const row of this._ruleRows)
            this.rulesGroup.remove(row);
        this._ruleRows = [];
        const rules = readRules(this.settings);
        for (const rule of rules) {
            const row = new Adw.ActionRow({
                title: rule.name || rule.app,
                subtitle: `Profile ${rule.profile_id} · ${rule.titles.length} title exceptions`,
                use_markup: false,
            });
            row.add_prefix(appIcon(rule));
            row.add_suffix(enableSwitch(`Enable rule for ${rule.name || rule.app}`, rule.enabled, enabled => {
                try {
                    const current = readRules(this.settings);
                    const target = current.find(item => item.app === rule.app);
                    if (target) {
                        target.enabled = enabled;
                        writeRules(this.settings, current);
                    }
                } catch (error) {
                    this.toast(error.message);
                    this._renderRules();
                }
            }));
            row.add_suffix(button('Edit application rule', () => this._editRule(rule, false), 'document-edit-symbolic'));
            row.add_suffix(button('Delete application rule', () => this._deleteRule(rule), 'user-trash-symbolic'));
            this.rulesGroup.add(row);
            this._ruleRows.push(row);
        }
        if (!rules.length) {
            const row = new Adw.ActionRow({title: 'No application rules', subtitle: 'Open the target application, then select its window to add a rule.'});
            this.rulesGroup.add(row);
            this._ruleRows.push(row);
        }
    }

    _deleteRule(rule) {
        try {
            const rules = readRules(this.settings);
            const index = rules.findIndex(item => item.app === rule.app);
            if (index < 0)
                return;
            const [removed] = rules.splice(index, 1);
            writeRules(this.settings, rules);
            const toast = new Adw.Toast({title: 'Application rule deleted', button_label: 'Undo', timeout: 8});
            toast.connect('button-clicked', () => {
                try {
                    const current = readRules(this.settings);
                    if (current.some(item => item.app === removed.app))
                        return;
                    current.splice(Math.min(index, current.length), 0, removed);
                    writeRules(this.settings, current);
                } catch (error) {
                    this.toast(error.message);
                }
            });
            this.window.add_toast(toast);
        } catch (error) {
            this.toast(error.message);
        }
    }

    _editRule(original, isNew, picked = null) {
        if (this.editor) {
            this.editor.present();
            return;
        }
        const baseline = JSON.stringify(original);
        const draft = copy(original);
        const {window: editor, page, header} = editorWindow(this.window,
            isNew ? 'Add application rule' : 'Edit application rule', 700, 650);
        this.editor = editor;
        editor.connect('close-request', () => {
            this._titleDialog?.close();
            this.editor = null;
            return false;
        });
        const app = new Adw.PreferencesGroup({title: draft.name || draft.app});
        page.add(app);
        const identity = new Adw.ActionRow({title: 'Application identity', subtitle: draft.app, use_markup: false});
        identity.add_prefix(appIcon(draft));
        app.add(identity);
        const id = spin(draft.profile_id);
        action(app, 'Default Profile ID', id);
        id.connect('value-changed', () => { draft.profile_id = id.get_value(); });
        const titles = new Adw.PreferencesGroup({title: 'Window title exceptions', description: 'Checked from top to bottom. No match uses the application default.'});
        page.add(titles);
        let titleRows = [];
        const render = () => {
            for (const row of titleRows)
                titles.remove(row);
            titleRows = [];
            draft.titles.forEach((condition, index) => {
                const row = new Adw.ActionRow({
                    title: `${MODE_LABELS[MATCH_MODES.indexOf(condition.mode)]}: ${condition.title}`,
                    subtitle: `Profile ${condition.profile_id}${condition.ignore_case ? ' · Ignore case' : ''}`,
                    use_markup: false,
                });
                row.add_suffix(enableSwitch(`Enable exception for ${condition.title}`, condition.enabled, enabled => {
                    condition.enabled = enabled;
                }));
                const up = button('Move up', () => {
                    [draft.titles[index - 1], draft.titles[index]] = [draft.titles[index], draft.titles[index - 1]];
                    render();
                }, 'go-up-symbolic');
                up.sensitive = index > 0;
                const down = button('Move down', () => {
                    [draft.titles[index + 1], draft.titles[index]] = [draft.titles[index], draft.titles[index + 1]];
                    render();
                }, 'go-down-symbolic');
                down.sensitive = index < draft.titles.length - 1;
                row.add_suffix(up);
                row.add_suffix(down);
                row.add_suffix(button('Edit exception', () => this._editTitle(editor, condition, draft, picked, updated => {
                    draft.titles[index] = updated;
                    render();
                }), 'document-edit-symbolic'));
                row.add_suffix(button('Remove exception', () => {
                    draft.titles.splice(index, 1);
                    render();
                }, 'user-trash-symbolic'));
                titles.add(row);
                titleRows.push(row);
            });
        };
        titles.header_suffix = button('Add title exception', () => this._editTitle(editor, {
            enabled: true, mode: 'contains', title: picked?.title || '', ignore_case: false, profile_id: draft.profile_id,
        }, draft, picked, condition => {
            draft.titles.push(condition);
            render();
        }), 'list-add-symbolic');
        render();
        editorSave(editor, header, 'Save', () => {
            try {
                id.update();
                draft.profile_id = id.get_value();
                const rules = readRules(this.settings);
                const index = rules.findIndex(rule => rule.app === original.app);
                if ((!isNew && (index < 0 || JSON.stringify(rules[index]) !== baseline)) ||
                    (isNew && index >= 0))
                    throw new Error('This rule changed in another window. Cancel and reopen it before saving.');
                if (isNew)
                    rules.push(draft);
                else
                    rules[index] = draft;
                writeRules(this.settings, rules);
                editor.close();
            } catch (error) {
                editor.add_toast(new Adw.Toast({title: error.message}));
            }
        });
        editor.present();
    }

    _editTitle(parent, original, app, picked, onSave) {
        const draft = copy(original);
        const {window: dialog, page, header} = editorWindow(parent,
            'Window title exception', 620, 600);
        const group = new Adw.PreferencesGroup({description: 'Match title text, not website addresses. Keep a stable part of the title; ordinary text is matched literally. Saving this exception updates the application draft; save the application rule to apply it.'});
        page.add(group);
        const mode = new Adw.ComboRow({title: 'Match method', model: Gtk.StringList.new(MODE_LABELS), selected: MATCH_MODES.indexOf(draft.mode)});
        group.add(mode);
        const text = new Adw.EntryRow({title: 'Title text', text: draft.title, activates_default: true});
        text.add_suffix(button('Get title from a window', async () => {
            const info = await this.pickWindow(dialog);
            if (!info || this.closed)
                return;
            if (info.app !== app.app) {
                dialog.add_toast(new Adw.Toast({title: 'Select a window from this application.'}));
                return;
            }
            picked = info;
            text.text = info.title;
            updatePreview();
        }, 'find-location-symbolic'));
        group.add(text);
        const errorLabel = new Gtk.Label({wrap: true, xalign: 0, margin_start: 12, margin_end: 12, visible: false});
        errorLabel.add_css_class('error');
        group.add(errorLabel);
        toggle(group, 'Ignore case', draft.ignore_case, value => { draft.ignore_case = value; updatePreview(); });
        const id = spin(draft.profile_id);
        action(group, 'Profile ID', id);
        const preview = new Adw.ActionRow({title: 'Condition preview', use_markup: false});
        group.add(preview);
        let save;
        const updatePreview = () => {
            draft.mode = MATCH_MODES[mode.selected];
            draft.title = text.text;
            try {
                const matches = titleMatcher(draft);
                save.sensitive = true;
                text.remove_css_class('error');
                errorLabel.visible = false;
                preview.subtitle = picked ? `${picked.title}\n${matches(picked.title) ? 'Matches' : 'Does not match'}` : 'Valid condition. Select a window to preview.';
            } catch (error) {
                save.sensitive = false;
                text.add_css_class('error');
                errorLabel.label = error.message;
                errorLabel.visible = true;
                preview.subtitle = 'Enter a valid condition to preview.';
            }
        };
        mode.connect('notify::selected', updatePreview);
        text.connect('notify::text', updatePreview);
        this._titleDialog = dialog;
        dialog.connect('close-request', () => { this._titleDialog = null; return false; });
        save = editorSave(dialog, header, 'Save', () => {
            updatePreview();
            try {
                titleMatcher(draft);
                id.update();
                draft.profile_id = id.get_value();
                onSave(draft);
                dialog.close();
            } catch (error) {
                dialog.add_toast(new Adw.Toast({title: error.message}));
            }
        });
        updatePreview();
        dialog.present();
    }

    _buildSettingsPages() {
        const general = new Adw.PreferencesPage({title: 'Behavior', icon_name: 'preferences-system-symbolic'});
        const appearance = new Adw.PreferencesPage({title: 'Indicators', icon_name: 'applications-graphics-symbolic'});
        this.window.add(general);
        this.window.add(appearance);
        for (const [name, title] of Object.entries(SETTING_GROUPS)) {
            const group = new Adw.PreferencesGroup({title});
            const isBehavior = name === 'scroll' || name === 'pointer_fallback';
            (isBehavior ? general : appearance).add(group);
            const entries = SETTINGS.filter(entry => entry.group === name);
            for (const entry of entries) {
                if (entry.type === 'b') {
                    const control = new Gtk.Switch({valign: Gtk.Align.CENTER});
                    action(group, entry.title, control);
                    this.settings.bind(entry.key, control, 'active', Gio.SettingsBindFlags.DEFAULT);
                } else if (entry.type === 'd') {
                    const control = spin(this.settings.get_double(entry.key), entry.min, entry.max, entry.step);
                    action(group, entry.title, control);
                    this.settings.bind(entry.key, control, 'value', Gio.SettingsBindFlags.DEFAULT);
                } else {
                    // ColorButton is available throughout the supported GTK4 range.
                    const control = new Gtk.ColorButton({use_alpha: false, valign: Gtk.Align.CENTER});
                    action(group, entry.title, control);
                    control.connect('color-set', () => {
                        const rgba = control.get_rgba();
                        const hex = [rgba.red, rgba.green, rgba.blue].map(v => Math.round(v * 255).toString(16).padStart(2, '0')).join('');
                        this.settings.set_string(entry.key, `#${hex}`);
                    });
                    const refreshColor = () => {
                        const value = new Gdk.RGBA();
                        if (value.parse(this.settings.get_string(entry.key)))
                            control.set_rgba(value);
                    };
                    const changed = this.settings.connect(`changed::${entry.key}`, refreshColor);
                    refreshColor();
                    this._colorSignals ??= [];
                    this._colorSignals.push(changed);
                }
            }
            group.header_suffix = button('Reset this group to defaults', () => {
                for (const entry of entries)
                    this.settings.reset(entry.key);
            }, 'edit-undo-symbolic');
        }
    }

    destroy() {
        if (this.closed)
            return;
        this.closed = true;
        if (this.picking)
            this._call('CancelPick', 1000, null).catch(() => {});
        if (this._hideWait) {
            GLib.source_remove(this._hideWait);
            this._hideWait = 0;
            this._hideWaitResolve?.();
        }
        this._titleDialog?.close();
        this.editor?.close();
        this._cancellable.cancel();
        GLib.source_remove(this._statusTimer);
        this.settings.disconnect(this._rulesChanged);
        for (const signal of this._colorSignals || [])
            this.settings.disconnect(signal);
    }
}
