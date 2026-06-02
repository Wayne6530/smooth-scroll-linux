import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import Meta from 'gi://Meta';
import Shell from 'gi://Shell';
import St from 'gi://St';

import { Extension } from 'resource:///org/gnome/shell/extensions/extension.js';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';

const IPC_PATH = '/dev/shm/smooth_scroll_shm';
const IPC_SIZE = 32;
const IPC_MAGIC = 0x53530001;
const IPC_OFFSET_MAGIC = 0x00;
const IPC_OFFSET_DAEMON_PID = 0x04;
const IPC_OFFSET_STATE_BITS = 0x08;
const IPC_OFFSET_SCROLL_ID = 0x0c;
const IPC_OFFSET_FORCE_PASSTHROUGH = 0x10;

const CONFIG_DIR_NAME = 'smooth-scroll-gnome-extension';
const CONFIG_FILE_NAME = 'config.json';

const DEFAULT_CONFIG = {
    dot: {
        enabled: true,
        offset_x: 12,
        offset_y: 0,
        size: 8,
        color: '#4ea1ff',
        min_alpha: 0.16,
        min_alpha_speed: 100,
        max_alpha: 0.9,
        max_alpha_speed: 3200,
    },
    arrow: {
        enabled: true,
        offset_x: 12,
        offset_y: 0,
        size: 22,
        color: '#4ea1ff',
        alpha: 0.9,
        padding_scale: 0.12,
        min_padding: 2,
        head_size_scale: 0.18,
        min_head_size: 3,
        line_width_scale: 0.08,
        min_line_width: 1,
        head_width_scale: 0.5,
    },
    passthrough: {
        enabled: true,
        offset_x: 12,
        offset_y: 0,
        size: 12,
        padding_scale: 0.22,
        min_padding: 3,
        line_width_scale: 0.12,
        min_line_width: 2,
        color: '#ff5c5c',
        alpha: 0.9,
    },
    scroll: {
        stop_on_pointer_leave_window: true,
        poll_interval_ms: 8,
    },
    pointer_fallback: {
        enabled: true,
        interval_ms: 4,
        event_fresh_ms: 6,
    },
    force_passthrough_rules: [],
};

function clamp(value, min, max) {
    return Math.max(min, Math.min(max, value));
}

function boolValue(value, fallback) {
    return typeof value === 'boolean' ? value : fallback;
}

function numberValue(value, fallback, min, max) {
    if (typeof value !== 'number' || Number.isNaN(value))
        return fallback;

    return clamp(value, min, max);
}

function cloneDefaultConfig() {
    return JSON.parse(JSON.stringify(DEFAULT_CONFIG));
}

function isPlainObject(value) {
    return value && typeof value === 'object' && !Array.isArray(value);
}

function parseColor(color) {
    if (typeof color !== 'string')
        return { r: 0.31, g: 0.63, b: 1.0 };

    const longHex = color.match(/^#([0-9a-fA-F]{6})$/);
    if (longHex) {
        const value = parseInt(longHex[1], 16);
        return {
            r: ((value >> 16) & 0xff) / 255,
            g: ((value >> 8) & 0xff) / 255,
            b: (value & 0xff) / 255,
        };
    }

    const shortHex = color.match(/^#([0-9a-fA-F]{3})$/);
    if (shortHex) {
        const r = parseInt(shortHex[1][0] + shortHex[1][0], 16);
        const g = parseInt(shortHex[1][1] + shortHex[1][1], 16);
        const b = parseInt(shortHex[1][2] + shortHex[1][2], 16);
        return { r: r / 255, g: g / 255, b: b / 255 };
    }

    return { r: 0.31, g: 0.63, b: 1.0 };
}

function decodeBytes(contents) {
    return new TextDecoder('utf-8').decode(contents);
}

function readFileContents(file) {
    const result = file.load_contents(null);
    if (typeof result[0] === 'boolean')
        return result[0] ? result[1] : null;

    return result[0];
}

function viewForBytes(contents) {
    return new DataView(contents.buffer, contents.byteOffset, contents.byteLength);
}

function monotonicMillis() {
    return GLib.get_monotonic_time() / 1000;
}

const POINTER_CAPTURED_EVENT_TYPES = [
    Clutter.EventType.MOTION,
    Clutter.EventType.BUTTON_PRESS,
    Clutter.EventType.BUTTON_RELEASE,
    Clutter.EventType.SCROLL,
].filter(type => type !== undefined);

function eventType(event) {
    try {
        if (typeof event?.type === 'function')
            return event.type();

        if (typeof event?.get_type === 'function')
            return event.get_type();
    } catch (error) {
        return null;
    }

    return null;
}

function isPointerCapturedEvent(event) {
    return POINTER_CAPTURED_EVENT_TYPES.includes(eventType(event));
}

function compileRegex(pattern) {
    if (typeof pattern !== 'string' || pattern === '')
        return undefined;

    try {
        return new RegExp(pattern);
    } catch (error) {
        console.warn(`Smooth Scroll IPC Companion: invalid regex "${pattern}": ${error.message}`);
        return null;
    }
}

function normalizeConfig(config) {
    const fallback = DEFAULT_CONFIG;

    if (!isPlainObject(config))
        config = cloneDefaultConfig();

    if (!isPlainObject(config.dot))
        config.dot = {};
    if (!isPlainObject(config.arrow))
        config.arrow = {};
    if (!isPlainObject(config.passthrough))
        config.passthrough = {};
    if (!isPlainObject(config.scroll))
        config.scroll = {};
    if (!isPlainObject(config.pointer_fallback))
        config.pointer_fallback = {};
    config.dot.enabled = boolValue(config.dot.enabled, fallback.dot.enabled);
    config.dot.offset_x = numberValue(config.dot.offset_x, fallback.dot.offset_x, -256, 256);
    config.dot.offset_y = numberValue(config.dot.offset_y, fallback.dot.offset_y, -256, 256);
    config.dot.size = numberValue(config.dot.size, fallback.dot.size, 1, 64);
    config.dot.color = typeof config.dot.color === 'string' && config.dot.color !== ''
        ? config.dot.color
        : fallback.dot.color;
    config.dot.min_alpha = numberValue(config.dot.min_alpha, fallback.dot.min_alpha, 0, 1);
    config.dot.max_alpha = numberValue(config.dot.max_alpha, fallback.dot.max_alpha, 0, 1);
    config.dot.min_alpha_speed = numberValue(config.dot.min_alpha_speed, fallback.dot.min_alpha_speed, 0, 65535);
    config.dot.max_alpha_speed = numberValue(config.dot.max_alpha_speed, fallback.dot.max_alpha_speed, 0, 65535);

    config.arrow.enabled = boolValue(config.arrow.enabled, fallback.arrow.enabled);
    config.arrow.offset_x = numberValue(config.arrow.offset_x, fallback.arrow.offset_x, -256, 256);
    config.arrow.offset_y = numberValue(config.arrow.offset_y, fallback.arrow.offset_y, -256, 256);
    config.arrow.size = Math.round(numberValue(config.arrow.size, fallback.arrow.size, 4, 256));
    config.arrow.color = typeof config.arrow.color === 'string' && config.arrow.color !== ''
        ? config.arrow.color
        : fallback.arrow.color;
    config.arrow.alpha = numberValue(config.arrow.alpha, fallback.arrow.alpha, 0, 1);
    config.arrow.padding_scale = numberValue(config.arrow.padding_scale, fallback.arrow.padding_scale, 0, 0.45);
    config.arrow.min_padding = Math.round(numberValue(config.arrow.min_padding, fallback.arrow.min_padding, 0, 64));
    config.arrow.head_size_scale = numberValue(config.arrow.head_size_scale, fallback.arrow.head_size_scale, 0.05, 0.45);
    config.arrow.min_head_size = Math.round(numberValue(config.arrow.min_head_size, fallback.arrow.min_head_size, 1, 64));
    config.arrow.line_width_scale = numberValue(config.arrow.line_width_scale, fallback.arrow.line_width_scale, 0.01, 0.4);
    config.arrow.min_line_width = Math.round(numberValue(config.arrow.min_line_width, fallback.arrow.min_line_width, 1, 32));
    config.arrow.head_width_scale = numberValue(config.arrow.head_width_scale, fallback.arrow.head_width_scale, 0.1, 1.5);

    config.passthrough.enabled = boolValue(config.passthrough.enabled, fallback.passthrough.enabled);
    config.passthrough.offset_x = numberValue(config.passthrough.offset_x, fallback.passthrough.offset_x, -256, 256);
    config.passthrough.offset_y = numberValue(config.passthrough.offset_y, fallback.passthrough.offset_y, -256, 256);
    config.passthrough.size = Math.round(numberValue(config.passthrough.size, fallback.passthrough.size, 4, 256));
    config.passthrough.padding_scale = numberValue(config.passthrough.padding_scale, fallback.passthrough.padding_scale, 0, 0.45);
    config.passthrough.min_padding = Math.round(numberValue(config.passthrough.min_padding, fallback.passthrough.min_padding, 0, 64));
    config.passthrough.line_width_scale = numberValue(config.passthrough.line_width_scale, fallback.passthrough.line_width_scale, 0.01, 0.5);
    config.passthrough.min_line_width = Math.round(numberValue(config.passthrough.min_line_width, fallback.passthrough.min_line_width, 1, 32));
    config.passthrough.color = typeof config.passthrough.color === 'string' && config.passthrough.color !== ''
        ? config.passthrough.color
        : fallback.passthrough.color;
    config.passthrough.alpha = numberValue(config.passthrough.alpha, fallback.passthrough.alpha, 0, 1);

    config.scroll.stop_on_pointer_leave_window = boolValue(
        config.scroll.stop_on_pointer_leave_window,
        fallback.scroll.stop_on_pointer_leave_window);
    config.scroll.poll_interval_ms = Math.round(numberValue(
        config.scroll.poll_interval_ms,
        fallback.scroll.poll_interval_ms,
        8,
        250));

    config.pointer_fallback.enabled = boolValue(
        config.pointer_fallback.enabled,
        fallback.pointer_fallback.enabled);
    config.pointer_fallback.interval_ms = Math.round(numberValue(
        config.pointer_fallback.interval_ms,
        fallback.pointer_fallback.interval_ms,
        1,
        250));
    config.pointer_fallback.event_fresh_ms = Math.round(numberValue(
        config.pointer_fallback.event_fresh_ms,
        fallback.pointer_fallback.event_fresh_ms,
        0,
        1000));

    if (!Array.isArray(config.force_passthrough_rules))
        config.force_passthrough_rules = fallback.force_passthrough_rules;

    config.force_passthrough_rules = config.force_passthrough_rules
        .filter(rule => isPlainObject(rule) && typeof rule.app === 'string' && rule.app !== '')
        .map(rule => {
            rule.enabled = rule.enabled !== false;
            rule.force_passthrough = rule.force_passthrough !== false;

            if (!Array.isArray(rule.titles))
                rule.titles = [];

            rule.titles = rule.titles
                .filter(titleRule => (
                    isPlainObject(titleRule) &&
                    typeof titleRule.title === 'string' &&
                    titleRule.title !== ''
                ))
                .map(titleRule => {
                    titleRule.enabled = titleRule.enabled !== false;
                    titleRule.force_passthrough = titleRule.force_passthrough !== false;
                    titleRule._titleRegex = compileRegex(titleRule.title);
                    titleRule._invalidRegex = !titleRule._titleRegex;
                    return titleRule;
                });

            return rule;
        });

    return config;
}

function loadConfig(path) {
    const file = Gio.File.new_for_path(path);
    if (!file.query_exists(null))
        return normalizeConfig(cloneDefaultConfig());

    try {
        const contents = readFileContents(file);
        if (!contents)
            return normalizeConfig(cloneDefaultConfig());

        return normalizeConfig(JSON.parse(decodeBytes(contents)));
    } catch (error) {
        console.warn(`Smooth Scroll IPC Companion: failed to load config: ${error.message}`);
        return normalizeConfig(cloneDefaultConfig());
    }
}

class SmoothScrollIpcClient {
    constructor(path) {
        this._file = Gio.File.new_for_path(path);
        this._readStream = null;
        this._lastValid = false;
        this._lastPid = 0;
    }

    close() {
        this._closeReadStream();
    }

    readSnapshot() {
        try {
            const stream = this._ensureReadStream();
            if (!stream)
                return this._invalidSnapshot();

            if (!stream.seek(0, GLib.SeekType.SET, null))
                return this._invalidSnapshot();

            const bytes = stream.read_bytes(IPC_SIZE, null);
            const contents = bytes.get_data();
            if (!contents || contents.byteLength < IPC_SIZE)
                return this._invalidSnapshot();

            const view = viewForBytes(contents);
            const magic = view.getUint32(IPC_OFFSET_MAGIC, true);
            if (magic !== IPC_MAGIC)
                return this._invalidSnapshot();

            const pid = view.getUint32(IPC_OFFSET_DAEMON_PID, true);
            if (pid === 0)
                return this._invalidSnapshot();

            const stateBits = view.getUint32(IPC_OFFSET_STATE_BITS, true);
            const scrollId = view.getUint32(IPC_OFFSET_SCROLL_ID, true);
            const forcePassthrough = view.getUint32(IPC_OFFSET_FORCE_PASSTHROUGH, true);

            this._lastValid = true;
            this._lastPid = pid;

            return {
                valid: true,
                pid,
                connected: (stateBits & (1 << 0)) !== 0,
                passthrough: (stateBits & (1 << 1)) !== 0,
                dragView: (stateBits & (1 << 2)) !== 0,
                freeSpin: (stateBits & (1 << 3)) !== 0,
                horizontal: (stateBits & (1 << 4)) !== 0,
                positive: (stateBits & (1 << 5)) !== 0,
                speed: stateBits >>> 16,
                scrollId,
                forcePassthrough,
            };
        } catch (error) {
            this._closeReadStream();
            return this._invalidSnapshot();
        }
    }

    requestStop(snapshot = null) {
        const state = snapshot && snapshot.valid ? snapshot : this.readSnapshot();
        if (!state.valid)
            return false;

        return this._writeU32(IPC_OFFSET_SCROLL_ID, (state.scrollId + 1) >>> 0);
    }

    setForcePassthrough(enabled) {
        return this._writeU32(IPC_OFFSET_FORCE_PASSTHROUGH, enabled ? 1 : 0);
    }

    _invalidSnapshot() {
        this._lastValid = false;
        this._lastPid = 0;

        return {
            valid: false,
            pid: 0,
            connected: false,
            passthrough: false,
            dragView: false,
            freeSpin: false,
            horizontal: false,
            positive: false,
            speed: 0,
            scrollId: 0,
            forcePassthrough: 0,
        };
    }

    _ensureReadStream() {
        if (this._readStream)
            return this._readStream;

        if (!this._file.query_exists(null))
            return null;

        try {
            this._readStream = this._file.read(null);
            if (!this._readStream.can_seek()) {
                this._closeReadStream();
                return null;
            }
        } catch (error) {
            this._closeReadStream();
            return null;
        }

        return this._readStream;
    }

    _closeReadStream() {
        if (!this._readStream)
            return;

        try {
            this._readStream.close(null);
        } catch (error) {
            // Ignore close errors; reconnecting on the next poll is enough.
        }

        this._readStream = null;
    }

    _writeU32(offset, value) {
        let stream = null;

        try {
            if (!this._file.query_exists(null))
                return false;

            stream = this._file.open_readwrite(null);
            if (!stream.can_seek() || !stream.seek(offset, GLib.SeekType.SET, null))
                return false;

            const bytes = new Uint8Array(4);
            new DataView(bytes.buffer).setUint32(0, value >>> 0, true);

            const output = stream.get_output_stream();
            output.write_all(bytes, null);
            output.flush(null);

            return true;
        } catch (error) {
            return false;
        } finally {
            if (stream) {
                try {
                    stream.close(null);
                } catch (error) {
                    // Ignore close errors; the next poll will retry if needed.
                }
            }
        }
    }
}

class ConfigManager {
    constructor(path, onReload) {
        this.path = path;
        this.config = loadConfig(path);
        this._onReload = onReload;
        this._monitor = null;
        this._monitorChangedId = 0;
        this._reloadSourceId = 0;
    }

    start() {
        const file = Gio.File.new_for_path(this.path);
        let monitorFile = file;
        let monitorDirectory = false;

        if (!file.query_exists(null)) {
            const parent = file.get_parent();
            if (!parent || !parent.query_exists(null))
                return;

            monitorFile = parent;
            monitorDirectory = true;
        }

        try {
            this._monitor = monitorDirectory
                ? monitorFile.monitor_directory(Gio.FileMonitorFlags.NONE, null)
                : monitorFile.monitor_file(Gio.FileMonitorFlags.NONE, null);
            this._monitorChangedId = this._monitor.connect('changed', (_monitor, changedFile) => {
                if (monitorDirectory && changedFile.get_path() !== this.path)
                    return;

                this._scheduleReload();
            });
        } catch (error) {
            console.warn(`Smooth Scroll IPC Companion: failed to monitor config: ${error.message}`);
        }
    }

    stop() {
        if (this._reloadSourceId) {
            GLib.source_remove(this._reloadSourceId);
            this._reloadSourceId = 0;
        }

        if (this._monitorChangedId && this._monitor) {
            this._monitor.disconnect(this._monitorChangedId);
            this._monitorChangedId = 0;
        }

        if (this._monitor) {
            this._monitor.cancel();
            this._monitor = null;
        }
    }

    _scheduleReload() {
        if (this._reloadSourceId)
            GLib.source_remove(this._reloadSourceId);

        this._reloadSourceId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, 250, () => {
            this._reloadSourceId = 0;
            this.config = loadConfig(this.path);
            this._onReload(this.config);
            return GLib.SOURCE_REMOVE;
        });
    }
}

class PointerWindowResolver {
    constructor() {
        this._tracker = Shell.WindowTracker.get_default();
    }

    windowAt(x, y) {
        const windows = [];

        for (const actor of global.get_window_actors()) {
            const window = actor.meta_window || actor.get_meta_window?.();
            if (!window || !this._isPointerWindowCandidate(window, x, y))
                continue;

            windows.push(window);
        }

        if (windows.length === 0)
            return null;

        const sorted = global.display.sort_windows_by_stacking(windows);
        return sorted[sorted.length - 1] || null;
    }

    shellInteractionActive(x, y) {
        return this._overviewActive() || this._shellActorOnTop(x, y);
    }

    _overviewActive() {
        return Boolean(
            Main.overview?.visible ||
            Main.overview?.visibleTarget ||
            Main.overview?.animationInProgress
        );
    }

    _shellActorOnTop(x, y) {
        let actor = null;

        try {
            actor = global.stage.get_actor_at_pos(Clutter.PickMode.REACTIVE, x, y);
        } catch (error) {
            return false;
        }

        for (let current = actor; current; current = current.get_parent?.()) {
            const window = current.meta_window || current.get_meta_window?.();
            if (!window) {
                if (current === global.window_group)
                    return false;

                if (current === Main.layoutManager?.uiGroup)
                    return true;

                continue;
            }

            return this._isExcludedWindowType(window.get_window_type?.());
        }

        return false;
    }

    keyForWindow(window) {
        if (!window)
            return null;

        if (window.get_stable_sequence)
            return `stable:${window.get_stable_sequence()}`;

        if (window.get_id)
            return `id:${window.get_id()}`;

        return window.get_description?.() || null;
    }

    infoForWindow(window) {
        if (!window)
            return null;

        let app = null;
        try {
            app = this._tracker.get_window_app(window);
        } catch (error) {
            app = null;
        }

        const appIds = [
            app?.get_id?.(),
            window.get_gtk_application_id?.(),
            window.get_sandboxed_app_id?.(),
            window.get_startup_id?.(),
        ].filter(value => typeof value === 'string' && value !== '');

        return {
            appIds,
            title: window.get_title?.() || '',
            wmClass: window.get_wm_class?.() || '',
            wmClassInstance: window.get_wm_class_instance?.() || '',
        };
    }

    _isPointerWindowCandidate(window, x, y) {
        if (!this._isVisibleWindow(window))
            return false;

        if (this._isExcludedWindowType(window.get_window_type?.()))
            return false;

        return this._containsPointer(window, x, y);
    }

    _isExcludedWindowType(type) {
        const excludedTypes = [
            Meta.WindowType.DESKTOP,
            Meta.WindowType.DOCK,
            Meta.WindowType.MENU,
            Meta.WindowType.DROPDOWN_MENU,
            Meta.WindowType.POPUP_MENU,
            Meta.WindowType.TOOLTIP,
            Meta.WindowType.NOTIFICATION,
            Meta.WindowType.COMBO,
            Meta.WindowType.DND,
            Meta.WindowType.OVERRIDE_OTHER,
        ].filter(value => value !== undefined);

        return excludedTypes.includes(type);
    }

    _isVisibleWindow(window) {
        if (window.is_hidden?.())
            return false;

        if (window.showing_on_its_workspace && !window.showing_on_its_workspace())
            return false;

        return true;
    }

    _containsPointer(window, x, y) {
        const rect = window.get_frame_rect?.();
        if (!rect)
            return false;

        return x >= rect.x && x < rect.x + rect.width && y >= rect.y && y < rect.y + rect.height;
    }
}

class DotOverlay {
    constructor() {
        this._actor = new St.DrawingArea({
            visible: false,
            reactive: false,
            can_focus: false,
        });
        this._mode = 'dot';
        this._currentSize = DEFAULT_CONFIG.dot.size;
        this._lastX = null;
        this._lastY = null;
        this._dotColor = parseColor(DEFAULT_CONFIG.dot.color);
        this._arrowColor = parseColor(DEFAULT_CONFIG.arrow.color);
        this._passthroughColor = parseColor(DEFAULT_CONFIG.passthrough.color);
        this._repaintId = this._actor.connect('repaint', area => this._draw(area));

        Main.layoutManager.uiGroup.add_child(this._actor);
        this.applyConfig(DEFAULT_CONFIG);
    }

    destroy() {
        this.hide();
        if (this._repaintId) {
            this._actor.disconnect(this._repaintId);
            this._repaintId = 0;
        }
        this._actor.destroy();
        this._actor = null;
    }

    applyConfig(config) {
        if (!this._actor)
            return;

        this._config = config;
        this._dotColor = parseColor(config.dot.color);
        this._arrowColor = parseColor(config.arrow.color);
        this._passthroughColor = parseColor(config.passthrough.color);
        this._actor.queue_repaint();
    }

    update(snapshot, x, y, forcePassthroughActive = false) {
        if (!this._actor) {
            this.hide();
            return false;
        }

        const nextMode = this._modeForSnapshot(snapshot, forcePassthroughActive);
        if (!nextMode) {
            this.hide();
            return false;
        }

        const nextSize = this._visualSize(nextMode);
        const needsRepaint = !this._actor.visible || nextMode !== this._mode || nextSize !== this._currentSize;

        this._mode = nextMode;
        this._currentSize = nextSize;

        const alpha = this._alphaForMode(nextMode, snapshot);

        if (needsRepaint)
            this._actor.set_size(this._currentSize, this._currentSize);

        this._actor.opacity = Math.round(alpha * 255);
        this._setPointerPosition(x, y);
        this._raiseTop();

        if (needsRepaint)
            this._actor.queue_repaint();

        this._actor.show();
        return true;
    }

    moveTo(x, y) {
        if (!this.isVisible())
            return false;

        this._setPointerPosition(x, y);
        this._raiseTop();
        return true;
    }

    isVisible() {
        return Boolean(this._actor?.visible);
    }

    hide() {
        if (this._actor) {
            this._actor.hide();
            this._lastX = null;
            this._lastY = null;
        }
    }

    _raiseTop() {
        if (!this._actor)
            return;

        const parent = this._actor.get_parent?.();
        try {
            if (parent?.set_child_above_sibling)
                parent.set_child_above_sibling(this._actor, null);
            else if (this._actor.raise_top)
                this._actor.raise_top();
        } catch (error) {
            // Layering is best-effort; the indicator still works if a Shell
            // version does not expose a supported raise API.
        }
    }

    _setPointerPosition(x, y) {
        const size = this._currentSize;
        const stage = global.get_stage();
        const offset = this._offsetForMode(this._mode);
        const dotX = clamp(Math.round(x + offset.x - size / 2), 0, Math.max(0, stage.width - size));
        const dotY = clamp(Math.round(y + offset.y - size / 2), 0, Math.max(0, stage.height - size));

        if (dotX !== this._lastX || dotY !== this._lastY) {
            this._actor.set_position(dotX, dotY);
            this._lastX = dotX;
            this._lastY = dotY;
        }
    }

    _modeForSnapshot(snapshot, forcePassthroughActive) {
        if (forcePassthroughActive && this._config.passthrough.enabled)
            return 'passthrough';

        if (snapshot.dragView)
            return this._config.arrow.enabled ? 'arrow' : null;

        if (snapshot.speed > 0)
            return this._config.dot.enabled ? 'dot' : null;

        return null;
    }

    _alphaForMode(mode, snapshot) {
        if (mode === 'passthrough')
            return this._config.passthrough.alpha;

        if (mode === 'arrow')
            return this._config.arrow.alpha;

        return this._alphaForSpeed(snapshot.speed);
    }

    _offsetForMode(mode) {
        if (mode === 'passthrough') {
            return {
                x: this._config.passthrough.offset_x,
                y: this._config.passthrough.offset_y,
            };
        }

        if (mode === 'arrow') {
            return {
                x: this._config.arrow.offset_x,
                y: this._config.arrow.offset_y,
            };
        }

        return {
            x: this._config.dot.offset_x,
            y: this._config.dot.offset_y,
        };
    }

    _alphaForSpeed(speed) {
        const startSpeed = this._config.dot.min_alpha_speed;
        const endSpeed = this._config.dot.max_alpha_speed;
        const startAlpha = this._config.dot.min_alpha;
        const endAlpha = this._config.dot.max_alpha;

        if (startSpeed === endSpeed)
            return speed >= endSpeed ? endAlpha : startAlpha;

        const t = clamp((speed - startSpeed) / (endSpeed - startSpeed), 0, 1);
        return clamp(startAlpha + (endAlpha - startAlpha) * t, 0, 1);
    }

    _visualSize(mode) {
        if (mode === 'arrow')
            return this._config.arrow.size;

        if (mode === 'passthrough') {
            return this._config.passthrough.size;
        }

        return Math.round(this._config.dot.size);
    }

    _draw(area) {
        const cr = area.get_context();
        const size = this._currentSize;

        cr.setSourceRGBA(0, 0, 0, 0.28);
        if (this._mode === 'arrow')
            this._drawArrow(cr, size, 1);
        else if (this._mode === 'passthrough')
            this._drawPassthrough(cr, size, 1);
        else
            this._drawDot(cr, size, 1);

        const color = this._colorForMode(this._mode);
        cr.setSourceRGBA(color.r, color.g, color.b, 1);
        if (this._mode === 'arrow')
            this._drawArrow(cr, size, 0);
        else if (this._mode === 'passthrough')
            this._drawPassthrough(cr, size, 0);
        else
            this._drawDot(cr, size, 0);

        if (cr.$dispose)
            cr.$dispose();
    }

    _colorForMode(mode) {
        if (mode === 'passthrough')
            return this._passthroughColor;

        if (mode === 'arrow')
            return this._arrowColor;

        return this._dotColor;
    }

    _drawDot(cr, size, shadowOffset) {
        const radius = Math.max(1, (size - shadowOffset * 2) / 2);
        const center = size / 2 + shadowOffset;

        cr.arc(center, center, radius, 0, Math.PI * 2);
        cr.fill();
    }

    _drawArrow(cr, size, shadowOffset) {
        const arrow = this._config.arrow;
        const center = size / 2 + shadowOffset;
        const pad = Math.max(arrow.min_padding, Math.round(size * arrow.padding_scale));
        const head = Math.max(arrow.min_head_size, Math.round(size * arrow.head_size_scale));
        const lineWidth = Math.max(arrow.min_line_width, Math.round(size * arrow.line_width_scale));
        const start = pad + head;
        const end = size - pad - head;

        cr.setLineWidth(lineWidth);
        cr.moveTo(start + shadowOffset, center);
        cr.lineTo(end + shadowOffset, center);
        cr.moveTo(center, start + shadowOffset);
        cr.lineTo(center, end + shadowOffset);
        cr.stroke();

        this._fillTriangle(cr, center, pad + shadowOffset, head, arrow.head_width_scale, 'up');
        this._fillTriangle(cr, center, size - pad + shadowOffset, head, arrow.head_width_scale, 'down');
        this._fillTriangle(cr, pad + shadowOffset, center, head, arrow.head_width_scale, 'left');
        this._fillTriangle(cr, size - pad + shadowOffset, center, head, arrow.head_width_scale, 'right');
    }

    _fillTriangle(cr, x, y, head, widthScale, direction) {
        const half = head * widthScale;

        switch (direction) {
            case 'up':
                cr.moveTo(x, y);
                cr.lineTo(x - half, y + head);
                cr.lineTo(x + half, y + head);
                break;
            case 'down':
                cr.moveTo(x, y);
                cr.lineTo(x - half, y - head);
                cr.lineTo(x + half, y - head);
                break;
            case 'left':
                cr.moveTo(x, y);
                cr.lineTo(x + head, y - half);
                cr.lineTo(x + head, y + half);
                break;
            case 'right':
                cr.moveTo(x, y);
                cr.lineTo(x - head, y - half);
                cr.lineTo(x - head, y + half);
                break;
            default:
                return;
        }

        cr.closePath();
        cr.fill();
    }

    _drawPassthrough(cr, size, shadowOffset) {
        const passthrough = this._config.passthrough;
        const maxPad = Math.max(0, Math.floor((size - 2) / 2));
        const pad = Math.min(
            maxPad,
            Math.max(passthrough.min_padding, Math.round(size * passthrough.padding_scale))
        );
        const lineWidth = Math.max(
            passthrough.min_line_width,
            Math.round(size * passthrough.line_width_scale)
        );
        const start = pad + shadowOffset;
        const end = size - pad + shadowOffset;

        cr.setLineWidth(lineWidth);
        cr.moveTo(start, start);
        cr.lineTo(end, end);
        cr.moveTo(end, start);
        cr.lineTo(start, end);
        cr.stroke();
    }
}

function forcePassthroughForWindow(rules, info) {
    if (!info)
        return false;

    for (const rule of rules) {
        if (!rule.enabled || !info.appIds.includes(rule.app))
            continue;

        for (const titleRule of rule.titles) {
            if (!titleRule.enabled || titleRule._invalidRegex)
                continue;

            if (titleRule._titleRegex.test(info.title))
                return titleRule.force_passthrough;
        }

        return rule.force_passthrough;
    }

    return false;
}

export default class SmoothScrollIpcCompanionExtension extends Extension {
    enable() {
        this._configPath = GLib.build_filenamev([
            GLib.get_user_config_dir(),
            CONFIG_DIR_NAME,
            CONFIG_FILE_NAME,
        ]);

        this._ipc = new SmoothScrollIpcClient(IPC_PATH);
        this._resolver = new PointerWindowResolver();
        this._dot = new DotOverlay();
        this._anchorWindowKey = null;
        this._stopRequestedForAnchor = false;
        this._lastSpeed = 0;
        this._lastPid = 0;
        this._lastForcePassthrough = null;
        this._lastSnapshot = null;
        this._pendingPointerStateSourceId = 0;
        this._pendingPointerX = null;
        this._pendingPointerY = null;
        this._pointerFallbackSourceId = 0;
        this._lastPointerEventTimeMs = 0;
        this._unredirectDisabled = false;

        this._configManager = new ConfigManager(this._configPath, config => this._onConfigReload(config));
        this._configManager.start();
        this._config = this._configManager.config;
        this._dot.applyConfig(this._config);

        this._stage = global.get_stage();
        this._stageCapturedEventId = this._stage.connect(
            'captured-event',
            (_stage, event) => this._onStageCapturedEvent(event));

        this._startPoll();
    }

    disable() {
        if (this._stageCapturedEventId && this._stage) {
            this._stage.disconnect(this._stageCapturedEventId);
            this._stageCapturedEventId = 0;
            this._stage = null;
        }

        if (this._pollSourceId) {
            GLib.source_remove(this._pollSourceId);
            this._pollSourceId = 0;
        }

        if (this._pendingPointerStateSourceId) {
            GLib.source_remove(this._pendingPointerStateSourceId);
            this._pendingPointerStateSourceId = 0;
        }

        this._stopPointerFallback();

        if (this._configManager) {
            this._configManager.stop();
            this._configManager = null;
        }

        this._setUnredirectDisabled(false);

        if (this._ipc)
            this._ipc.setForcePassthrough(false);

        if (this._ipc)
            this._ipc.close();

        if (this._dot) {
            this._dot.destroy();
            this._dot = null;
        }

        this._config = null;
        this._ipc = null;
        this._resolver = null;
        this._anchorWindowKey = null;
        this._lastForcePassthrough = null;
        this._lastSnapshot = null;
        this._pendingPointerX = null;
        this._pendingPointerY = null;
        this._unredirectDisabled = false;
    }

    _onConfigReload(config) {
        const oldInterval = this._config?.scroll?.poll_interval_ms;

        this._config = config;
        this._dot.applyConfig(config);

        if (oldInterval !== config.scroll.poll_interval_ms)
            this._startPoll();

        this._restartPointerFallbackIfVisible();
    }

    _startPoll() {
        if (this._pollSourceId)
            GLib.source_remove(this._pollSourceId);

        this._pollSourceId = GLib.timeout_add(
            GLib.PRIORITY_DEFAULT,
            this._config?.scroll?.poll_interval_ms || DEFAULT_CONFIG.scroll.poll_interval_ms,
            () => {
                try {
                    this._tick();
                } catch (error) {
                    console.warn(`Smooth Scroll IPC Companion: poll failed: ${error.message}`);
                }

                return GLib.SOURCE_CONTINUE;
            });
    }

    _tick() {
        const snapshot = this._ipc.readSnapshot();
        this._lastSnapshot = snapshot;
        const [x, y] = global.get_pointer();

        if (!snapshot.valid) {
            this._resetScrollAnchor();
            this._setForcePassthrough(false);
            this._hideDot();
            this._lastSpeed = 0;
            this._lastPid = 0;
            return;
        }

        if (this._lastPid !== snapshot.pid) {
            this._resetScrollAnchor();
            this._lastForcePassthrough = null;
            this._lastPid = snapshot.pid;
        }

        this._handlePointerState(snapshot, x, y);

        this._lastSpeed = snapshot.speed;
    }

    _onStageCapturedEvent(event) {
        if (!this._lastSnapshot?.valid)
            return false;

        if (!isPointerCapturedEvent(event))
            return false;

        const [x, y] = global.get_pointer();
        this._lastPointerEventTimeMs = monotonicMillis();
        const snapshot = this._lastSnapshot;
        const forcePassthroughActive = this._isForcePassthroughActive(snapshot);

        if (!this._stopRequestedForAnchor || forcePassthroughActive)
            this._dot.moveTo(x, y);

        this._schedulePointerState(x, y);

        return false;
    }

    _schedulePointerState(x, y) {
        this._pendingPointerX = x;
        this._pendingPointerY = y;

        if (this._pendingPointerStateSourceId)
            return;

        this._pendingPointerStateSourceId = GLib.idle_add(GLib.PRIORITY_DEFAULT_IDLE, () => {
            this._pendingPointerStateSourceId = 0;

            if (!this._lastSnapshot?.valid || this._pendingPointerX === null || this._pendingPointerY === null)
                return GLib.SOURCE_REMOVE;

            const pendingX = this._pendingPointerX;
            const pendingY = this._pendingPointerY;
            this._pendingPointerX = null;
            this._pendingPointerY = null;

            this._handlePointerState(this._lastSnapshot, pendingX, pendingY, true);

            return GLib.SOURCE_REMOVE;
        });
    }

    _handlePointerState(snapshot, x, y, dotAlreadyUpdated = false) {
        const pointerWindow = this._resolver.windowAt(x, y);
        const pointerWindowInfo = this._resolver.infoForWindow(pointerWindow);
        const wasForcePassthroughActive = this._isForcePassthroughActive(snapshot);
        const forcePassthroughActive = this._updateForcePassthrough(pointerWindow, pointerWindowInfo, x, y);
        if (forcePassthroughActive)
            this._resetScrollAnchor();
        else
            this._updatePointerLeaveBrake(snapshot, pointerWindow);

        if ((!this._stopRequestedForAnchor || forcePassthroughActive) &&
            (!dotAlreadyUpdated || wasForcePassthroughActive !== forcePassthroughActive || !this._dot?.isVisible()))
            this._updateDot(snapshot, x, y, forcePassthroughActive);
    }

    _updateDot(snapshot, x, y, forcePassthroughActive) {
        const canShowIndicator = Boolean(
            (forcePassthroughActive && this._config?.passthrough?.enabled) ||
            (snapshot.dragView && this._config?.arrow?.enabled) ||
            (snapshot.speed > 0 && this._config?.dot?.enabled)
        );

        if (canShowIndicator)
            this._setUnredirectDisabled(true);

        let visible = false;
        try {
            visible = this._dot.update(snapshot, x, y, forcePassthroughActive);
        } finally {
            if (!visible || !canShowIndicator)
                this._setUnredirectDisabled(false);
        }

        this._setPointerFallbackActive(visible);
    }

    _hideDot() {
        this._setUnredirectDisabled(false);
        this._setPointerFallbackActive(false);
        this._dot?.hide();
    }

    _setUnredirectDisabled(disabled) {
        if (this._unredirectDisabled === disabled)
            return;

        const updateUnredirect = disabled
            ? Meta.disable_unredirect_for_display
            : Meta.enable_unredirect_for_display;

        if (typeof updateUnredirect !== 'function')
            return;

        try {
            updateUnredirect(global.display);
            this._unredirectDisabled = disabled;
        } catch (error) {
            console.warn(`Smooth Scroll IPC Companion: failed to update unredirect state: ${error.message}`);
        }
    }

    _setPointerFallbackActive(active) {
        if (active && this._config?.pointer_fallback?.enabled)
            this._startPointerFallback();
        else
            this._stopPointerFallback();
    }

    _startPointerFallback() {
        if (this._pointerFallbackSourceId)
            return;

        this._pointerFallbackSourceId = GLib.timeout_add(
            GLib.PRIORITY_DEFAULT,
            this._config?.pointer_fallback?.interval_ms || DEFAULT_CONFIG.pointer_fallback.interval_ms,
            () => {
                if (!this._dot?.isVisible()) {
                    this._pointerFallbackSourceId = 0;
                    return GLib.SOURCE_REMOVE;
                }

                const eventFreshMs = this._config?.pointer_fallback?.event_fresh_ms ??
                    DEFAULT_CONFIG.pointer_fallback.event_fresh_ms;
                if (monotonicMillis() - this._lastPointerEventTimeMs >= eventFreshMs) {
                    const [x, y] = global.get_pointer();
                    this._dot.moveTo(x, y);
                }

                return GLib.SOURCE_CONTINUE;
            });
    }

    _stopPointerFallback() {
        if (!this._pointerFallbackSourceId)
            return;

        GLib.source_remove(this._pointerFallbackSourceId);
        this._pointerFallbackSourceId = 0;
    }

    _restartPointerFallbackIfVisible() {
        const wasActive = this._pointerFallbackSourceId !== 0;
        this._stopPointerFallback();

        if ((wasActive || this._dot?.isVisible()) && this._config?.pointer_fallback?.enabled)
            this._startPointerFallback();
    }

    _updatePointerLeaveBrake(snapshot, pointerWindow) {
        if (!this._config.scroll.stop_on_pointer_leave_window || snapshot.speed <= 0) {
            if (snapshot.speed <= 0)
                this._resetScrollAnchor();
            return;
        }

        const pointerWindowKey = this._resolver.keyForWindow(pointerWindow);

        if (this._lastSpeed === 0) {
            this._anchorWindowKey = pointerWindowKey;
            this._stopRequestedForAnchor = false;

            if (!this._anchorWindowKey)
                this._requestAnchorStop(snapshot);
            return;
        }

        if (!this._anchorWindowKey) {
            this._requestAnchorStop(snapshot);
            return;
        }

        if (pointerWindowKey !== this._anchorWindowKey)
            this._requestAnchorStop(snapshot);
    }

    _requestAnchorStop(snapshot) {
        if (this._stopRequestedForAnchor)
            return;

        this._ipc.requestStop(snapshot);
        this._stopRequestedForAnchor = true;
        this._hideDot();
    }

    _resetScrollAnchor() {
        this._anchorWindowKey = null;
        this._stopRequestedForAnchor = false;
    }

    _shouldForcePassthrough(pointerWindow, pointerWindowInfo, x, y) {
        if (this._resolver.shellInteractionActive(x, y))
            return true;

        if (!pointerWindow)
            return true;

        return forcePassthroughForWindow(this._config.force_passthrough_rules, pointerWindowInfo);
    }

    _updateForcePassthrough(pointerWindow, pointerWindowInfo, x, y) {
        return this._setForcePassthrough(
            this._shouldForcePassthrough(pointerWindow, pointerWindowInfo, x, y)
        );
    }

    _isForcePassthroughActive(snapshot) {
        return Boolean(
            (this._lastForcePassthrough === true || snapshot.forcePassthrough !== 0)
        );
    }

    _setForcePassthrough(enabled) {
        if (this._lastForcePassthrough === enabled)
            return this._lastForcePassthrough === true;

        if (this._ipc.setForcePassthrough(enabled))
            this._lastForcePassthrough = enabled;

        return this._lastForcePassthrough === true;
    }
}
