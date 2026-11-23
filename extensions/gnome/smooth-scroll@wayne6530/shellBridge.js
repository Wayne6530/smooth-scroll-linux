import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import Shell from 'gi://Shell';
import St from 'gi://St';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import {BUS_PATH, BUS_XML} from './bridgeProtocol.js';

// Transient requests only: persisted preferences belong exclusively to GSettings.
export class ShellBridge {
    constructor(resolver, getStatus) {
        this._resolver = resolver;
        this._getStatus = getStatus;
        this._invocation = null;
        this._actor = null;
        this._grab = null;
        this._timeout = 0;
        this._ownerWatch = 0;
        this._pressed = false;
        this._touchSlot = null;
        this._dbus = Gio.DBusExportedObject.wrapJSObject(BUS_XML, this);
        this._dbus.export(Gio.DBus.session, BUS_PATH);
        this._sessionId = Main.sessionMode.connect('updated', () => {
            if (Main.sessionMode.isLocked || Main.sessionMode.isGreeter)
                this._finish({error: 'Window selection was cancelled.'});
        });
    }

    GetStatus() {
        const state = this._getStatus();
        return JSON.stringify({
            available: state.valid, connected: state.connected,
            requested: state.requestedProfileId, active: state.activeProfileId,
        });
    }

    PickWindowAsync(_params, invocation) {
        if (this._invocation || Main.sessionMode.isLocked || Main.sessionMode.isGreeter) {
            invocation.return_value(new GLib.Variant('(s)', [JSON.stringify({
                error: 'Window selection is unavailable. Finish the current selection first.',
            })]));
            return;
        }
        this._invocation = invocation;
        this._ownerWatch = Gio.bus_watch_name_on_connection(
            Gio.DBus.session, invocation.get_sender(), Gio.BusNameWatcherFlags.NONE,
            null, () => this._finish({error: 'Preferences closed.'}));
        try {
            const monitor = Main.layoutManager.primaryMonitor;
            this._actor = new St.Label({
                text: 'Select an application window · Esc to cancel',
                reactive: true, can_focus: true,
                style: 'background-color: #242424; color: white; padding: 16px; border-radius: 12px;',
            });
            Main.layoutManager.uiGroup.add_child(this._actor);
            this._actor.set_position(monitor.x + 24, monitor.y + 48);
            this._actor.connect('event', (_actor, event) => this._event(event));
            this._grab = Main.pushModal(this._actor, {actionMode: Shell.ActionMode.NONE});
            // Older Shell versions expose seat-state checks (including X11).
            // Newer Wayland-only Clutter grabs no longer provide this method.
            if (!this._grab || (this._grab.get_seat_state &&
                this._grab.get_seat_state() !== Clutter.GrabState.ALL)) {
                this._finish({error: 'Unable to capture input for window selection.'});
                return;
            }
            this._timeout = GLib.timeout_add(GLib.PRIORITY_DEFAULT, 30000, () => {
                this._timeout = 0;
                this._finish({error: 'Window selection timed out. Try again.'});
                return GLib.SOURCE_REMOVE;
            });
        } catch (error) {
            this._finish({error: error.message});
        }
    }

    CancelPickAsync(_params, invocation) {
        // One preferences process must not cancel another process's selection.
        if (this._invocation?.get_sender() === invocation.get_sender())
            this._finish({cancelled: true});
        invocation.return_value(null);
    }

    _event(event) {
        try {
            const type = event.type();
            if (![Clutter.EventType.KEY_PRESS, Clutter.EventType.KEY_RELEASE,
                Clutter.EventType.BUTTON_PRESS, Clutter.EventType.BUTTON_RELEASE,
                Clutter.EventType.MOTION, Clutter.EventType.SCROLL,
                Clutter.EventType.TOUCH_BEGIN, Clutter.EventType.TOUCH_UPDATE,
                Clutter.EventType.TOUCH_END, Clutter.EventType.TOUCH_CANCEL].includes(type))
                return Clutter.EVENT_PROPAGATE;
            if (type === Clutter.EventType.KEY_PRESS && event.get_key_symbol() === Clutter.KEY_Escape) {
                this._finish({cancelled: true});
            } else if (type === Clutter.EventType.TOUCH_BEGIN) {
                this._touchSlot ??= event.get_event_sequence().get_slot();
            } else if (type === Clutter.EventType.TOUCH_END || type === Clutter.EventType.TOUCH_CANCEL) {
                if (this._touchSlot === event.get_event_sequence().get_slot()) {
                    if (type === Clutter.EventType.TOUCH_CANCEL)
                        this._finish({cancelled: true});
                    else
                        this._selectAt(...event.get_coords());
                }
            } else if (this._touchSlot === null && type === Clutter.EventType.BUTTON_PRESS) {
                this._pressed = event.get_button() === 1;
            } else if (this._touchSlot === null && type === Clutter.EventType.BUTTON_RELEASE &&
                this._pressed && event.get_button() === 1) {
                this._selectAt(...event.get_coords());
            }
        } catch (error) {
            this._finish({error: error.message});
        }
        return Clutter.EVENT_STOP;
    }

    _selectAt(x, y) {
        const window = this._resolver.shellInteractionActive(x, y)
            ? null : this._resolver.windowAt(x, y);
        const info = this._resolver.infoForWindow(window, true);
        this._finish(info?.app ? info : {
            error: 'This window has no persistent application identity. Select a regular application window.',
        });
    }

    _finish(result) {
        if (this._timeout)
            GLib.source_remove(this._timeout);
        this._timeout = 0;
        if (this._ownerWatch)
            Gio.bus_unwatch_name(this._ownerWatch);
        this._ownerWatch = 0;
        if (this._grab)
            Main.popModal(this._grab);
        this._grab = null;
        this._actor?.destroy();
        this._actor = null;
        this._pressed = false;
        this._touchSlot = null;
        const invocation = this._invocation;
        this._invocation = null;
        invocation?.return_value(new GLib.Variant('(s)', [JSON.stringify(result)]));
    }

    destroy() {
        this._finish({error: 'The extension was disabled.'});
        Main.sessionMode.disconnect(this._sessionId);
        this._dbus.unexport();
        this._dbus = null;
    }
}
