/** Complete downloadable configurations: shared device options plus profiles. */
const ConfigPresets = (() => {
  const globalKeys = ParamSchema.params.filter(param => param.group === 'device').map(param => param.key);
  const items = ProfilePresets.items.filter(preset => preset.id !== 'compatibility');
  function get(id) { return items.find(item => item.id === id) || items[0]; }
  function values(id) {
    const preset = get(id);
    const defaults = ParamSchema.getDefaultValues();
    return {
      globals: Object.fromEntries(globalKeys.map(key => [key, structuredClone(defaults[key])])),
      profiles: [
        { name: '', values: ProfilePresets.values(preset.id), presetId: preset.id },
        { name: 'Compatibility', values: ProfilePresets.values('compatibility'), presetId: 'compatibility' },
      ],
    };
  }
  return { items, get, values, globalKeys };
})();
