/** Editable configuration; profile values and preset baselines never contain device options. */
const Profiles = (() => {
  const globalKeys = ConfigPresets.globalKeys;
  let items = [];
  let selected = 0;
  let globals = {};
  const copy = value => structuredClone(value);
  const defaults = () => ProfilePresets.defaults();
  function init(config) {
    selected = 0;
    globals = copy(config.globals);
    items = config.profiles.map(item => ({ ...copy(item), values: ProfilePresets.pick(item.values), baseline: ProfilePresets.pick(item.values) }));
  }
  function active() { return items[selected]; }
  function update(values, editorState) {
    if (selected !== 1) {
      active().values = ProfilePresets.pick(values);
      if (editorState !== undefined) active().editorState = copy(editorState);
    }
    for (const key of globalKeys) if (key in values) globals[key] = copy(values[key]);
  }
  function values() { return { ...copy(active().values), ...copy(globals) }; }
  function select(id) {
    if (!Number.isInteger(id) || id < 0 || id >= items.length) return false;
    selected = id;
    return true;
  }
  function add() {
    selected = items.length;
    const values = ProfilePresets.values('inertia');
    items.push({ name: '', values, presetId: 'inertia', baseline: copy(values) });
    return selected;
  }
  function remove() {
    if (selected < 2) return;
    items.splice(selected, 1);
    selected = Math.min(selected, items.length - 1);
  }
  function setName(name) { if (selected >= 2) active().name = name; }
  function applyPreset(id) {
    if (selected === 1) return;
    const preset = ProfilePresets.get(id);
    active().values = ProfilePresets.values(preset.id);
    active().presetId = preset.id;
    active().editorState = null;
    captureBaseline();
  }
  function captureBaseline() { active().baseline = copy(active().values); }
  function restore() {
    if (selected !== 1) {
      active().values = copy(active().baseline);
      active().editorState = null;
    }
  }
  function modified() {
    return JSON.stringify(active().values) !== JSON.stringify(active().baseline);
  }
  function config() {
    return { globals: copy(globals), profiles: items.map(({name, values}) => ({name, values: copy(values)})) };
  }
  function all() {
    return items.map((item, id) => ({ name: item.name, id, values: copy(item.values) }));
  }
  return { globalKeys, defaults, init, update, values, select, add, remove, setName, all, config,
    applyPreset, captureBaseline, restore, modified, presetId: () => active().presetId,
    editorState: () => copy(active().editorState ?? null),
    selected: () => selected, ready: () => items.length > 0 };
})();
