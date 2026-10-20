"""KashipanEngine CPU Profiler. Run with Python 3.10+ (Tkinter required)."""
import argparse
from collections import deque
from datetime import datetime
import queue
import time
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

from protocol import DEFAULT_PORT, MAX_SAMPLES, Receiver, load_capture, save_capture
from single_instance import InstanceGuard


class ProfilerApp:
    def __init__(self, root, port=DEFAULT_PORT, connect=True):
        self.root = root
        self.receiver = None
        self.samples = deque(maxlen=MAX_SAMPLES)
        self.session = None
        self.selected_process = ""
        self.sort_key, self.descending = "averageMs", True
        self.selection = 0
        self.paused = False
        self.last_received = None
        self.connection_status = "未接続"
        root.title("KashipanEngine · CPU Profiler")
        root.geometry("1160x820")
        root.minsize(860, 650)
        root.configure(bg="#111827")
        style = ttk.Style(root)
        style.theme_use("clam")
        style.configure(".", background="#111827", foreground="#e2e8f0", font=("Yu Gothic UI", 10))
        style.configure("TButton", background="#273449", padding=(12, 7))
        style.map("TButton", background=[("active", "#365373")])
        style.configure("TEntry", fieldbackground="#1e293b", insertcolor="#e2e8f0")
        style.configure("Treeview", background="#182235", fieldbackground="#182235", rowheight=27)
        style.configure("Treeview.Heading", background="#273449", padding=7)
        style.map("Treeview", background=[("selected", "#245b80")])
        outer = ttk.Frame(root, padding=20)
        outer.pack(fill="both", expand=True)
        ttk.Label(outer, text="KASHIPAN / CPU PROFILER", font=("Segoe UI", 19, "bold")).pack(anchor="w")
        ttk.Label(outer, text="CPU経過時間をリアルタイムに確認", foreground="#94a3b8").pack(anchor="w", pady=(0, 14))
        toolbar = ttk.Frame(outer)
        toolbar.pack(fill="x")
        ttk.Label(toolbar, text="127.0.0.1 :").pack(side="left")
        self.port = tk.StringVar(value=str(port))
        ttk.Entry(toolbar, textvariable=self.port, width=7).pack(side="left", padx=(6, 12))
        self.connect_button = ttk.Button(toolbar, text="接続", command=self.toggle_connection)
        self.connect_button.pack(side="left")
        self.pause_button = ttk.Button(toolbar, text="一時停止", command=self.toggle_pause)
        self.pause_button.pack(side="left", padx=6)
        ttk.Button(toolbar, text="履歴をクリア", command=self.clear).pack(side="left")
        ttk.Button(toolbar, text="読み込み", command=self.open_capture).pack(side="right")
        ttk.Button(toolbar, text="履歴を保存", command=self.save).pack(side="right", padx=6)
        self.status = tk.StringVar(value="未接続")
        ttk.Label(outer, textvariable=self.status, foreground="#7dd3fc").pack(anchor="w", pady=(12, 8))
        self.metrics = tk.StringVar(value="エンジンを起動して接続してください")
        ttk.Label(outer, textvariable=self.metrics, font=("Segoe UI", 15, "bold")).pack(anchor="w")
        self.chart = tk.Canvas(outer, height=195, bg="#0c1423", highlightthickness=0)
        self.chart.pack(fill="x", pady=(12, 4))
        self.chart.bind("<Configure>", lambda event: self.draw_chart())
        self.chart.bind("<Button-1>", self.chart_click)
        self.chart_label = tk.StringVar(value="青: フレーム時間 / 緑: 選択した処理 / 単位 ms")
        ttk.Label(outer, textvariable=self.chart_label, foreground="#94a3b8").pack(anchor="w")
        history = ttk.Frame(outer)
        history.pack(fill="x", pady=6)
        ttk.Label(history, text="過去のサンプル").pack(side="left")
        self.slider = ttk.Scale(history, from_=0, to=1, command=self.scrub)
        self.slider.pack(side="left", fill="x", expand=True, padx=12)
        self.position = tk.StringVar(value="0 / 0")
        ttk.Label(history, textvariable=self.position, width=13).pack(side="right")
        search = ttk.Frame(outer)
        search.pack(fill="x", pady=(2, 8))
        ttk.Label(search, text="処理名で検索").pack(side="left")
        self.filter = tk.StringVar()
        ttk.Entry(search, textvariable=self.filter, width=38).pack(side="left", padx=10)
        self.filter.trace_add("write", lambda *args: self.draw_table())
        self.details = tk.StringVar()
        ttk.Label(search, textvariable=self.details, foreground="#94a3b8").pack(side="right")
        table_frame = ttk.Frame(outer)
        table_frame.pack(fill="both", expand=True)
        columns = ("name", "milliseconds", "averageMs", "peakMs", "calls")
        self.table = ttk.Treeview(table_frame, columns=columns, show="headings", selectmode="browse")
        for key, title in zip(columns, ("処理名", "直近 ms", "平均 ms", "最大 ms", "呼び出し回数")):
            self.table.heading(key, text=title, command=lambda key=key: self.sort(key))
            self.table.column(key, width=440 if key == "name" else 115, minwidth=75,
                              anchor="w" if key == "name" else "e", stretch=key == "name")
        scrollbar = ttk.Scrollbar(table_frame, orient="vertical", command=self.table.yview)
        self.table.configure(yscrollcommand=scrollbar.set)
        self.table.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")
        self.table.bind("<<TreeviewSelect>>", self.select_process)
        ttk.Label(outer, text="待機を含むCPU経過時間です。親の時間は子を含み、行の合計はフレーム時間と一致しません。",
                  foreground="#94a3b8").pack(anchor="w", pady=(12, 0))
        ttk.Label(outer, text="約10回/秒のサンプリング · 最大600件を保持 · 全フレームの記録やGPU計測は行いません。",
                  foreground="#94a3b8").pack(anchor="w")
        root.protocol("WM_DELETE_WINDOW", self.close)
        if connect:
            self.toggle_connection()
        self.poll_token = root.after(100, self.poll)

    def toggle_connection(self):
        if self.receiver:
            self.disconnect()
            return
        try:
            port = int(self.port.get())
            if not 1 <= port <= 65535:
                raise ValueError()
        except ValueError:
            messagebox.showerror("接続", "ポートは1〜65535で指定してください", parent=self.root)
            return
        self.paused = False
        self.pause_button.configure(text="一時停止")
        self.receiver = Receiver(port)
        self.receiver.start()
        self.connect_button.configure(text="切断")
        self.connection_status = "接続待ち"

    def disconnect(self):
        if self.receiver:
            self.receiver.stop()
            self.receiver = None
        self.connection_status = "切断 / 履歴を表示中"
        self.connect_button.configure(text="接続")

    def clear(self):
        self.samples.clear()
        self.selected_process = ""
        self.selection = 0
        self.render()

    def toggle_pause(self):
        self.paused = not self.paused
        self.pause_button.configure(text="ライブに戻る" if self.paused else "一時停止")
        if not self.paused:
            self.selection = max(0, len(self.samples) - 1)
        self.render()

    def poll(self):
        changed = False
        if self.receiver:
            for _ in range(64):
                try:
                    kind, value = self.receiver.events.get_nowait()
                except queue.Empty:
                    break
                if kind == "status":
                    self.connection_status = value
                else:
                    self.last_received = time.monotonic()
                    if self.paused:
                        continue  # Freeze both the view and the bounded history.
                    if value["session"] != self.session:
                        self.clear()
                        self.session = value["session"]
                    self.samples.append(value)
                    changed = True
            if changed:
                self.selection = len(self.samples) - 1
                self.render()
        text = self.connection_status
        if self.receiver and self.last_received is not None:
            age = time.monotonic() - self.last_received
            if age > 1.5:
                text += f" / 新しい完了フレームなし {age:.0f}秒"
            if self.receiver.dropped:
                text += f" / 受信キュー超過 {self.receiver.dropped}件"
        if self.paused:
            text += " / 履歴の収集を停止中"
        self.status.set(text)
        self.poll_token = self.root.after(100, self.poll)

    def current(self):
        return self.samples[min(self.selection, len(self.samples) - 1)] if self.samples else None

    def render(self):
        count = len(self.samples)
        self.slider.configure(to=max(1, count - 1))
        # ttk.Scale.set invokes its callback. Update the value without scrubbing.
        self.slider.configure(command="")
        self.slider.set(self.selection)
        self.slider.configure(command=self.scrub)
        self.position.set(f"{self.selection + 1 if count else 0} / {count}")
        sample = self.current()
        if sample:
            stamp = datetime.fromtimestamp(sample["receivedAt"]).strftime("%H:%M:%S.%f")[:-3]
            self.metrics.set(f'{sample["fps"]:.1f} FPS     {sample["frameMs"]:.3f} ms     '
                             f'Frame #{sample["frameId"]}     {stamp}')
            self.details.set(f'PID {sample["pid"]} / 集計 {sample["frames"]}フレーム')
        else:
            self.metrics.set("計測データを待っています")
            self.details.set("")
        self.draw_table()
        self.draw_chart()

    def scrub(self, value):
        if not self.samples:
            return
        self.paused = True
        self.pause_button.configure(text="ライブに戻る")
        self.selection = min(len(self.samples) - 1, max(0, round(float(value))))
        self.render()

    def chart_click(self, event):
        width = max(1, self.chart.winfo_width() - 70)
        self.scrub((event.x - 50) / width * max(0, len(self.samples) - 1))

    def sort(self, key):
        self.descending = not self.descending if key == self.sort_key else key != "name"
        self.sort_key = key
        self.draw_table()

    def draw_table(self):
        if not hasattr(self, "table"):
            return
        sample = self.current()
        rows = sample["results"] if sample else []
        search = self.filter.get().casefold()
        rows = sorted((row for row in rows if search in row["name"].casefold()),
                      key=lambda row: row[self.sort_key], reverse=self.descending)
        scroll = self.table.yview()[0]
        self.table.delete(*self.table.get_children())
        for index, row in enumerate(rows):
            key = str(index)
            self.table.insert("", "end", iid=key, values=(row["name"],
                f'{row["milliseconds"]:.3f}', f'{row["averageMs"]:.3f}',
                f'{row["peakMs"]:.3f}', row["calls"]))
            if row["name"] == self.selected_process:
                self.table.selection_set(key)
        self.table.yview_moveto(scroll)

    def select_process(self, event):
        selection = self.table.selection()
        if selection:
            self.selected_process = self.table.item(selection[0], "values")[0]
            self.draw_chart()

    def draw_chart(self):
        canvas = self.chart
        canvas.delete("all")
        width, height = max(100, canvas.winfo_width()), max(100, canvas.winfo_height())
        left, right, top, bottom = 50, width - 20, 18, height - 28
        samples = list(self.samples)
        frame_values = [sample["frameMs"] for sample in samples]
        process_values = [next((row["milliseconds"] for row in sample["results"]
                               if row["name"] == self.selected_process), 0) for sample in samples]
        maximum = max([20, *frame_values, *process_values]) * 1.12
        def y(value):
            return bottom - value / maximum * (bottom - top)
        for index in range(5):
            value = maximum * index / 4
            canvas.create_line(left, y(value), right, y(value), fill="#243145")
            canvas.create_text(left - 8, y(value), text=f"{value:.1f}", anchor="e", fill="#94a3b8")
        canvas.create_line(left, y(1000 / 60), right, y(1000 / 60), fill="#715c30", dash=(4, 5))
        canvas.create_text(right, y(1000 / 60) - 8, text="60 FPS / 16.67 ms", anchor="e", fill="#c4a365")
        if samples:
            def x(index):
                return left + index / max(1, len(samples) - 1) * (right - left)
            for values, color in ((frame_values, "#38bdf8"), (process_values, "#34d399")):
                points = [coordinate for index, value in enumerate(values) for coordinate in (x(index), y(value))]
                if len(points) >= 4:
                    canvas.create_line(*points, fill=color, width=2)
                else:
                    canvas.create_oval(points[0]-2, points[1]-2, points[0]+2, points[1]+2, fill=color)
            canvas.create_line(x(self.selection), top, x(self.selection), bottom, fill="#e2e8f0", dash=(2, 4))
            elapsed = samples[-1]["receivedAt"] - samples[0]["receivedAt"]
            canvas.create_text(left, height - 12, text=f"受信履歴 {elapsed:.1f}秒 / クリックして選択",
                               anchor="w", fill="#94a3b8")
        else:
            canvas.create_text(width/2, height/2, text="接続するとフレーム時間が表示されます", fill="#94a3b8")
        self.chart_label.set("青: フレーム時間 / 緑: " + (self.selected_process or "一覧で処理を選択") + " / 単位 ms")

    def save(self):
        if not self.samples:
            messagebox.showinfo("保存", "保存できる履歴がありません", parent=self.root)
            return
        path = filedialog.asksaveasfilename(parent=self.root, defaultextension=".jsonl",
            initialfile="cpu-" + datetime.now().strftime("%Y%m%d-%H%M%S") + ".jsonl",
            filetypes=[("CPU計測記録", "*.jsonl")])
        if path:
            try:
                save_capture(path, list(self.samples))
            except (OSError, ValueError) as error:
                messagebox.showerror("保存できません", str(error), parent=self.root)

    def open_capture(self):
        path = filedialog.askopenfilename(parent=self.root, filetypes=[("CPU計測記録", "*.jsonl")])
        if not path:
            return
        try:
            samples = load_capture(path)
        except (OSError, ValueError, UnicodeError, RecursionError) as error:
            messagebox.showerror("読み込めません", str(error), parent=self.root)
            return
        self.disconnect()
        self.samples = deque(samples, maxlen=MAX_SAMPLES)
        self.selection = len(samples) - 1
        self.paused = True
        self.pause_button.configure(text="ライブに戻る")
        self.connection_status = "保存済みの記録を表示中"
        self.render()

    def close(self):
        self.root.after_cancel(self.poll_token)
        self.disconnect()
        self.root.destroy()


def main():
    parser = argparse.ArgumentParser(description="KashipanEngine CPU Profiler")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--offline", action="store_true", help="接続せずに起動")
    args = parser.parse_args()
    with InstanceGuard(args.port) as instance:
        root = tk.Tk()
        if not instance.acquired:
            root.withdraw()
            messagebox.showinfo("CPU Profiler", "このポートのプロファイラーは起動済みです。タスクバーから開いてください。", parent=root)
            root.destroy()
            return
        ProfilerApp(root, args.port, not args.offline)
        root.mainloop()


if __name__ == "__main__":
    main()
