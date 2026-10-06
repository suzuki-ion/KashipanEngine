
'use strict';
(() => {
  const script = document.querySelector('script[src*="parody.js"]');
  const base = new URL('.', script.src);
  const dialog = document.querySelector('.prize-dialog');
  const toast = document.querySelector('.offer-toast');
  const trigger = document.querySelector('.notification-trigger');
  let accepted = false;
  try { accepted = sessionStorage.getItem('portfolio-prize-seen') === '1'; } catch {}
  const dismiss = () => {
    dialog.close();
    try { sessionStorage.setItem('portfolio-prize-seen', '1'); } catch {}
  };
  dialog.querySelectorAll('[data-close-prize]').forEach(button => button.addEventListener('click', dismiss));
  dialog.querySelector('[data-claim-prize]').addEventListener('click', dismiss);
  dialog.addEventListener('cancel', () => {
    try { sessionStorage.setItem('portfolio-prize-seen', '1'); } catch {}
  });
  if (!accepted) dialog.showModal();
  const offers = [
    ['grunner', 'グランナー', 'granchor', '東京ゲームショウ2026 出展作品が見つかりました。'],
    ['ice-boss', '大氷怪鳥 ジー・クアイス', 'ice', 'ボス戦への挑戦状が届いています。'],
    ['kurukuru-layer', 'くるくるレイヤー', 'kurukuru', '学科内評価1位！回転するステージをご確認ください。'],
    ['clubom', 'CLUBOM', 'clubom', '光と音のステージがあなたを待っています。'],
    ['monoku', 'モノクション', 'monoku', '白黒の世界と専用エディターが見つかりました。'],
    ['gun-sole', 'Gun Sole', 'gunsole', 'プレイヤーとカメラの秘密をご確認ください。'],
    ['kakedama', 'カケダマ', 'kakedama', 'コインを集めるシューティングが公開中です。'],
    ['ore-cave', '鉱石洞窟', 'cave', '広大な地下ステージへの招待状が届きました。'],
    ['job-hunting', '3D横スクロールアクション', 'featured', '制作中の作品とプログラム説明を公開中です。']
  ];
  let index = 0;
  let timer;
  const schedule = () => { window.clearTimeout(timer); timer = window.setTimeout(showOffer, 18000); };
  function showOffer() {
    if (document.hidden || dialog.open) { schedule(); return; }
    const [slug, title, image, copy] = offers[index++ % offers.length];
    toast.querySelector('#offer-title').textContent = title;
    toast.querySelector('.offer-copy').textContent = copy;
    toast.querySelector('.offer-image').src = new URL('assets/' + image + '.webp', base).href;
    toast.querySelector('.offer-image').alt = title + 'のゲーム画面';
    toast.querySelector('.offer-link').href = new URL('works/' + slug + '/', base).href;
    toast.hidden = false;
    schedule();
  }
  toast.querySelector('[data-close-offer]').addEventListener('click', () => { toast.hidden = true; schedule(); });
  trigger.addEventListener('click', showOffer);
  timer = window.setTimeout(showOffer, 8000);
})();
