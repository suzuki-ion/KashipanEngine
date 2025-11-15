#pragma once

// ログ出力（非同期キューへ積む）
void WriteLog(const std::string &formattedLine) {
    // ここでは IO を行わず、ワーカースレッドに委譲
    {
        std::lock_guard<std::mutex> lock(sLogMutex);
        sLogQueue.emplace_back(formattedLine);
    }
    sLogCv.notify_one();
}
