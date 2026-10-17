'use strict';
(() => {
  const script = document.querySelector('script[src*="parody.js"]');
  const base = new URL('.', script.src);
  const assetUrl = image => new URL('assets/' + image + '.webp', base).href;
  const workUrl = slug => new URL('works/' + slug + '/', base).href;
  const worksUrl = new URL('index.html#works', base).href;
  const store = {
    get: key => { try { return sessionStorage.getItem(key); } catch { return null; } },
    set: (key, value) => { try { sessionStorage.setItem(key, value); } catch {} }
  };
  const random = (min, max) => min + Math.floor(Math.random() * (max - min + 1));
  const pick = list => list[random(0, list.length - 1)];
  const shuffle = list => list.map(item => [Math.random(), item]).sort((a, b) => a[0] - b[0]).map(pair => pair[1]);
  const element = (html) => {
    const template = document.createElement('template');
    template.innerHTML = html.trim();
    return template.content.firstElementChild;
  };

  const root = document.documentElement;
  const dialog = document.querySelector('.prize-dialog');
  const toast = document.querySelector('.offer-toast');
  const trigger = document.querySelector('.notification-trigger');
  const works = [
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
  const isCalm = () => root.classList.contains('calm');
  const isBusy = () => document.hidden || isCalm() || document.querySelector('dialog[open]') !== null;
  root.classList.toggle('calm', store.get('portfolio-calm') === '1');

  // 当選通知：「あとで確認する」は一度引き止めてから閉じる
  const markPrizeSeen = () => store.set('portfolio-prize-seen', '1');
  const closePrize = () => { dialog.close(); markPrizeSeen(); };
  dialog.querySelector('.window-title [data-close-prize]').addEventListener('click', closePrize);
  dialog.querySelector('[data-claim-prize]').addEventListener('click', closePrize);
  dialog.addEventListener('cancel', markPrizeSeen);
  const prizeDismiss = dialog.querySelector('.prize-dismiss');
  prizeDismiss.addEventListener('click', () => {
    if (prizeDismiss.dataset.warned) { closePrize(); return; }
    prizeDismiss.dataset.warned = '1';
    prizeDismiss.innerHTML = '<strong>本当に閉じますか？</strong><br>9作品を見逃すことになります。（もう一度押すと閉じます）';
  });

  // 偽の認証画面（パロディ。実在サービスの見た目は使わない）
  const captcha = element(`
    <dialog class="captcha-dialog" aria-labelledby="captcha-heading">
      <div class="captcha-alert"><span aria-hidden="true">⚠</span><div><h2 id="captcha-heading">閲覧前の確認</h2><p>作品を閲覧するには、あなたが人事担当者であることを確認してください。</p></div></div>
      <div class="captcha-box">
        <div class="captcha-check-step">
          <button type="button" class="captcha-check" role="checkbox" aria-checked="false"><span class="captcha-mark" aria-hidden="true"></span><span>私は人事担当者です</span></button>
          <div class="captcha-brand"><span aria-hidden="true">🍞</span><strong>KASHIPAN CHECK</strong><small>※ロボットでも通過できます</small></div>
        </div>
        <div class="captcha-challenge" hidden>
          <div class="captcha-prompt">以下の画像から<strong>ゲーム画面</strong>をすべて選択してください</div>
          <div class="captcha-grid"></div>
          <p class="captcha-error" role="alert"></p>
          <div class="captcha-footer"><small>KASHIPAN CHECK 🍞</small><button type="button" class="captcha-verify">確認</button></div>
        </div>
        <div class="captcha-success" role="status" hidden><span aria-hidden="true">✔</span><p>認証に成功しました！<strong>あなたは採用担当者に選ばれました</strong></p></div>
      </div>
      <button type="button" class="captcha-skip">認証せずに閲覧する（推奨しません）</button>
    </dialog>`);
  document.body.append(captcha);
  const checkButton = captcha.querySelector('.captcha-check');
  const challenge = captcha.querySelector('.captcha-challenge');
  const grid = captcha.querySelector('.captcha-grid');
  const error = captcha.querySelector('.captcha-error');
  const success = captcha.querySelector('.captcha-success');
  let failures = 0;
  shuffle(works).forEach(([, title, image]) => {
    const tile = element('<button type="button" class="captcha-tile" aria-pressed="false"><img alt=""></button>');
    tile.querySelector('img').src = assetUrl(image);
    tile.querySelector('img').alt = title + 'のゲーム画面';
    tile.addEventListener('click', () => {
      tile.setAttribute('aria-pressed', String(tile.getAttribute('aria-pressed') !== 'true'));
      error.textContent = '';
    });
    grid.append(tile);
  });
  const finishCaptcha = () => captcha.close();
  captcha.addEventListener('close', () => {
    store.set('portfolio-captcha-passed', '1');
    if (!isCalm()) dialog.showModal();
  });
  checkButton.addEventListener('click', () => {
    if (checkButton.classList.contains('loading')) return;
    checkButton.classList.add('loading');
    window.setTimeout(() => {
      checkButton.closest('.captcha-check-step').hidden = true;
      challenge.hidden = false;
      grid.querySelector('button').focus();
    }, 1100);
  });
  captcha.querySelector('.captcha-verify').addEventListener('click', () => {
    const remaining = grid.querySelectorAll('[aria-pressed="false"]').length;
    if (remaining > 0) {
      failures++;
      error.textContent = remaining === works.length
        ? '画像を選択してください。'
        : `ゲーム画面がまだ${remaining}枚残っています。よく見てください。` + (failures >= 2 ? '（ヒント：全部ゲーム画面です）' : '');
      return;
    }
    challenge.hidden = true;
    success.hidden = false;
    window.setTimeout(finishCaptcha, 1600);
  });
  captcha.querySelector('.captcha-skip').addEventListener('click', finishCaptcha);

  // 作品通知トースト
  let index = 0;
  let timer;
  const schedule = () => { window.clearTimeout(timer); timer = window.setTimeout(showOffer, 18000); };
  function showOffer() {
    if (isBusy()) { schedule(); return; }
    const [slug, title, image, copy] = works[index++ % works.length];
    toast.querySelector('#offer-title').textContent = title;
    toast.querySelector('.offer-copy').textContent = copy;
    toast.querySelector('.offer-image').src = assetUrl(image);
    toast.querySelector('.offer-image').alt = title + 'のゲーム画面';
    toast.querySelector('.offer-link').href = workUrl(slug);
    toast.hidden = false;
    schedule();
  }
  toast.querySelector('[data-close-offer]').addEventListener('click', () => { toast.hidden = true; schedule(); });
  trigger.addEventListener('click', () => { if (!isCalm()) showOffer(); });

  // 読み込み中のまま止まる進捗バー
  const main = document.querySelector('main');
  main.prepend(element('<div class="load-strip" aria-hidden="true"><span>あなた専用の作品集を読み込み中…</span><div class="load-bar"><i></i></div><strong>99%</strong></div>'));

  // 画面下部の固定バー：閲覧者数・カウントダウン・CTA・通常表示への切り替え
  const bar = element(`
    <div class="scam-bar" role="region" aria-label="特別公開キャンペーン">
      <p class="scam-bar-viewers"><span class="live-dot" aria-hidden="true"></span>現在 <strong>37</strong>人が閲覧中<small>※適当です</small></p>
      <p class="scam-bar-timer"><span>特別公開終了まで</span> <strong>15:00</strong></p>
      <a class="scam-cta scam-bar-cta" href="${worksUrl}">▶ 今すぐ<em>無料で</em>9作品を見る</a>
      <button type="button" class="calm-toggle">通常表示に切り替え</button>
    </div>`);
  const calmReturn = element('<button type="button" class="calm-return">詐欺サイト風に戻す</button>');
  document.body.append(bar, calmReturn);
  const setCalm = calm => {
    root.classList.toggle('calm', calm);
    store.set('portfolio-calm', calm ? '1' : '0');
    if (calm) {
      toast.hidden = true;
      proof.hidden = true;
    }
  };
  bar.querySelector('.calm-toggle').addEventListener('click', () => setCalm(true));
  calmReturn.addEventListener('click', () => setCalm(false));

  const viewers = bar.querySelector('.scam-bar-viewers strong');
  let viewerCount = random(28, 46);
  const timerText = bar.querySelector('.scam-bar-timer');
  const countdownLength = 15 * 60;
  let deadline = Number(store.get('portfolio-deadline')) || 0;
  const resetDeadline = () => { deadline = Date.now() + countdownLength * 1000; store.set('portfolio-deadline', String(deadline)); };
  if (deadline <= Date.now()) resetDeadline();
  let extendedUntil = 0;
  const tick = () => {
    if (Math.random() < 0.35) viewerCount = Math.min(99, Math.max(12, viewerCount + random(-3, 3)));
    viewers.textContent = viewerCount;
    const now = Date.now();
    if (now < extendedUntil) return;
    let left = Math.ceil((deadline - now) / 1000);
    if (left <= 0) {
      resetDeadline();
      extendedUntil = now + 4000;
      timerText.classList.add('extended');
      timerText.innerHTML = '<strong>⚡ 好評につき公開延長決定！</strong>';
      return;
    }
    timerText.classList.remove('extended');
    const pad = value => String(value).padStart(2, '0');
    timerText.innerHTML = `<span>特別公開終了まで</span> <strong>${pad(Math.floor(left / 60))}:${pad(left % 60)}</strong>`;
  };
  tick();
  window.setInterval(tick, 1000);

  // 左下の閲覧通知（架空）
  const proof = element('<aside class="proof-toast" aria-hidden="true" hidden><img alt=""><p><span class="proof-text"></span><small></small></p></aside>');
  document.body.append(proof);
  const regions = ['東京都', '大阪府', '神奈川県', '愛知県', '福岡県', '北海道', '埼玉県', '京都府', '宮城県', '広島県'];
  const visitors = ['人事担当者', '採用担当者', 'ゲーム会社の方', 'エンジニアの方', 'プランナーの方', '通りすがりの方'];
  const actions = ['を閲覧しました', 'の詳細を確認しました', 'をダウンロードしました', 'に感動しました'];
  const showProof = () => {
    const narrow = window.matchMedia('(max-width: 720px)').matches;
    if (isBusy() || (narrow && !toast.hidden)) return;
    const [, title, image] = pick(works);
    const minutes = random(0, 9);
    proof.querySelector('img').src = assetUrl(image);
    proof.querySelector('.proof-text').textContent = `${pick(regions)}の${pick(visitors)}様が「${title}」${pick(actions)}`;
    proof.querySelector('small').textContent = (minutes === 0 ? 'たった今' : minutes + '分前') + ' ※架空の通知です';
    proof.hidden = false;
    proof.classList.remove('leaving');
    window.setTimeout(() => proof.classList.add('leaving'), 4600);
    window.setTimeout(() => { proof.hidden = true; }, 5000);
  };
  window.setTimeout(() => { showProof(); window.setInterval(showProof, 9000); }, 5000);

  // 右側の追従バナー（広い画面のみ）
  const sideAd = element(`
    <aside class="side-ad" aria-label="広告">
      <p class="side-ad-label">PR / 広告</p>
      <p class="side-ad-head">話題沸騰！</p>
      <a class="side-ad-link"><span class="side-ad-stamp">衝撃</span><img alt=""><strong></strong></a>
      <p class="side-ad-copy"></p>
      <a class="scam-cta side-ad-cta">詳しく見る ▶</a>
      <small>※画像はイメージです（実際のゲーム画面です）</small>
    </aside>`);
  document.body.append(sideAd);
  const stamps = ['衝撃', '話題', '必見', '神作', '速報'];
  let adIndex = random(0, works.length - 1);
  const rotateAd = () => {
    const [slug, title, image, copy] = works[adIndex++ % works.length];
    sideAd.querySelector('img').src = assetUrl(image);
    sideAd.querySelector('img').alt = title + 'のゲーム画面';
    sideAd.querySelector('.side-ad-link strong').textContent = title;
    sideAd.querySelector('.side-ad-copy').textContent = copy;
    sideAd.querySelector('.side-ad-stamp').textContent = pick(stamps);
    sideAd.querySelectorAll('a').forEach(link => { link.href = workUrl(slug); });
  };
  rotateAd();
  window.setInterval(() => { if (!document.hidden && !isCalm()) rotateAd(); }, 6000);

  // 初回だけ：認証画面 → 当選通知
  if (!isCalm() && store.get('portfolio-prize-seen') !== '1') {
    if (store.get('portfolio-captcha-passed') === '1') dialog.showModal();
    else captcha.showModal();
  }
  timer = window.setTimeout(showOffer, 8000);
})();
