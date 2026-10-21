// KashipanEngine 起動スプラッシュ画面
//
// C++側（SplashScreen.cpp）から100ms間隔で
//   { type: "log", items: [{ text, severity }, ...] }
// が届くので、末尾に追記して古い行から流れて消えるようにする。
// severity は LogSeverity (Debug=0, Info=1, Warning=2, Error=3, Critical=4) の数値。

(function () {
  const severityClassNames = ["severity-debug", "", "severity-warning", "severity-error", "severity-critical"];
  const maxVisibleLines = 6;

  const logList = document.getElementById("log-list");

  function appendLine(text, severity) {
    const li = document.createElement("li");
    li.textContent = text.replace(/[\r\n]+$/, "");
    const className = severityClassNames[severity] || "";
    if (className) li.className = className;
    logList.appendChild(li);

    while (logList.children.length > maxVisibleLines) {
      logList.removeChild(logList.firstChild);
    }
  }

  if (window.chrome && window.chrome.webview) {
    window.chrome.webview.addEventListener("message", (event) => {
      const message = event.data;
      if (!message) return;

      if (message.type === "log" && Array.isArray(message.items)) {
        for (const item of message.items) {
          appendLine(item.text || "", item.severity);
        }
      } else if (message.type === "closing") {
        // C++側（SplashScreen::Close）がウィンドウを破棄する直前に送ってくる。
        // splash.cssのtransitionでフェードアウトさせる
        document.body.classList.add("closing");
      }
    });
  }
})();
