// KashipanEngine プロジェクトランチャーの画面制御
//
// プロジェクトの列挙・作成・起動はすべてC++側（ProjectManager）が行い、
// この画面は表示と入力の受け渡しに徹する。
//   C++ → JS : { type: "projects" | "status", ... }
//   JS → C++ : { action: "refresh" | "open" | "create", ... }

const host = window.chrome && window.chrome.webview;

const listElement = document.getElementById("project-list");
const openButton = document.getElementById("open");
const refreshButton = document.getElementById("refresh");
const newNameInput = document.getElementById("new-name");
const createButton = document.getElementById("create");
const statusElement = document.getElementById("status");

/// 現在表示しているプロジェクト一覧
let projects = [];
/// 選択中のプロジェクト名（未選択なら null）
let selectedName = null;
/// エディターの起動中など、操作を受け付けたくない状態か
let isBusy = false;

function send(message) {
    if (host) host.postMessage(message);
}

function setStatus(text, level) {
    statusElement.textContent = text || "";
    statusElement.className = level || "";
}

function updateButtons() {
    openButton.disabled = isBusy || selectedName === null;
    refreshButton.disabled = isBusy;
    createButton.disabled = isBusy || newNameInput.value.trim() === "";
    newNameInput.disabled = isBusy;
}

function selectProject(name) {
    selectedName = name;
    for (const element of listElement.children) {
        element.classList.toggle("selected", element.dataset.name === name);
    }
    updateButtons();
}

/// プロジェクト名からバッジ用の頭文字を作る（英数字なら1文字目、日本語などはそのまま1文字）
function initialOf(name) {
    return name.trim().charAt(0).toUpperCase() || "?";
}

function renderProjects() {
    listElement.textContent = "";

    if (projects.length === 0) {
        const empty = document.createElement("div");
        empty.className = "empty";
        empty.textContent = "プロジェクトがありません。下の欄から新規作成してください。";
        listElement.appendChild(empty);
        selectProject(null);
        return;
    }

    for (const project of projects) {
        const item = document.createElement("div");
        item.className = "project";
        item.dataset.name = project.name;

        const badge = document.createElement("div");
        badge.className = "badge";
        badge.textContent = initialOf(project.name);

        const info = document.createElement("div");
        info.className = "info";

        const name = document.createElement("div");
        name.className = "name";
        name.textContent = project.name;

        const path = document.createElement("div");
        path.className = "path";
        path.textContent = project.path;
        path.title = project.path;

        info.append(name, path);
        item.append(badge, info);

        item.addEventListener("click", () => selectProject(project.name));
        // ダブルクリックはそのまま起動（Unity Hubと同じ操作感）
        item.addEventListener("dblclick", () => {
            selectProject(project.name);
            requestOpen();
        });

        listElement.appendChild(item);
    }
}

function requestOpen() {
    if (isBusy || selectedName === null) return;
    isBusy = true;
    updateButtons();
    setStatus("エディターを起動しています...", "busy");
    send({ action: "open", name: selectedName });
}

function requestCreate() {
    const name = newNameInput.value.trim();
    if (isBusy || name === "") return;
    setStatus("");
    send({ action: "create", name: name });
}

//--------- 入力 ---------//
openButton.addEventListener("click", requestOpen);
refreshButton.addEventListener("click", () => send({ action: "refresh" }));
createButton.addEventListener("click", requestCreate);
newNameInput.addEventListener("input", updateButtons);
newNameInput.addEventListener("keydown", (event) => {
    if (event.key === "Enter") requestCreate();
});
document.addEventListener("keydown", (event) => {
    // 一覧に触っていなくてもEnterで起動できるようにする
    if (event.key === "Enter" && event.target !== newNameInput) requestOpen();
});

//--------- C++からの通知 ---------//
if (host) {
    host.addEventListener("message", (event) => {
        const message = event.data;
        if (message.type === "projects") {
            projects = message.items || [];
            renderProjects();
            // 前回開いたプロジェクトを初期選択にする（無ければ先頭）
            const preferred = projects.some((p) => p.name === message.startup)
                ? message.startup
                : (projects[0] ? projects[0].name : null);
            selectProject(preferred);
        } else if (message.type === "status") {
            // 失敗した場合は操作を再度受け付ける
            if (message.level === "error") isBusy = false;
            if (message.clearNameInput) newNameInput.value = "";
            setStatus(message.text, message.level);
            updateButtons();
        }
    });
    send({ action: "refresh" });
}

updateButtons();
