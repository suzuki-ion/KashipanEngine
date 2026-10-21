// KashipanEngine プロジェクトランチャーの画面制御
//
// プロジェクトの列挙・作成・削除・起動はすべてC++側（ProjectManager）が行い、
// この画面は表示と入力の受け渡しに徹する。
//   C++ → JS : { type: "projects" | "status", ... }
//   JS → C++ : { action: "refresh" | "open" | "create" | "delete" | "reveal", ... }

const host = window.chrome && window.chrome.webview;

const listElement = document.getElementById("project-list");
const openButton = document.getElementById("open");
const revealButton = document.getElementById("reveal");
const deleteButton = document.getElementById("delete");
const refreshButton = document.getElementById("refresh");
const templateSelect = document.getElementById("template");
const templateDescription = document.getElementById("template-description");
const newNameInput = document.getElementById("new-name");
const createButton = document.getElementById("create");
const statusElement = document.getElementById("status");

const confirmOverlay = document.getElementById("confirm-overlay");
const confirmName = document.getElementById("confirm-name");
const confirmPath = document.getElementById("confirm-path");
const confirmCancelButton = document.getElementById("confirm-cancel");
const confirmDeleteButton = document.getElementById("confirm-delete");

/// 現在表示しているプロジェクト一覧
let projects = [];
/// 選択できるテンプレート一覧
let templates = [];
/// 選択中のプロジェクト名（未選択なら null）
let selectedName = null;
/// エディターの起動中など、操作を受け付けたくない状態か
let isBusy = false;
/// 削除の確認ダイアログを表示中か
let isConfirming = false;

function send(message) {
    if (host) host.postMessage(message);
}

function setStatus(text, level) {
    statusElement.textContent = text || "";
    statusElement.className = level || "";
}

function findProject(name) {
    return projects.find((project) => project.name === name) || null;
}

function updateButtons() {
    const hasSelection = selectedName !== null;
    const locked = isBusy || isConfirming;
    openButton.disabled = locked || !hasSelection;
    revealButton.disabled = locked || !hasSelection;
    deleteButton.disabled = locked || !hasSelection;
    refreshButton.disabled = locked;
    createButton.disabled = locked || newNameInput.value.trim() === "";
    newNameInput.disabled = locked;
    templateSelect.disabled = locked;
    // GitHubアップロードのトグルは一覧の各行にあるため、ロック中はCSSでまとめて操作不可にする
    listElement.classList.toggle("locked", locked);
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

        const githubToggle = document.createElement("label");
        githubToggle.className = "github-toggle";
        githubToggle.title = "GitHubへのプッシュに含める";
        // トグルの操作が行の選択・起動（クリック/ダブルクリック）に波及しないようにする
        githubToggle.addEventListener("click", (event) => event.stopPropagation());
        githubToggle.addEventListener("dblclick", (event) => event.stopPropagation());

        const githubCheckbox = document.createElement("input");
        githubCheckbox.type = "checkbox";
        githubCheckbox.checked = Boolean(project.includeInGithubPush);
        githubCheckbox.addEventListener("change", () => {
            setStatus("");
            send({ action: "setIncludeInGithubPush", name: project.name, include: githubCheckbox.checked });
        });

        const githubSlider = document.createElement("span");
        githubSlider.className = "slider";

        const githubLabel = document.createElement("span");
        githubLabel.className = "github-toggle-label";
        githubLabel.textContent = "GitHub";

        githubToggle.append(githubCheckbox, githubSlider, githubLabel);
        item.append(badge, info, githubToggle);

        item.addEventListener("click", () => selectProject(project.name));
        // ダブルクリックはそのまま起動（Unity Hubと同じ操作感）
        item.addEventListener("dblclick", () => {
            selectProject(project.name);
            requestOpen();
        });

        listElement.appendChild(item);
    }
}

function renderTemplates() {
    templateSelect.textContent = "";
    for (const template of templates) {
        const option = document.createElement("option");
        option.value = template.name;
        option.textContent = template.displayName;
        templateSelect.appendChild(option);
    }
    updateTemplateDescription();
}

function updateTemplateDescription() {
    const selected = templates.find((t) => t.name === templateSelect.value);
    templateDescription.textContent = selected ? selected.description : "";
}

//--------- 操作 ---------//
function requestOpen() {
    if (isBusy || isConfirming || selectedName === null) return;
    isBusy = true;
    updateButtons();
    setStatus("エディターを起動しています...", "busy");
    send({ action: "open", name: selectedName });
}

function requestReveal() {
    if (isBusy || isConfirming || selectedName === null) return;
    setStatus("");
    send({ action: "reveal", name: selectedName });
}

function requestCreate() {
    const name = newNameInput.value.trim();
    if (isBusy || isConfirming || name === "") return;
    setStatus("");
    send({ action: "create", name: name, template: templateSelect.value });
}

//--------- 削除の確認 ---------//
function openConfirmDialog() {
    const project = findProject(selectedName);
    if (isBusy || isConfirming || !project) return;

    confirmName.textContent = project.name;
    confirmPath.textContent = project.path;
    confirmPath.title = project.path;
    confirmOverlay.hidden = false;
    isConfirming = true;
    updateButtons();
    confirmCancelButton.focus();
}

function closeConfirmDialog() {
    confirmOverlay.hidden = true;
    isConfirming = false;
    updateButtons();
}

function confirmDelete() {
    const name = selectedName;
    closeConfirmDialog();
    if (!name) return;
    setStatus("");
    send({ action: "delete", name: name });
}

//--------- 入力 ---------//
openButton.addEventListener("click", requestOpen);
revealButton.addEventListener("click", requestReveal);
deleteButton.addEventListener("click", openConfirmDialog);
refreshButton.addEventListener("click", () => send({ action: "refresh" }));
createButton.addEventListener("click", requestCreate);
templateSelect.addEventListener("change", updateTemplateDescription);
newNameInput.addEventListener("input", updateButtons);
newNameInput.addEventListener("keydown", (event) => {
    if (event.key === "Enter") requestCreate();
});

confirmCancelButton.addEventListener("click", closeConfirmDialog);
confirmDeleteButton.addEventListener("click", confirmDelete);
// 背景をクリックした場合もキャンセル扱いにする（誤操作で削除しないよう、既定は常に取り消し側）
confirmOverlay.addEventListener("click", (event) => {
    if (event.target === confirmOverlay) closeConfirmDialog();
});

document.addEventListener("keydown", (event) => {
    if (isConfirming) {
        if (event.key === "Escape") closeConfirmDialog();
        return;
    }
    // 一覧に触っていなくてもEnterで起動できるようにする
    if (event.key === "Enter" && event.target !== newNameInput) requestOpen();
});

//--------- C++からの通知 ---------//
if (host) {
    host.addEventListener("message", (event) => {
        const message = event.data;
        if (message.type === "projects") {
            projects = message.items || [];

            // テンプレート一覧は毎回同じ内容なので、選択を保ったまま初回だけ組み立てる
            if (templates.length === 0 && Array.isArray(message.templates)) {
                templates = message.templates;
                renderTemplates();
            }

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
