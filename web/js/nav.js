/**
 * Shared navigation bar component for all pages.
 * Injects a horizontal nav with page links, GitHub link, and language toggle.
 */
// eslint-disable-next-line no-unused-vars
const Nav = (() => {

  const PAGES = [
    { id: 'feature',    href: 'index.html',      labelKey: 'nav.feature' },
    { id: 'quickstart', href: 'quickstart.html', labelKey: 'nav.quickstart' },
    { id: 'config',     href: 'config.html',     labelKey: 'nav.config' },
    { id: 'faq',        href: 'faq.html',        labelKey: 'nav.faq' },
  ];

  const GITHUB_URL = 'https://github.com/Wayne6530/smooth-scroll-linux';

  function render(containerId) {
    const container = document.getElementById(containerId);
    if (!container) return;

    const currentPage = getCurrentPageId();

    const nav = document.createElement('nav');
    nav.className = 'site-nav';

    // Left: title + page links
    const left = document.createElement('div');
    left.className = 'nav-left';

    const title = document.createElement('a');
    title.className = 'nav-title';
    title.href = 'index.html';
    title.setAttribute('aria-label', 'Smooth Scroll Linux');
    title.textContent = 'Smooth Scroll Linux';
    left.appendChild(title);

    for (const page of PAGES) {
      const link = document.createElement('a');
      link.className = 'nav-link' + (page.id === currentPage ? ' active' : '');
      link.href = page.href;
      link.dataset.navId = page.id;
      link.textContent = I18n.t(page.labelKey);
      left.appendChild(link);
    }

    nav.appendChild(left);

    // Right: GitHub + language toggle
    const right = document.createElement('div');
    right.className = 'nav-right';

    const githubLink = document.createElement('a');
    githubLink.className = 'nav-github';
    githubLink.href = GITHUB_URL;
    githubLink.target = '_blank';
    githubLink.rel = 'noopener noreferrer';
    githubLink.setAttribute('aria-label', 'GitHub');
    githubLink.innerHTML = '<svg viewBox="0 0 16 16" width="18" height="18" fill="currentColor"><path d="M8 0C3.58 0 0 3.58 0 8c0 3.54 2.29 6.53 5.47 7.59.4.07.55-.17.55-.38 0-.19-.01-.82-.01-1.49-2.01.37-2.53-.49-2.69-.94-.09-.23-.48-.94-.82-1.13-.28-.15-.68-.52-.01-.53.63-.01 1.08.58 1.23.82.72 1.21 1.87.87 2.33.66.07-.52.28-.87.51-1.07-1.78-.2-3.64-.89-3.64-3.95 0-.87.31-1.59.82-2.15-.08-.2-.36-1.02.08-2.12 0 0 .67-.21 2.2.82.64-.18 1.32-.27 2-.27.68 0 1.36.09 2 .27 1.53-1.04 2.2-.82 2.2-.82.44 1.1.16 1.92.08 2.12.51.56.82 1.27.82 2.15 0 3.07-1.87 3.75-3.65 3.95.29.25.54.73.54 1.48 0 1.07-.01 1.93-.01 2.2 0 .21.15.46.55.38A8.013 8.013 0 0016 8c0-4.42-3.58-8-8-8z"/></svg>';
    right.appendChild(githubLink);

    const langBtn = document.createElement('button');
    langBtn.id = 'lang-toggle';
    langBtn.textContent = I18n.lang() === 'zh' ? 'EN' : '中文';
    right.appendChild(langBtn);

    nav.appendChild(right);
    container.appendChild(nav);
  }

  function updateLang() {
    document.querySelectorAll('.nav-link').forEach(link => {
      const page = PAGES.find(p => p.id === link.dataset.navId);
      if (page) link.textContent = I18n.t(page.labelKey);
    });
  }

  function getCurrentPageId() {
    const path = location.pathname;
    const filename = path.split('/').pop().replace('.html', '') || 'index';
    if (filename === 'index') return 'feature';
    if (filename === 'config') return 'config';
    return filename;
  }

  return { render, updateLang };
})();
