/**
 * Entry point: wires editor + writer + visualization using the static schema.
 */
document.addEventListener('DOMContentLoaded', () => {
  const schema = ParamSchema;
  document.documentElement.lang = I18n.lang();

  // Render nav first (creates #lang-toggle in the DOM)
  if (typeof Nav !== 'undefined') {
    Nav.render('nav');
  }

  ConfigWriter.init(schema);

  const onParamChange = (values, changedKey, editorState) => {
    if (Profiles.ready()) {
      Profiles.update(values, editorState);
    }
    updateTomlPreview(values);
    if (typeof PresetUI !== 'undefined') PresetUI.updateStatus();

    if (typeof Visualization !== 'undefined') {
      const id = Profiles.ready() ? Profiles.selected() : 0;
      const valid = ConfigWriter.validateProfile(values, id).length === 0;
      Visualization.update(values, valid);
    }
  };

  ConfigEditor.init(schema, onParamChange);

  PresetUI.initEditor();
  ProfileUI.render();

  updateTomlPreview(ConfigEditor.getValues());

  const tomlDialog = document.getElementById('toml-dialog');
  document.getElementById('btn-preview').addEventListener('click', () => {
    if (!updateTomlPreview(ConfigEditor.getValues())) return;
    tomlDialog.showModal();
  });
  document.getElementById('btn-close-preview').addEventListener('click', () => tomlDialog.close());
  tomlDialog.addEventListener('click', event => {
    if (event.target !== tomlDialog) return;
    const bounds = tomlDialog.getBoundingClientRect();
    if (event.clientX < bounds.left || event.clientX > bounds.right ||
        event.clientY < bounds.top || event.clientY > bounds.bottom) tomlDialog.close();
  });

  document.getElementById('btn-download').addEventListener('click', () => {
    if (!updateTomlPreview(ConfigEditor.getValues())) return;
    const toml = ConfigWriter.generateConfig(Profiles.config());
    ConfigWriter.download(toml, 'smooth-scroll.toml');
  });

  document.getElementById('btn-reset').addEventListener('click', () => {
    PresetUI.restore();
    updateTomlPreview(ConfigEditor.getValues());
  });

  document.getElementById('lang-toggle').addEventListener('click', () => {
    const newLang = I18n.lang() === 'zh' ? 'en' : 'zh';
    I18n.setLang(newLang);
    document.documentElement.lang = newLang;
    updateLangToggle();
    renderConfigPageLabels();
    ConfigEditor.render();
    ProfileUI.render();
    if (typeof Visualization !== 'undefined') {
      Visualization.renderLabels();
    }
    if (typeof Nav !== 'undefined') {
      Nav.updateLang();
    }
    renderNextSteps();
    PresetUI.renderEditor();
    updateTomlPreview(ConfigEditor.getValues());
  });

  updateLangToggle();
  renderConfigPageLabels();
  renderNextSteps();

  document.querySelectorAll('.scenario-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      document.querySelectorAll('.scenario-btn').forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      if (typeof Visualization !== 'undefined') {
        Visualization.setScenario(btn.dataset.scenario);
      }
    });
  });

  if (typeof Visualization !== 'undefined') {
    Visualization.init(ConfigEditor.getValues());
  }
});

function updateTomlPreview(values) {
  const preview = document.getElementById('toml-preview');
  const issues = Profiles.ready() ? ConfigWriter.validateConfig(Profiles.config()) : [];
  const error = document.getElementById('export-error');
  const previewButton = document.getElementById('btn-preview');
  if (previewButton) previewButton.disabled = issues.length > 0;
  const download = document.getElementById('btn-download');
  if (download) download.disabled = issues.length > 0;
  const nextSteps = document.getElementById('next-steps');
  if (nextSteps) nextSteps.hidden = issues.length > 0;
  if (error) {
    error.hidden = issues.length === 0;
    document.getElementById('export-error-title').textContent = I18n.t('validation.fix-before-preview');
    document.getElementById('export-error-details').textContent = issues.map(issue => {
      const labels = issue.keys.map(key => I18n.label(ParamSchema.params.find(param => param.key === key))).join(' / ');
      const reason = I18n.t('validation.' + issue.kind);
      return `Profile ${issue.profileId} — ${labels}: ${reason}`;
    }).join('\n');
  }
  if (preview) {
    preview.hidden = issues.length > 0;
    preview.value = issues.length ? '' : Profiles.ready()
      ? ConfigWriter.generateConfig(Profiles.config()) : ConfigWriter.generate(values);
  }
  return issues.length === 0;
}

function updateLangToggle() {
  const btn = document.getElementById('lang-toggle');
  if (btn) btn.textContent = I18n.lang() === 'zh' ? 'EN' : '中文';
}

function renderConfigPageLabels() {
  document.getElementById('toml-config-version').textContent = `Config v${ParamSchema.CONFIG_VERSION}`;
  const labels = [
    ['btn-download', 'btn.download'],
    ['toml-preview-label', 'toml.preview'],
    ['btn-preview', 'btn.preview-export'],
    ['btn-close-preview', 'btn.close'],
  ];

  for (const [id, key] of labels) {
    const el = document.getElementById(id);
    if (el) el.textContent = I18n.t(key);
  }
}

function renderNextSteps() {
  const titleEl = document.getElementById('next-steps-title');
  const listEl = document.getElementById('next-steps-list');
  if (!titleEl || !listEl) return;
  titleEl.textContent = I18n.t('next-steps.title');
  listEl.innerHTML = '';
  for (let i = 1; i <= 3; i++) {
    const li = document.createElement('li');
    li.textContent = I18n.t('next-steps.step' + i);
    listEl.appendChild(li);
  }
}
