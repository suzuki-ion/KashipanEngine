// KashipanEngine Reference - shared navigation renderer
// Loaded together with one of nav-engine-data.js / nav-editor-data.js / nav-script-data.js,
// which each define KE_PAGES (relative to the Reference/ root) plus KE_SITE / KE_SITE_LABEL /
// KE_OTHER_SITES (array of { label, href }) for the cross-site switcher.
(function () {
  // A small offline lexer for the C++ / AngelScript examples. Read textContent
  // and create text nodes so sample strings can never become HTML markup.
  const codeKeywords = new Set((
    "alignas alignof and as auto break case cast catch class const constexpr consteval constinit " +
    "continue co_await co_return co_yield default delete do dynamic_cast else enum explicit export " +
    "external final for friend from function funcdef if import in inout interface is namespace new " +
    "noexcept not operator or out override private protected public register reinterpret_cast " +
    "requires return shared sizeof static static_assert static_cast struct switch template this " +
    "thread_local throw try typedef typename union using virtual volatile while xor"
  ).split(/\s+/));
  const codeTypes = new Set((
    "bool char char8_t char16_t char32_t double float int int8 int16 int32 int64 long short " +
    "signed string uint uint8 uint16 uint32 uint64 unsigned void wchar_t size_t array dictionary"
  ).split(/\s+/));
  const codeLiterals = new Set(["true", "false", "null", "nullptr"]);

  function highlightCodeBlocks() {
    document.querySelectorAll("pre code").forEach((code) => {
      if (code.dataset.highlighted) return;
      const source = code.textContent;
      const language = Array.from(code.classList).find((name) => name.startsWith("language-"));
      if (language && !["language-angelscript", "language-cpp", "language-c", "language-json"].includes(language)) return;
      const trimmed = source.replace(/^\s*(?:\/\/[^\n]*(?:\n|$)\s*)*/, "");
      const isJson = language === "language-json" || /^[\[{]\s*(?:"|\{|\]|\})/.test(trimmed);
      // Unlabelled directory trees, command lines and output stay plain text.
      if (!language && !isJson && !/[;{}]|^\s*#(?:include|pragma|define)\b/m.test(source)) return;

      const fragment = document.createDocumentFragment();
      const tokens = /\/\/[^\r\n]*|\/\*[\s\S]*?(?:\*\/|$)|R"([^\s()\\]{0,16})\([\s\S]*?\)\1"|"(?:\\[\s\S]|[^"\\])*"|'(?:\\[\s\S]|[^'\\])*'|^\s*#[^\r\n]*|\b(?:0[xX][\da-fA-F]+|0[bB][01]+|\d+(?:\.\d*)?(?:[eE][+-]?\d+)?)(?:[uUlLfF]*)\b|\.\d+(?:[eE][+-]?\d+)?[fF]?\b|[a-zA-Z_][\w]*/gm;
      let offset = 0;
      let match;
      while ((match = tokens.exec(source)) !== null) {
        fragment.appendChild(document.createTextNode(source.slice(offset, match.index)));
        const token = match[0];
        const following = source.slice(tokens.lastIndex);
        let kind = "";
        if (token.startsWith("//") || token.startsWith("/*")) kind = "comment";
        else if (token.startsWith('"') || token.startsWith("'") || token.startsWith('R"')) {
          kind = isJson && /^\s*:/.test(following) ? "property" : "string";
        } else if (token.trimStart().startsWith("#")) kind = "preprocessor";
        else if (/^(?:\d|\.\d)/.test(token)) kind = "number";
        else if (codeLiterals.has(token)) kind = "literal";
        else if (!isJson && codeKeywords.has(token)) kind = "keyword";
        else if (!isJson && codeTypes.has(token)) kind = "type";
        else if (!isJson && /^\s*\(/.test(following)) kind = "function";
        else if (!isJson && /^[A-Z][a-zA-Z0-9]*$/.test(token)) kind = "type";
        if (kind) {
          const span = document.createElement("span");
          span.className = "syntax-" + kind;
          span.textContent = token;
          fragment.appendChild(span);
        } else {
          fragment.appendChild(document.createTextNode(token));
        }
        offset = tokens.lastIndex;
      }
      fragment.appendChild(document.createTextNode(source.slice(offset)));
      code.replaceChildren(fragment);
      code.dataset.highlighted = "true";
    });
  }

  // href の "/" の数から、現在ページを起点に Reference/ ルートへ戻るための "../" の数を求める
  function prefixForHref(href) {
    const depth = href.split("/").length - 1;
    return "../".repeat(depth);
  }

  function currentPrefix() {
    const page = KE_PAGES.find((p) => p.id === document.body.getAttribute("data-page"));
    return page ? prefixForHref(page.href) : "";
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

  // main内の見出し(h2/h3)へ出現順の連番IDを振る（検索結果からのジャンプ先として使用。
  // search-index.js を生成するスクリプト側でも同じ順序でカウントしている）
  function assignHeadingIds() {
    const headings = document.querySelectorAll("main h2, main h3");
    headings.forEach((h, i) => {
      if (!h.id) h.id = "kehead-" + i;
    });
  }

  function renderSidebar(currentId) {
    const el = document.getElementById("sidebar");
    if (!el) return;
    const prefix = currentPrefix();
    const groups = groupsInOrder(KE_PAGES);

    let html = '';
    html += '<a class="sidebar-title" href="' + prefix + KE_PAGES[0].href + '">KashipanEngine</a>';
    html += '<span class="sidebar-sub">' + KE_SITE_LABEL + '</span>';
    html += '<a class="sidebar-search" href="' + prefix + 'search.html">&#128269; 全リファレンスを検索</a>';
    if (typeof KE_OTHER_SITES !== "undefined") {
      KE_OTHER_SITES.forEach((s) => {
        html += '<a class="sidebar-switch" href="' + prefix + s.href + '">&#8646; ' + s.label + '</a>';
      });
    }

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
    const prefix = currentPrefix();
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
    const prefix = currentPrefix();
    const page = KE_PAGES.find((p) => p.id === currentId);
    if (!page) return;
    el.innerHTML = '<a href="' + prefix + KE_PAGES[0].href + '">' + KE_SITE_LABEL + '</a> / ' +
      (page.group ? page.group + ' / ' : '') + page.title;
  }

  document.addEventListener("DOMContentLoaded", function () {
    const currentId = document.body.getAttribute("data-page");
    assignHeadingIds();
    highlightCodeBlocks();
    renderSidebar(currentId);
    renderFooter(currentId);
    renderBreadcrumb(currentId);
  });
})();
