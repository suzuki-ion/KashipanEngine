// KashipanEngine Reference - shared navigation renderer
(function () {
  function basePrefix() {
    return location.pathname.includes("/Tutorial/") ? "../" : "";
  }

  function groupsInOrder(pages) {
    const order = [];
    const map = new Map();
    pages.forEach((p) => {
      if (!map.has(p.group)) { map.set(p.group, []); order.push(p.group); }
      map.get(p.group).push(p);
    });
    return order.map((g) => ({ group: g, items: map.get(g) }));
  }

  function renderSidebar(currentId) {
    const el = document.getElementById("sidebar");
    if (!el) return;
    const prefix = basePrefix();
    const groups = groupsInOrder(KE_PAGES);

    let html = '';
    html += '<a class="sidebar-title" href="' + prefix + '00_Index.html">KashipanEngine</a>';
    html += '<span class="sidebar-sub">使い方リファレンス</span>';

    groups.forEach((g) => {
      html += '<div class="nav-group"><div class="nav-group-title">' + g.group + '</div><ul>';
      g.items.forEach((p) => {
        const active = p.id === currentId ? ' class="active"' : '';
        html += '<li><a' + active + ' href="' + prefix + p.href + '">' + p.title + '</a></li>';
      });
      html += '</ul></div>';
    });

    el.innerHTML = html;
  }

  function renderFooter(currentId) {
    const el = document.getElementById("page-footer");
    if (!el) return;
    const prefix = basePrefix();
    const idx = KE_PAGES.findIndex((p) => p.id === currentId);
    if (idx === -1) return;
    const prev = idx > 0 ? KE_PAGES[idx - 1] : null;
    const next = idx < KE_PAGES.length - 1 ? KE_PAGES[idx + 1] : null;

    let html = '';
    html += prev
      ? '<a class="prev" href="' + prefix + prev.href + '"><small>&larr; 前へ</small>' + prev.title + '</a>'
      : '<span></span>';
    html += next
      ? '<a class="next" href="' + prefix + next.href + '"><small>次へ &rarr;</small>' + next.title + '</a>'
      : '<span></span>';
    el.innerHTML = html;
  }

  function renderBreadcrumb(currentId) {
    const el = document.getElementById("breadcrumb");
    if (!el) return;
    const prefix = basePrefix();
    const page = KE_PAGES.find((p) => p.id === currentId);
    if (!page) return;
    el.innerHTML = '<a href="' + prefix + '00_Index.html">リファレンス</a> / ' +
      (page.group ? page.group + ' / ' : '') + page.title;
  }

  document.addEventListener("DOMContentLoaded", function () {
    const currentId = document.body.getAttribute("data-page");
    renderSidebar(currentId);
    renderFooter(currentId);
    renderBreadcrumb(currentId);
  });
})();
