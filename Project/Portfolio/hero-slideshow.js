
'use strict';
(() => {
  const hero = document.querySelector('.hero-slideshow');
  if (!hero) return;
  const slides = [...hero.querySelectorAll('.hero-slide')];
  const button = hero.querySelector('.slideshow-toggle');
  const reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)');
  let index = 0;
  let paused = reducedMotion.matches;
  let visible = true;
  let timer = null;
  let loading = false;
  const canPlay = () => !paused && !document.hidden && visible;
  const advance = async () => {
    if (!canPlay() || loading) return;
    loading = true;
    const nextIndex = (index + 1) % slides.length;
    const next = slides[nextIndex];
    try {
      next.loading = 'eager';
      await next.decode();
      if (!canPlay()) return;
      next.classList.add('is-active');
      slides[index].classList.remove('is-active');
      index = nextIndex;
    } catch {
      // Keep the current image visible if the next image cannot load.
    } finally {
      loading = false;
    }
  };
  const syncPlayback = () => {
    window.clearInterval(timer);
    timer = null;
    button.hidden = false;
    hero.classList.toggle('allow-motion', !paused);
    button.setAttribute('aria-pressed', String(paused));
    button.textContent = paused ? '背景の切り替えを再開' : '背景の切り替えを停止';
    if (canPlay()) timer = window.setInterval(advance, 6000);
  };
  button.addEventListener('click', () => {
    paused = !paused;
    syncPlayback();
  });
  reducedMotion.addEventListener('change', () => {
    paused = reducedMotion.matches;
    syncPlayback();
  });
  document.addEventListener('visibilitychange', syncPlayback);
  const observer = new IntersectionObserver(entries => {
    visible = entries[0].isIntersecting;
    syncPlayback();
  });
  observer.observe(hero);
  syncPlayback();
})();
