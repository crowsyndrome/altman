#include "accounts_context_menu.h"
#include "accounts_join_ui.h"
#include "imgui_internal.h"
#include "accounts.h"
#include <imgui.h>
#include <random>
#include <string>
#include <vector>
#include <set>
#include <unordered_map>

#include "../../utils/roblox_api.h"
#include "../components.h"
#include "../../utils/time_utils.h"
#include "../../utils/logging.hpp"
#include "../../utils/status.h"
#include "../../ui.h"
#include "../data.h"
#include "../../utils/threading.h"

using namespace ImGui;

void RenderAccountsTable(vector<AccountData> &accounts_to_display, const char *table_id, float table_height) {
    constexpr int column_count = 6;
    ImGuiTableFlags table_flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_ScrollY | ImGuiTableFlags_Hideable | ImGuiTableFlags_Reorderable |
                                  ImGuiTableFlags_ContextMenuInBody;

    if (g_selectedAccountIds.empty() && g_defaultAccountId != -1) {
        g_selectedAccountIds.insert(g_defaultAccountId);
    }

    if (BeginTable(table_id, column_count, table_flags, ImVec2(0.0f, table_height > 0 ? table_height - 2.0f : 0.0f))) {
        TableSetupColumn("Display Name", ImGuiTableColumnFlags_WidthStretch);
        TableSetupColumn("Username", ImGuiTableColumnFlags_WidthStretch);
        TableSetupColumn("UserID", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        TableSetupColumn("Voice", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultHide, 100.0f);
        TableSetupColumn("Note", ImGuiTableColumnFlags_WidthStretch);
        TableSetupScrollFreeze(0, 1);

        TableNextRow(ImGuiTableRowFlags_Headers);
        TableNextColumn();
        TextUnformatted("Display Name");
        TableNextColumn();
        TextUnformatted("Username");
        TableNextColumn();
        TextUnformatted("UserID");
        TableNextColumn();
        TextUnformatted("Status");
        TableNextColumn();
        TextUnformatted("Voice");
        TableNextColumn();
        TextUnformatted("Note");

        for (auto &account: accounts_to_display) {
            TableNextRow();
            PushID(account.id);

            bool is_row_selected = g_selectedAccountIds.contains(account.id);

            if (is_row_selected) {
                TableSetBgColor(ImGuiTableBgTarget_RowBg0, GetColorU32(ImGuiCol_Header));
            }

            float row_interaction_height = GetFrameHeight();
            if (row_interaction_height <= 0)
                row_interaction_height = GetTextLineHeightWithSpacing();
            if (row_interaction_height <= 0)
                row_interaction_height = 19.0f;

            float text_visual_height = GetTextLineHeight();
            float vertical_padding = (row_interaction_height - text_visual_height) * 0.5f;
            vertical_padding = ImMax(0.0f, vertical_padding);

            TableNextColumn();
            float cell_content_start_y = GetCursorPosY();

            char selectable_label[64];
            snprintf(selectable_label, sizeof(selectable_label), "##row_selectable_%d", account.id);

            if (Selectable(
                selectable_label,
                is_row_selected,
                ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap,
                ImVec2(0, row_interaction_height))) {
                if (GetIO().KeyCtrl) {
                    if (is_row_selected) g_selectedAccountIds.erase(account.id);
                    else g_selectedAccountIds.insert(account.id);
                } else {
                    bool was_already_solely_selected = (is_row_selected && g_selectedAccountIds.size() == 1);
                    g_selectedAccountIds.clear();
                    if (!was_already_solely_selected) g_selectedAccountIds.insert(account.id);
                }
            }

            static std::unordered_map<int, double> holdStartTimes;
            if (IsItemActivated() && IsMouseDown(ImGuiMouseButton_Left)) {
                holdStartTimes[account.id] = ImGui::GetTime();
            }
            if (IsItemActive()) {
                auto it = holdStartTimes.find(account.id);
                if (it != holdStartTimes.end() && (ImGui::GetTime() - it->second) >= 0.65f) {
                    holdStartTimes.erase(it);
                    if (!account.cookie.empty()) {
                        LOG_INFO(
                            "Opening browser for account: " + account.displayName + " (ID: " + std::to_string(account.id
                            ) + ")");
                        Threading::newThread([acc = account]() { LaunchBrowserWithCookie(acc); });
                    } else {
                        LOG_WARN("Cannot open browser - cookie is empty for account: " + account.displayName);
                        Status::Error("Cookie is empty for this account");
                    }
                }
            } else {
                holdStartTimes.erase(account.id);
            }

            string context_menu_id = string(table_id) + "_ContextMenu_" + to_string(account.id);

            RenderAccountContextMenu(account, context_menu_id);

            SetItemAllowOverlap();

            SetCursorPosY(cell_content_start_y + vertical_padding);
            TextUnformatted(account.displayName.c_str());

            SetCursorPosY(cell_content_start_y + row_interaction_height);

            auto render_centered_text_in_cell = [&](const char *text, ImVec4 *color = nullptr) {
                TableNextColumn();
                float current_cell_start_y = GetCursorPosY();

                SetCursorPosY(current_cell_start_y + vertical_padding);
                if (color)
                    TextColored(*color, "%s", text);
                else
                    TextUnformatted(text);

                SetCursorPosY(current_cell_start_y + row_interaction_height);
            };

            render_centered_text_in_cell(account.username.c_str());
            render_centered_text_in_cell(account.userId.c_str());

            ImVec4 statusColor = getStatusColor(account.status);
            render_centered_text_in_cell(account.status.c_str(), &statusColor);

            TableNextColumn();
            float voice_y = GetCursorPosY();
            SetCursorPosY(voice_y + vertical_padding);
            TextUnformatted(account.voiceStatus.c_str());
            if (account.voiceStatus == "Banned" && account.voiceBanExpiry > 0 && IsItemHovered()) {
                BeginTooltip();
                string timeStr = formatRelativeFuture(account.voiceBanExpiry);
                TextUnformatted(timeStr.c_str());
                EndTooltip();
            }
            SetCursorPosY(voice_y + row_interaction_height);

            render_centered_text_in_cell(account.note.c_str());

            PopID();
        }
        EndTable();
    }
}

void RenderFullAccountsTabContent() {
    float availH = GetContentRegionAvail().y;
    ImGuiStyle &style = GetStyle();

    float join_options_section_height = 0;

    join_options_section_height += GetTextLineHeight() + style.ItemSpacing.y;
    join_options_section_height += GetFrameHeight() + style.ItemSpacing.y;
    if (join_type_combo_index == 1) {
        join_options_section_height += GetFrameHeight() + style.ItemSpacing.y;
        join_options_section_height += GetFrameHeight() + style.ItemSpacing.y;
    } else {
        join_options_section_height += GetFrameHeight() + style.ItemSpacing.y;
    }
    join_options_section_height += 1.0f + style.ItemSpacing.y;
    join_options_section_height += GetFrameHeight() + style.ItemSpacing.y;
    join_options_section_height += style.ItemSpacing.y;

    float separator_height_after_table = 1.0f + style.ItemSpacing.y;

    float total_height_for_join_ui_and_sep = separator_height_after_table + join_options_section_height;

    float tableH = ImMax(GetFrameHeight() * 3.0f, availH - total_height_for_join_ui_and_sep);
    if (tableH < GetFrameHeight() * 3.0f)
        tableH = GetFrameHeight() * 3.0f;
    if (availH <= total_height_for_join_ui_and_sep)
        tableH = GetFrameHeight() * 3.0f;

    RenderAccountsTable(g_accounts, "AccountsTable", tableH);

    Separator();
    RenderJoinOptions();
}
