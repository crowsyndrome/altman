#define _CRT_SECURE_NO_WARNINGS

#include <unordered_set>
#include <filesystem>
#include <imgui.h>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <utility>
#include <algorithm>

#include "history.h"
#include "log_types.h"
#include "log_parser.h"
#include "history_utils.h"

#include "../../utils/threading.h"
#include "../../utils/launcher.hpp"
#include "../../utils/modal_popup.h"
#include "../../utils/status.h"
#include "../../ui.h"
#include "../data.h"
#include "../../utils/splitter.h"

namespace fs = filesystem;
using namespace ImGui;
using namespace std;

static int g_selected_log_idx = -1;

static vector<LogInfo> g_logs;
static atomic_bool g_logs_loading{false};
static atomic_bool g_stop_log_watcher{false};
static once_flag g_start_log_watcher_once;
static mutex g_logs_mtx;
static float s_historyListWidth = 0.0f; // Initialized on first use

static void refreshLogs() {
    if (g_logs_loading.load())
        return;

    g_logs_loading = true;
    Threading::newThread([]() {
        LOG_INFO("Scanning Roblox logs folder...");
        vector<LogInfo> tempLogs;
        string dir = logsFolder();
        if (!dir.empty()) {
            for (const auto &entry: fs::directory_iterator(dir)) {
                if (entry.is_regular_file()) {
                    string fName = entry.path().filename().string();
                    if (fName.length() > 4 && fName.substr(fName.length() - 4) == ".log") {
                        LogInfo logInfo;
                        logInfo.fileName = fName;
                        logInfo.fullPath = entry.path().string();
                        parseLogFile(logInfo);
                        if (!logInfo.timestamp.empty() || !logInfo.version.empty()) {
                            tempLogs.push_back(logInfo);
                        }
                    }
                }
            }
        }

        sort(tempLogs.begin(), tempLogs.end(), [](const LogInfo &a, const LogInfo &b) {
            return b.timestamp < a.timestamp;
        }); {
            lock_guard<mutex> lk(g_logs_mtx);
            g_logs = move(tempLogs);
            g_selected_log_idx = -1;
        }

        LOG_INFO("Log scan complete.");
        g_logs_loading = false;
    });
}

static void workerScan() {
    string dir = logsFolder();
    if (dir.empty())
        return;

    vector<LogInfo> tempLogs;
    unordered_set<string> seenFiles; {
        lock_guard<mutex> lk(g_logs_mtx);
        for (const auto &log: g_logs)
            seenFiles.insert(log.fileName);
    }

    for (const auto &entry: fs::directory_iterator(dir)) {
        if (g_stop_log_watcher.load())
            return;
        if (entry.is_regular_file()) {
            string fName = entry.path().filename().string();
            if (fName.rfind(".log", fName.length() - 4) != string::npos) {
                if (seenFiles.contains(fName))
                    continue;

                LogInfo logInfo;
                logInfo.fileName = fName;
                logInfo.fullPath = entry.path().string();
                parseLogFile(logInfo);
                if (!logInfo.timestamp.empty() || !logInfo.version.empty()) {
                    tempLogs.push_back(logInfo);
                }
            }
        }
    }

    if (!tempLogs.empty()) {
        lock_guard<mutex> lk(g_logs_mtx);
        g_logs.insert(g_logs.end(), tempLogs.begin(), tempLogs.end());
        sort(g_logs.begin(), g_logs.end(), [](const LogInfo &a, const LogInfo &b) {
            return b.timestamp < a.timestamp;
        });
    }

    refreshLogs();
}

static void startLogWatcher() {
    refreshLogs();
}

static void DisplayOptionalText(const char *label, const string &value) {
    if (!value.empty()) {
        PushID(label);
        Text("%s: %s", label, value.c_str());
        if (BeginPopupContextItem("CopyHistoryValue")) {
            if (MenuItem("Copy")) {
                SetClipboardText(value.c_str());
            }
            EndPopup();
        }
        PopID();
    }
}

static void DisplayLogDetails(const LogInfo &logInfo) {
    float desiredTextIndent = 8.0f;

    ImGuiTableFlags tableFlags = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
                                 ImGuiTableFlags_SizingFixedFit;

    PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0.0f, 4.0f));
    if (BeginTable("HistoryInfoTable", 2, tableFlags)) {
        TableSetupColumn("##historylabel", ImGuiTableColumnFlags_WidthFixed, 110.f);
        TableSetupColumn("##historyvalue", ImGuiTableColumnFlags_WidthStretch);

        auto addRow = [&](const char *label, const string &value) {
            if (value.empty())
                return;
            TableNextRow();
            TableSetColumnIndex(0);
            Indent(desiredTextIndent);
            Spacing();
            TextUnformatted(label);
            Spacing();
            Unindent(desiredTextIndent);

            TableSetColumnIndex(1);
            Indent(desiredTextIndent);
            Spacing();
            PushID(label);
            TextWrapped("%s", value.c_str());
            if (BeginPopupContextItem("CopyHistoryValue")) {
                if (MenuItem("Copy")) {
                    SetClipboardText(value.c_str());
                }
                EndPopup();
            }
            PopID();
            Spacing();
            Unindent(desiredTextIndent);
        };

        addRow("File:", logInfo.fileName);

        string timeStr = friendlyTimestamp(logInfo.timestamp);
        addRow("Time:", timeStr);
        addRow("Version:", logInfo.version);
        addRow("Channel:", logInfo.channel);
        addRow("Place ID:", logInfo.placeId);
        addRow("Job ID:", logInfo.jobId);
        addRow("Universe ID:", logInfo.universeId);
        if (!logInfo.serverIp.empty()) {
            string serverStr = logInfo.serverIp + ":" + logInfo.serverPort;
            addRow("Server:", serverStr);
        }
        addRow("User ID:", logInfo.userId);

        EndTable();
    }
    PopStyleVar();
}

void RenderHistoryTab() {
    call_once(g_start_log_watcher_once, startLogWatcher);

    if (Button("Refresh Logs")) {
        refreshLogs();
    }
    SameLine();
    if (g_logs_loading.load()) {
        TextUnformatted("Loading...");
    }

    Separator();

    float splitterThickness = 4.0f;
    ImGuiStyle &style = GetStyle();
    if (s_historyListWidth <= 0.0f)
        s_historyListWidth = GetContentRegionAvail().x * 0.4f;
    float availWidth = GetContentRegionAvail().x;
    float detailWidth = availWidth - s_historyListWidth - splitterThickness - style.ItemSpacing.x;
    if (detailWidth < 100.0f)
        detailWidth = 100.0f;
    if (s_historyListWidth < 100.0f)
        s_historyListWidth = 100.0f;

    float listWidth = s_historyListWidth;

    BeginChild("##HistoryList", ImVec2(listWidth, 0), true); {
        lock_guard<mutex> lk(g_logs_mtx);
        string lastDay;
        string lastVersion;
        bool indented = false;

        for (int i = 0; i < static_cast<int>(g_logs.size()); ++i) {
            const auto &logInfo = g_logs[i];

            string thisDay = logInfo.timestamp.size() >= 10 ? logInfo.timestamp.substr(0, 10) : "Unknown";
            string thisVersion = logInfo.version.empty() ? "" : logInfo.version;

            if (thisDay != lastDay || thisVersion != lastVersion) {
                if (indented)
                    Unindent();
                string header = thisDay;
                if (!thisVersion.empty())
                    header += "  v" + thisVersion;
                else
                    header += "  Unknown Version";
                SeparatorText(header.c_str());
                Indent();
                indented = true;
                lastDay = thisDay;
                lastVersion = thisVersion;
            }

            PushID(i);
            if (Selectable(niceLabel(logInfo).c_str(), g_selected_log_idx == i))
                g_selected_log_idx = i;
            PopID();
        }

        if (indented)
            Unindent();
    }
    EndChild();
    SameLine();
    Splitter(true, splitterThickness, &s_historyListWidth, &detailWidth, 100.0f, 100.0f);
    SameLine();

    float desiredTextIndent = 8.0f;

    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    BeginChild("##HistoryDetails", ImVec2(detailWidth, 0), true);
    PopStyleVar();
    if (g_selected_log_idx >= 0) {
        lock_guard<mutex> lk(g_logs_mtx);
        if (g_selected_log_idx < static_cast<int>(g_logs.size())) {
            const auto &logInfo = g_logs[g_selected_log_idx];
            DisplayLogDetails(logInfo);

            Separator();

            Indent(desiredTextIndent / 2);
            if (Button("Launch this game session")) {
                if (!logInfo.placeId.empty() && !g_selectedAccountIds.empty()) {
                    uint64_t place_id_val = 0;
                    try {
                        place_id_val = stoull(logInfo.placeId);
                    } catch (...) {
                    }

                    if (place_id_val > 0) {
                        vector<pair<int, string>> accounts;
                        for (int id : g_selectedAccountIds) {
                            auto it = find_if(g_accounts.begin(), g_accounts.end(),
                                             [&](const AccountData &a) { return a.id == id; });
                            if (it != g_accounts.end())
                                accounts.emplace_back(it->id, it->cookie);
                        }
                        if (!accounts.empty()) {
                            LOG_INFO("Launching game from history...");
                            thread([place_id_val, jobId = logInfo.jobId, accounts]() {
                                      launchRobloxSequential(place_id_val, jobId, accounts);
                                  })
                                .detach();
                        } else {
                            LOG_INFO("Selected account not found.");
                        }
                    } else {
                        LOG_INFO("Invalid Place ID in log.");
                    }
                } else {
                    LOG_INFO("Place ID missing or no account selected.");
                    if (g_selectedAccountIds.empty()) {
                        Status::Error("No account selected to open log entry.");
                        ModalPopup::Add("Select an account first.");
                    } else {
                        Status::Error("Invalid log entry.");
                        ModalPopup::Add("Invalid log entry.");
                    }
                }
            }

            Separator();
            TextUnformatted("Raw Log Output:");
            BeginChild("##LogOutputScroll", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
            for (const auto &line: logInfo.outputLines) {
                TextUnformatted(line.c_str());
            }
            EndChild();
            Unindent(desiredTextIndent / 2);
        }
    } else {
        Indent(desiredTextIndent);
        TextWrapped("Select a log from the list to see details or launch the session.");
        Unindent(desiredTextIndent);
    }
    EndChild();
}
