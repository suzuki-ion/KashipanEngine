
'use strict';
const sidebar = document.querySelector('.sidebar');
if (sidebar) {
  const panel = sidebar.querySelector('.side-panel');
  const desktop = window.matchMedia('(min-width: 1100px)');
  const syncLayout = () => { panel.open = desktop.matches; };
  syncLayout();
  desktop.addEventListener('change', syncLayout);
  const links = [...sidebar.querySelectorAll('.section-links a')];
  const targets = links.map(link => document.getElementById(link.hash.slice(1)));
  const updateCurrentSection = () => {
    const threshold = desktop.matches ? 120 : 85;
    let active = 0;
    let nearestTop = -Infinity;
    targets.forEach((target, index) => {
      if (!target) return;
      const top = target.getBoundingClientRect().top;
      if (top <= threshold && top >= nearestTop) {
        nearestTop = top;
        active = index;
      }
    });
    links.forEach((link, index) => {
      if (index === active) link.setAttribute('aria-current', 'location');
      else link.removeAttribute('aria-current');
    });
  };
  let scheduled = false;
  window.addEventListener('scroll', () => {
    if (scheduled) return;
    scheduled = true;
    window.requestAnimationFrame(() => {
      updateCurrentSection();
      scheduled = false;
    });
  }, { passive: true });
  window.addEventListener('resize', updateCurrentSection);
  sidebar.addEventListener('click', event => {
    if (event.target.closest('a') && !desktop.matches) panel.open = false;
  });
  updateCurrentSection();
}
