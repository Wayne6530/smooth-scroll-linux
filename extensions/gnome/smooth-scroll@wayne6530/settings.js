// Shared metadata for the runtime and preferences. Tests check schema parity.
export const SETTING_GROUPS = {
    dot: 'Inertial scroll dot',
    free_spin: 'Free Spin ring',
    arrow: 'Drag View arrow',
    auto_scroll: 'Auto Scroll marker',
    passthrough: 'Compatibility indicator',
    scroll: 'Window behavior',
    pointer_fallback: 'Pointer tracking',
};

export const SETTINGS = [
    {
        group: 'dot', field: 'enabled', title: 'Enabled',
        type: 'b', default: true,
    },
    {
        group: 'dot', field: 'offset_x', title: 'Horizontal offset (px)',
        type: 'd', default: 12, min: -256.0, max: 256.0, step: 1,
    },
    {
        group: 'dot', field: 'offset_y', title: 'Vertical offset (px)',
        type: 'd', default: 0, min: -256.0, max: 256.0, step: 1,
    },
    {
        group: 'dot', field: 'size', title: 'Size (px)',
        type: 'd', default: 8, min: 1.0, max: 64.0, step: 1,
    },
    {
        group: 'dot', field: 'color', title: 'Color',
        type: 's', default: '#4ea1ff',
    },
    {
        group: 'dot', field: 'min_alpha', title: 'Minimum opacity',
        type: 'd', default: 0.16, min: 0.0, max: 1.0, step: 0.01,
    },
    {
        group: 'dot', field: 'min_alpha_speed', title: 'Speed at minimum opacity',
        type: 'd', default: 100, min: 0.0, max: 65535.0, step: 1,
    },
    {
        group: 'dot', field: 'max_alpha', title: 'Maximum opacity',
        type: 'd', default: 0.9, min: 0.0, max: 1.0, step: 0.01,
    },
    {
        group: 'dot', field: 'max_alpha_speed', title: 'Speed at maximum opacity',
        type: 'd', default: 3200, min: 0.0, max: 65535.0, step: 1,
    },
    {
        group: 'free_spin', field: 'enabled', title: 'Enabled',
        type: 'b', default: true,
    },
    {
        group: 'free_spin', field: 'color', title: 'Color',
        type: 's', default: '#4ea1ff',
    },
    {
        group: 'free_spin', field: 'alpha', title: 'Opacity',
        type: 'd', default: 0.9, min: 0.0, max: 1.0, step: 0.01,
    },
    {
        group: 'free_spin', field: 'ring_gap', title: 'Ring gap (px)',
        type: 'd', default: 1, min: 0.0, max: 32.0, step: 1,
    },
    {
        group: 'free_spin', field: 'ring_width', title: 'Ring width (px)',
        type: 'd', default: 2, min: 1.0, max: 16.0, step: 1,
    },
    {
        group: 'arrow', field: 'enabled', title: 'Enabled',
        type: 'b', default: true,
    },
    {
        group: 'arrow', field: 'offset_x', title: 'Horizontal offset (px)',
        type: 'd', default: 12, min: -256.0, max: 256.0, step: 1,
    },
    {
        group: 'arrow', field: 'offset_y', title: 'Vertical offset (px)',
        type: 'd', default: 0, min: -256.0, max: 256.0, step: 1,
    },
    {
        group: 'arrow', field: 'size', title: 'Size (px)',
        type: 'd', default: 22, min: 4.0, max: 256.0, step: 1,
    },
    {
        group: 'arrow', field: 'color', title: 'Color',
        type: 's', default: '#4ea1ff',
    },
    {
        group: 'arrow', field: 'alpha', title: 'Opacity',
        type: 'd', default: 0.9, min: 0.0, max: 1.0, step: 0.01,
    },
    {
        group: 'arrow', field: 'padding_scale', title: 'Padding scale',
        type: 'd', default: 0.12, min: 0.0, max: 0.45, step: 0.01,
    },
    {
        group: 'arrow', field: 'min_padding', title: 'Min padding',
        type: 'd', default: 2, min: 0.0, max: 64.0, step: 1,
    },
    {
        group: 'arrow', field: 'head_size_scale', title: 'Head size scale',
        type: 'd', default: 0.18, min: 0.05, max: 0.45, step: 0.01,
    },
    {
        group: 'arrow', field: 'min_head_size', title: 'Min head size',
        type: 'd', default: 3, min: 1.0, max: 64.0, step: 1,
    },
    {
        group: 'arrow', field: 'line_width_scale', title: 'Line width scale',
        type: 'd', default: 0.08, min: 0.01, max: 0.4, step: 0.01,
    },
    {
        group: 'arrow', field: 'min_line_width', title: 'Min line width',
        type: 'd', default: 1, min: 1.0, max: 32.0, step: 1,
    },
    {
        group: 'arrow', field: 'head_width_scale', title: 'Head width scale',
        type: 'd', default: 0.5, min: 0.1, max: 1.5, step: 0.01,
    },
    {
        group: 'auto_scroll', field: 'enabled', title: 'Enabled',
        type: 'b', default: true,
    },
    {
        group: 'auto_scroll', field: 'offset_x', title: 'Horizontal offset (px)',
        type: 'd', default: 12, min: -256.0, max: 256.0, step: 1,
    },
    {
        group: 'auto_scroll', field: 'offset_y', title: 'Vertical offset (px)',
        type: 'd', default: 0, min: -256.0, max: 256.0, step: 1,
    },
    {
        group: 'auto_scroll', field: 'size', title: 'Size (px)',
        type: 'd', default: 24, min: 20.0, max: 256.0, step: 1,
    },
    {
        group: 'auto_scroll', field: 'color', title: 'Color',
        type: 's', default: '#cbd5e1',
    },
    {
        group: 'auto_scroll', field: 'dot_color', title: 'Moving dot color',
        type: 's', default: '#4ea1ff',
    },
    {
        group: 'auto_scroll', field: 'alpha', title: 'Opacity',
        type: 'd', default: 0.95, min: 0.0, max: 1.0, step: 0.01,
    },
    {
        group: 'auto_scroll', field: 'dot_size', title: 'Moving dot size (px)',
        type: 'd', default: 6, min: 2.0, max: 64.0, step: 1,
    },
    {
        group: 'passthrough', field: 'enabled', title: 'Enabled',
        type: 'b', default: true,
    },
    {
        group: 'passthrough', field: 'offset_x', title: 'Horizontal offset (px)',
        type: 'd', default: 12, min: -256.0, max: 256.0, step: 1,
    },
    {
        group: 'passthrough', field: 'offset_y', title: 'Vertical offset (px)',
        type: 'd', default: 0, min: -256.0, max: 256.0, step: 1,
    },
    {
        group: 'passthrough', field: 'size', title: 'Size (px)',
        type: 'd', default: 12, min: 4.0, max: 256.0, step: 1,
    },
    {
        group: 'passthrough', field: 'padding_scale', title: 'Padding scale',
        type: 'd', default: 0.22, min: 0.0, max: 0.45, step: 0.01,
    },
    {
        group: 'passthrough', field: 'min_padding', title: 'Min padding',
        type: 'd', default: 3, min: 0.0, max: 64.0, step: 1,
    },
    {
        group: 'passthrough', field: 'line_width_scale', title: 'Line width scale',
        type: 'd', default: 0.12, min: 0.01, max: 0.5, step: 0.01,
    },
    {
        group: 'passthrough', field: 'min_line_width', title: 'Min line width',
        type: 'd', default: 2, min: 1.0, max: 32.0, step: 1,
    },
    {
        group: 'passthrough', field: 'color', title: 'Color',
        type: 's', default: '#ff5c5c',
    },
    {
        group: 'passthrough', field: 'alpha', title: 'Opacity',
        type: 'd', default: 0.9, min: 0.0, max: 1.0, step: 0.01,
    },
    {
        group: 'scroll', field: 'stop_on_pointer_leave_window', title: 'Stop inertia when leaving the window',
        type: 'b', default: true,
    },
    {
        group: 'scroll', field: 'poll_interval_ms', title: 'IPC poll interval (ms)',
        type: 'd', default: 8, min: 8.0, max: 250.0, step: 1,
    },
    {
        group: 'pointer_fallback', field: 'enabled', title: 'Enabled',
        type: 'b', default: true,
    },
    {
        group: 'pointer_fallback', field: 'interval_ms', title: 'Pointer sampling interval (ms)',
        type: 'd', default: 4, min: 1.0, max: 250.0, step: 1,
    },
    {
        group: 'pointer_fallback', field: 'event_fresh_ms', title: 'Pointer event freshness (ms)',
        type: 'd', default: 6, min: 0.0, max: 1000.0, step: 1,
    },
].map(entry => ({...entry, key: `${entry.group}-${entry.field}`.replaceAll('_', '-')}));

export const DEFAULT_CONFIG = {profile_rules: [], default_profile_id: 0, shell_profile_id: 1};
for (const entry of SETTINGS) {
    DEFAULT_CONFIG[entry.group] ??= {};
    DEFAULT_CONFIG[entry.group][entry.field] = entry.default;
}
