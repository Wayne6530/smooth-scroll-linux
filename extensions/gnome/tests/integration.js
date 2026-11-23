import Adw from 'gi://Adw';
import Gtk from 'gi://Gtk?version=4.0';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import System from 'system';
import {Preferences} from '../smooth-scroll@wayne6530/preferences.js';
import {readRules} from '../smooth-scroll@wayne6530/settingsStore.js';

Adw.init();
const app = new Gtk.Application({application_id: 'org.example.SmoothScrollFixture'});
app.register(null);
const loop = new GLib.MainLoop(null, false);
const deadline = GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, 45, () => {
    printerr('Integration test timed out');
    System.exit(1);
    return GLib.SOURCE_REMOVE;
});
let failure = false;
const wait = ms => new Promise(resolve => GLib.timeout_add(GLib.PRIORITY_DEFAULT, ms, () => { resolve(); return GLib.SOURCE_REMOVE; }));
function assert(value, message) { if (!value) throw new Error(message); }
async function evaluate(code) {
    return new Promise((resolve, reject) => Gio.DBus.session.call('org.gnome.Shell', '/org/gnome/Shell', 'org.gnome.Shell', 'Eval',
        new GLib.Variant('(s)', [code]), new GLib.VariantType('(bs)'), Gio.DBusCallFlags.NONE, 3000, null, (conn, result) => {
            try {
                const [ok, value] = conn.call_finish(result).deepUnpack();
                if (!ok) throw new Error(value);
                resolve(value ? JSON.parse(value) : null);
            } catch (error) { reject(error); }
        }));
}
async function run() {
    // A fresh Shell opens the overview. Leave it before mapping/closing dialogs:
    // otherwise its pending map animations can outlive test windows.
    await evaluate('Main.overview.hide(); true');
    await wait(300);
    const settings = new Gio.Settings({schema_id: 'org.gnome.shell.extensions.smooth-scroll'});
    const window = new Adw.PreferencesWindow({application: app});
    const prefs = new Preferences(window, settings);
    window.present();
    await wait(500);
    assert(prefs.addButton.sensitive, 'Bridge availability enables picker');
    const rule = {app: 'org.example.SmoothScrollFixture.desktop', name: 'Fixture', icon: '', enabled: true, profile_id: 7,
        titles: [{enabled: true, mode: 'contains', title: 'Docs', ignore_case: false, profile_id: 8}]};
    const widgets = function* (widget) {
        yield widget;
        for (let child = widget.get_first_child(); child; child = child.get_next_sibling())
            yield* widgets(child);
    };
    prefs._editRule(rule, true, {title: 'Docs', app: rule.app});
    await wait(150);
    prefs._editTitle(prefs.editor, rule.titles[0], rule, {title: 'Docs', app: rule.app}, () => {});
    await wait(150);
    const exceptionWindow = prefs._titleDialog;
    const exceptionWidgets = [...widgets(exceptionWindow)];
    const exceptionSave = exceptionWidgets.find(widget => widget instanceof Gtk.Button && widget.label === 'Save');
    const exceptionCancel = exceptionWidgets.find(widget => widget instanceof Gtk.Button && widget.label === 'Cancel');
    const titleEntry = exceptionWidgets.find(widget => widget instanceof Adw.EntryRow);
    const mode = exceptionWidgets.find(widget => widget instanceof Adw.ComboRow);
    assert(exceptionSave.get_ancestor(Adw.HeaderBar.$gtype), 'Exception Save is in the header');
    assert(exceptionCancel.get_ancestor(Adw.HeaderBar.$gtype), 'Exception Cancel is in the header');
    assert(exceptionWindow.get_default_widget() === exceptionSave, 'Exception Save is the default action');
    assert([...widgets(titleEntry)].some(widget => widget.tooltip_text === 'Get title from a window'), 'Title picker belongs to the text field');
    titleEntry.text = '';
    assert(!exceptionSave.sensitive, 'Empty condition disables Save');
    mode.selected = 4;
    titleEntry.text = '[';
    assert(!exceptionSave.sensitive && titleEntry.has_css_class('error'), 'Invalid regex disables Save and marks the field');
    mode.selected = 0;
    titleEntry.text = 'Docs';
    assert(exceptionSave.sensitive && !titleEntry.has_css_class('error'), 'Valid literal condition enables Save');
    exceptionCancel.emit('clicked');
    assert(prefs._titleDialog === null, 'Header Cancel closes the exception draft');
    await wait(250); // Let Shell finish removing the attached modal before closing its parent.
    const save = [...widgets(prefs.editor)].find(widget => widget instanceof Gtk.Button && widget.label === 'Save');
    assert(save, 'Rule editor has a Save action');
    assert(save.get_ancestor(Adw.HeaderBar.$gtype), 'Rule Save is in the header');
    assert(!save.get_ancestor(Gtk.ScrolledWindow.$gtype), 'Rule Save stays outside scrolling content');
    const cancel = [...widgets(prefs.editor)].find(widget => widget instanceof Gtk.Button && widget.label === 'Cancel');
    assert(cancel.get_ancestor(Adw.HeaderBar.$gtype), 'Rule Cancel is in the header');
    assert(prefs.editor.get_default_widget() === save, 'Rule Save is the default action');
    save.emit('clicked');
    assert(readRules(settings)[0].profile_id === 7, 'Saving the editor persists the rule');
    await wait(300);
    assert(prefs._ruleRows.length === 1, 'Rules rerender after settings change');
    assert(await evaluate('global.smoothScrollTestExtension._config.profile_rules[0].profile_id') === 7, 'Shell observes GSettings edit');
    const ruleSwitch = () => [...widgets(prefs._ruleRows[0])].find(widget => widget instanceof Gtk.Switch);
    ruleSwitch().active = false;
    assert(!readRules(settings)[0].enabled, 'Application list switch applies immediately');
    ruleSwitch().active = true;
    prefs._editRule(readRules(settings)[0], false);
    await wait(300); // Allow the editor to map before simulating Cancel.
    [...widgets(prefs.editor)].find(widget => widget instanceof Gtk.Switch).active = false;
    prefs.editor.close();
    assert(readRules(settings)[0].titles[0].enabled, 'Cancel discards exception toggle');
    await wait(250);
    const fixture = new Gtk.ApplicationWindow({application: app, title: 'Docs - Smooth Scroll Fixture', default_width: 500, default_height: 350});
    const label = new Gtk.Label({label: 'Window selection fixture'});
    fixture.set_child(label);
    let presses = 0;
    const gesture = new Gtk.GestureClick();
    gesture.connect('pressed', () => presses++);
    fixture.add_controller(gesture);
    fixture.present();
    await evaluate('Main.overview.hide(); true');
    await wait(500);
    window.present();
    await wait(150);
    const picking = prefs.pickWindow();
    await wait(600);
    assert(await evaluate('!!global.smoothScrollTestExtension._bridge._grab'), 'Picker captures input');
    const info = await evaluate(`(() => {
        const window = global.get_window_actors().map(a => a.meta_window).find(w => w.get_title() === 'Docs - Smooth Scroll Fixture');
        const rect = window.get_frame_rect();
        return {x: rect.x + rect.width / 2, y: rect.y + rect.height / 2,
            info: global.smoothScrollTestExtension._resolver.infoForWindow(window)};
    })()`);
    // Inject real compositor input, ensuring the target receives neither press nor release.
    await evaluate(`global.smoothScrollPointer = global.stage.context.get_backend().get_default_seat().create_virtual_device(global.smoothScrollTestClutter.InputDeviceType.POINTER_DEVICE); true`);
    await evaluate(`global.smoothScrollPointer.notify_absolute_motion(0, ${info.x}, ${info.y}); true`);
    await wait(100);
    await evaluate('global.smoothScrollPointer.notify_button(0, 1, global.smoothScrollTestClutter.ButtonState.PRESSED); true');
    await wait(100);
    await evaluate('global.smoothScrollPointer.notify_button(0, 1, global.smoothScrollTestClutter.ButtonState.RELEASED); true');
    const picked = await picking;
    assert(picked?.app === rule.app, 'Picked desktop ID matches runtime ID');
    assert(picked.title === 'Docs - Smooth Scroll Fixture', 'Picked title');
    assert(presses === 0, 'Picker consumes click');
    assert(window.visible, 'Preferences restored after selection');
    assert(await evaluate('global.smoothScrollTestExtension._bridge._grab === null'), 'Grab released');
    assert(picked.name && !('name' in info.info) && !('icon' in info.info), 'Presentation fields are only collected for picking');
    const touchPick = prefs.pickWindow();
    await wait(600);
    await evaluate(`global.smoothScrollTouch = global.stage.context.get_backend().get_default_seat().create_virtual_device(global.smoothScrollTestClutter.InputDeviceType.TOUCHSCREEN_DEVICE); true`);
    await evaluate(`global.smoothScrollTouch.notify_touch_down(0, 0, ${info.x}, ${info.y}); true`);
    await wait(100);
    await evaluate('global.smoothScrollTouch.notify_touch_up(0, 0); true');
    assert((await touchPick)?.app === rule.app, 'Touchscreen tap selects the same window as a mouse click');
    // Exercise the nested title editor: no parent editor may obscure the target.
    prefs._editRule(readRules(settings)[0], false, picked);
    await wait(300);
    prefs._editTitle(prefs.editor, rule.titles[0], rule, picked, () => {});
    const titleDialog = prefs._titleDialog;
    await wait(300);
    const cancelled = prefs.pickWindow(titleDialog);
    await wait(500);
    assert(!window.visible && !prefs.editor.visible && !titleDialog.visible, 'All editor windows hide during selection');
    await evaluate(`global.smoothScrollKeyboard = global.stage.context.get_backend().get_default_seat().create_virtual_device(global.smoothScrollTestClutter.InputDeviceType.KEYBOARD_DEVICE); true`);
    await evaluate('global.smoothScrollKeyboard.notify_keyval(0, global.smoothScrollTestClutter.KEY_Escape, global.smoothScrollTestClutter.KeyState.PRESSED); true');
    await evaluate('global.smoothScrollKeyboard.notify_keyval(0, global.smoothScrollTestClutter.KEY_Escape, global.smoothScrollTestClutter.KeyState.RELEASED); true');
    assert(await cancelled === null, 'Escape resolves without creating a rule');
    assert(window.visible && prefs.editor.visible && titleDialog.visible, 'All preferences windows restored after cancel');
    await wait(300);
    titleDialog.close();
    await wait(250);
    prefs.editor.close();
    await wait(250);
    const disabled = prefs.pickWindow();
    await wait(500);
    await evaluate("Main.extensionManager.disableExtension('smooth-scroll@wayne6530'); true");
    assert(await disabled === null, 'Disabling extension finishes pending pick');
    assert(window.visible, 'Preferences restored when extension is disabled');
    await prefs._refreshStatus();
    assert(!prefs.addButton.sensitive, 'Picker unavailable when extension is disabled');
    assert(await evaluate('global.smoothScrollTestExtension._bridge === null'), 'Disable removes bridge');
    await evaluate("Main.extensionManager.enableExtension('smooth-scroll@wayne6530'); true");
    await wait(500);
    await prefs._refreshStatus();
    assert(prefs.addButton.sensitive, 'Bridge works after re-enable');
    assert(await evaluate('global.smoothScrollTestExtension._config.profile_rules[0].profile_id') === 7, 'Rule persists after re-enable');
    const closing = prefs.pickWindow();
    await wait(500);
    window.close();
    assert(await closing === null, 'Closing preferences cancels the picker');
    await wait(100);
    assert(await evaluate('global.smoothScrollTestExtension._bridge._grab === null'), 'Closing preferences releases input');
    fixture.destroy();
    // Finally load prefs.js through GNOME's actual extension preferences host.
    const command = Gio.Subprocess.new(['gnome-extensions', 'prefs', 'smooth-scroll@wayne6530'], Gio.SubprocessFlags.NONE);
    await new Promise((resolve, reject) => command.wait_check_async(null, (proc, result) => {
        try { proc.wait_check_finish(result); resolve(); } catch (error) { reject(error); }
    }));
    let hosted = false;
    for (let attempt = 0; attempt < 20; attempt++) {
        hosted = await evaluate("global.get_window_actors().some(a => a.meta_window.get_title() === 'Smooth Scroll')");
        if (hosted) break;
        await wait(250);
    }
    assert(hosted, 'GNOME preferences host loads prefs.js and its schema');
    await evaluate("global.get_window_actors().filter(a => a.meta_window.get_title() === 'Smooth Scroll').forEach(a => a.meta_window.delete(global.get_current_time())); true");
    print('GTK pages, rule editors, live GSettings, actual Shell window picking, click capture, nested editors, Escape, close, disable/re-enable and GNOME preferences hosting passed.');
}
run().catch(error => { logError(error); failure = true; }).finally(() => loop.quit());
loop.run();
GLib.source_remove(deadline);
System.exit(failure ? 1 : 0);
