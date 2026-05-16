/**
 * Shared behavior for static guide pages.
 */
// eslint-disable-next-line no-unused-vars
const ContentPage = (() => {
  function renderLangBlocks() {
    const lang = I18n.lang();
    document.documentElement.lang = lang;
    document.querySelectorAll('.lang-en').forEach(el => {
      el.hidden = lang !== 'en';
    });
    document.querySelectorAll('.lang-zh').forEach(el => {
      el.hidden = lang !== 'zh';
    });
  }

  function updateLangToggle() {
    const btn = document.getElementById('lang-toggle');
    if (btn) btn.textContent = I18n.lang() === 'zh' ? 'EN' : '中文';
  }

  function copyLabel(copied = false) {
    if (copied) return I18n.lang() === 'zh' ? '已复制' : 'Copied';
    return I18n.lang() === 'zh' ? '复制' : 'Copy';
  }

  function updateCopyButtons() {
    document.querySelectorAll('.code-copy').forEach(btn => {
      if (btn.dataset.copied === 'true') return;
      btn.textContent = copyLabel(false);
    });
  }

  function initCopyButtons() {
    document.querySelectorAll('.doc-page pre').forEach(pre => {
      if (pre.closest('.code-block')) return;

      const wrapper = document.createElement('div');
      wrapper.className = 'code-block';
      pre.parentNode.insertBefore(wrapper, pre);
      wrapper.appendChild(pre);

      const btn = document.createElement('button');
      btn.className = 'code-copy';
      btn.type = 'button';
      btn.textContent = copyLabel(false);
      wrapper.appendChild(btn);

      btn.addEventListener('click', async () => {
        const text = pre.innerText;
        try {
          await navigator.clipboard.writeText(text);
        } catch (err) {
          const textarea = document.createElement('textarea');
          textarea.value = text;
          textarea.setAttribute('readonly', '');
          textarea.style.position = 'fixed';
          textarea.style.opacity = '0';
          document.body.appendChild(textarea);
          textarea.select();
          document.execCommand('copy');
          textarea.remove();
        }

        btn.dataset.copied = 'true';
        btn.textContent = copyLabel(true);
        setTimeout(() => {
          btn.dataset.copied = 'false';
          btn.textContent = copyLabel(false);
        }, 1200);
      });
    });
  }

  function init() {
    Nav.render('nav');
    renderLangBlocks();
    updateLangToggle();
    initCopyButtons();

    const langToggle = document.getElementById('lang-toggle');
    if (!langToggle) return;

    langToggle.addEventListener('click', () => {
      I18n.setLang(I18n.lang() === 'zh' ? 'en' : 'zh');
      renderLangBlocks();
      updateLangToggle();
      updateCopyButtons();
      Nav.updateLang();
    });
  }

  return { init, renderLangBlocks };
})();
