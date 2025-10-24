#pragma once

// スコープ "Entering" の遅延出力をフラッシュ
void FlushPendingScopeEnters() {
    if (sScopeFrames.empty()) return;
    size_t first = 0;
    while (first < sScopeFrames.size() && sScopeFrames[first].enteredFlushed) ++first;
    for (size_t i = first; i < sScopeFrames.size(); ++i) {
        auto &frame = sScopeFrames[i];
        WriteLog(BuildLogLine(&frame, LogSeverity::Debug, "Entering scope."));
        frame.enteredFlushed = true;
    }
}
