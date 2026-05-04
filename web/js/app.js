/**
 * Entry point: wires editor + writer + visualization using the static schema.
 */
document.addEventListener('DOMContentLoaded', () => {
  const schema = ParamSchema;

  // Render nav first (creates #lang-toggle in the DOM)
  if (typeof Nav !== 'undefined') {
    Nav.render('nav');
  }

  ConfigWriter.init(schema);

  const onParamChange = (values, changedKey) => {
    updateTomlPreview(values);

    if (typeof Visualization !== 'undefined') {
      Visualization.update(values);
    }
  };

  ConfigEditor.init(schema, onParamChange);

  updateTomlPreview(ConfigEditor.getValues());

  document.getElementById('btn-copy').addEventListener('click', () => {
    const toml = ConfigWriter.generate(ConfigEditor.getValues());
    ConfigWriter.copyToClipboard(toml);
    const btn = document.getElementById('btn-copy');
    const orig = btn.textContent;
    btn.textContent = I18n.t('btn.copied');
    setTimeout(() => { btn.textContent = orig; }, 1500);
  });

  document.getElementById('btn-download').addEventListener('click', () => {
    const toml = ConfigWriter.generate(ConfigEditor.getValues());
    ConfigWriter.download(toml, 'smooth-scroll.toml');
  });

  document.getElementById('btn-reset').addEventListener('click', () => {
    ConfigEditor.resetToDefaults();
    updateTomlPreview(ConfigEditor.getValues());
  });

  document.getElementById('lang-toggle').addEventListener('click', () => {
    const newLang = I18n.lang() === 'zh' ? 'en' : 'zh';
    I18n.setLang(newLang);
    updateLangToggle();
    ConfigEditor.render();
    if (typeof Visualization !== 'undefined') {
      Visualization.renderLabels();
    }
    if (typeof Nav !== 'undefined') {
      Nav.updateLang();
    }
    renderNextSteps();
  });

  updateLangToggle();
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
