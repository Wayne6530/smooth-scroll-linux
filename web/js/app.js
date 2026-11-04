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

  const onParamChange = (values, changedKey) => {
    updateTomlPreview(values);
    if (typeof PresetUI !== 'undefined') PresetUI.updateStatus();

    if (typeof Visualization !== 'undefined') {
      Visualization.update(values);
    }
  };

  ConfigEditor.init(schema, onParamChange);

  PresetUI.initEditor();

  updateTomlPreview(ConfigEditor.getValues());

  const tomlDialog = document.getElementById('toml-dialog');
  document.getElementById('btn-preview').addEventListener('click', () => {
    updateTomlPreview(ConfigEditor.getValues());
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
    const toml = ConfigWriter.generate(ConfigEditor.getValues());
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
    if (typeof Visualization !== 'undefined') {
      Visualization.renderLabels();
    }
    if (typeof Nav !== 'undefined') {
      Nav.updateLang();
    }
    renderNextSteps();
    PresetUI.renderEditor();
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
  if (preview) {
    preview.value = ConfigWriter.generate(values);
  }
}

function updateLangToggle() {
  const btn = document.getElementById('lang-toggle');
  if (btn) btn.textContent = I18n.lang() === 'zh' ? 'EN' : '中文';
}

function renderConfigPageLabels() {
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
