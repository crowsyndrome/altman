#define _CRT_SECURE_NO_WARNINGS
#include <unordered_map>
#include <unordered_set>
#include "games_utils.h"
#include "games.h"
#include <imgui.h>
#include <vector>
#include <string>
#include <algorithm>
#include <thread>
#include <utility>

#include "../components.h"
#include "../../utils/launcher.hpp"
#include "../../utils/roblox_api.h"
#include "../../utils/status.h"
#include "../../utils/modal_popup.h"
#include "../../ui.h"
#include "../servers/servers_utils.h"
#include "../../utils/splitter.h"

using namespace ImGui;
using namespace std;

static char searchBuffer[64] = "";
static int selectedIndex = -1;
static vector<GameInfo> gamesList;
static unordered_map<uint64_t, RobloxApi::GameDetail> gameDetailCache;

static unordered_set<uint64_t> favoriteGameIds;
static vector<GameInfo> favoriteGamesList;
static bool hasLoadedFavorites = false;
static char renameBuffer[128] = "";
static uint64_t renamingUniverseId = 0;
static float s_gamesListWidth = 300.0f;

static void RenderGameSearch();

static void RenderFavoritesList(float listWidth, float availableHeight);

static void RenderSearchResultsList(float listWidth, float availableHeight);

static void RenderGameDetailsPanel(float panelWidth, float availableHeight);

static void RenderGameSearch() {
    InputTextWithHint("##game_search", "Search games", searchBuffer, sizeof(searchBuffer));
    SameLine();
    if (Button(" Search  \xEF\x80\x82 ") && searchBuffer[0] != '\0') {
        selectedIndex = -1;
        gamesList = RobloxApi::searchGames(searchBuffer);
        erase_if(gamesList, [&](const GameInfo &g) {
            return favoriteGameIds.contains(g.universeId);
        });
        gameDetailCache.clear();
    }
}

static void RenderFavoritesList(float listWidth, float availableHeight) {
    if (!favoriteGamesList.empty()) {
        for (int index = 0; index < static_cast<int>(favoriteGamesList.size()); ++index) {
            const auto &game = favoriteGamesList[index];
            if (searchBuffer[0] != '\0' && !containsCI(game.name, searchBuffer))
                continue;
            PushID(("fav" + to_string(game.universeId)).c_str());
            TextUnformatted("\xEF\x80\x85");
            SameLine();
            if (Selectable(game.name.c_str(), selectedIndex == -1000 - index)) {
                selectedIndex = -1000 - index;
            }

            if (BeginPopupContextItem("FavoriteContext")) {
                if (MenuItem("Unfavorite")) {
                    uint64_t universeIdToRemove = game.universeId;
                    favoriteGameIds.erase(universeIdToRemove);
                    erase_if(favoriteGamesList,
                             [&](const GameInfo &gameInfo) {
                                 return gameInfo.universeId == universeIdToRemove;
                             });

                    if (selectedIndex == -1000 - index)
                        selectedIndex = -1;

                    erase_if(g_favorites,
                             [&](const FavoriteGame &favoriteGame) {
                                 return favoriteGame.universeId == universeIdToRemove;
                             });
                    Data::SaveFavorites();
                    CloseCurrentPopup();
                }

                if (BeginMenu("Rename")) {
                    if (renamingUniverseId != game.universeId) {
                        strncpy(renameBuffer, game.name.c_str(), sizeof(renameBuffer) - 1);
                        renameBuffer[sizeof(renameBuffer) - 1] = '\0';
                        renamingUniverseId = game.universeId;
                    }

                    InputText("##RenameFavorite", renameBuffer, sizeof(renameBuffer));

                    if (Button("Save##RenameFavorite")) {
                        if (renamingUniverseId == game.universeId) {
                            favoriteGamesList[index].name = renameBuffer;
                            for (auto &f: g_favorites) {
                                if (f.universeId == game.universeId) {
                                    f.name = renameBuffer;
                                    break;
                                }
                            }
                            Data::SaveFavorites();
                        }
                        renamingUniverseId = 0;
                        CloseCurrentPopup();
                    }
                    SameLine();
                    if (Button("Cancel##RenameFavorite")) {
                        renamingUniverseId = 0;
                        CloseCurrentPopup();
                    }
                    ImGui::EndMenu();
                }
                EndPopup();
            }
            PopID();
        }
    }
}

static void RenderSearchResultsList(float listWidth, float availableHeight) {
    for (int index = 0; index < static_cast<int>(gamesList.size()); ++index) {
        const auto &game = gamesList[index];
        if (favoriteGameIds.contains(game.universeId))
            continue;
        PushID(static_cast<int>(game.universeId));

        if (Selectable(game.name.c_str(), selectedIndex == index)) {
            selectedIndex = index;
        }

        if (IsItemHovered())
            SetTooltip("Players: %s", formatWithCommas(game.playerCount).c_str());

        if (BeginPopupContextItem("GameContext")) {
            if (MenuItem("Favorite") && !favoriteGameIds.contains(game.universeId)) {
                favoriteGameIds.insert(game.universeId);
                GameInfo favoriteGameInfo = game;
                favoriteGamesList.insert(favoriteGamesList.begin(), favoriteGameInfo);

                FavoriteGame favoriteGameData{game.name, game.universeId, game.placeId};
                g_favorites.push_back(favoriteGameData);
                Data::SaveFavorites();
                CloseCurrentPopup();
            }
            EndPopup();
        }
        PopID();
    }
}

void RenderGamesTab() {
    if (!hasLoadedFavorites) {
        Data::LoadFavorites();
        for (const auto &favoriteData: g_favorites) {
            favoriteGameIds.insert(favoriteData.universeId);
            GameInfo favoriteGameInfo{};
            favoriteGameInfo.name = favoriteData.name;
            favoriteGameInfo.placeId = favoriteData.placeId;
            favoriteGameInfo.universeId = favoriteData.universeId;
            favoriteGameInfo.playerCount = 0;
            favoriteGamesList.push_back(favoriteGameInfo);
        }
        hasLoadedFavorites = true;
    }

    RenderGameSearch();

    float splitterThickness = 4.0f;
    ImGuiStyle &style = GetStyle();
    float availableHeight = GetContentRegionAvail().y;
    float availableWidth = GetContentRegionAvail().x;
    float detailWidth = availableWidth - s_gamesListWidth - splitterThickness - style.ItemSpacing.x;
    if (detailWidth < 100.0f)
        detailWidth = 100.0f;
    if (s_gamesListWidth < 100.0f)
        s_gamesListWidth = 100.0f;

    BeginChild("##GamesList", ImVec2(s_gamesListWidth, availableHeight), true);
    RenderFavoritesList(s_gamesListWidth, availableHeight);
    if (!favoriteGamesList.empty() && !gamesList.empty() && any_of(gamesList.begin(), gamesList.end(),
                                                                   [&](const GameInfo &gameInfo) {
                                                                       return !favoriteGameIds.contains(
                                                                           gameInfo.universeId);
                                                                   })) {
        Separator();
    }
    RenderSearchResultsList(s_gamesListWidth, availableHeight);
    EndChild();
    SameLine();
    Splitter(true, splitterThickness, &s_gamesListWidth, &detailWidth, 100.0f, 100.0f);
    SameLine();

    RenderGameDetailsPanel(detailWidth, availableHeight);
}

static void RenderGameDetailsPanel(float panelWidth, float availableHeight) {
    float desiredTextIndent = 8.0f;

    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    BeginChild("##GameDetails", ImVec2(panelWidth, availableHeight), true);
    PopStyleVar();

    const GameInfo *currentGameInfo = nullptr;
    uint64_t currentUniverseId = 0;

    if (selectedIndex <= -1000) {
        int favoriteIndex = -1000 - selectedIndex;
        if (favoriteIndex >= 0 && favoriteIndex < static_cast<int>(favoriteGamesList.size())) {
            currentGameInfo = &favoriteGamesList[favoriteIndex];
            currentUniverseId = currentGameInfo->universeId;
        }
    } else if (selectedIndex >= 0 && selectedIndex < static_cast<int>(gamesList.size())) {
        currentGameInfo = &gamesList[selectedIndex];
        currentUniverseId = currentGameInfo->universeId;
    }

    if (currentGameInfo) {
        const GameInfo &gameInfo = *currentGameInfo;
        RobloxApi::GameDetail detailInfo;
        auto cacheIterator = gameDetailCache.find(currentUniverseId);
        if (cacheIterator == gameDetailCache.end()) {
            if (currentUniverseId != 0) {
                detailInfo = RobloxApi::getGameDetail(currentUniverseId);
                gameDetailCache[currentUniverseId] = detailInfo;
            }
        } else {
            detailInfo = cacheIterator->second;
        }

        int serverCount = detailInfo.maxPlayers > 0
                              ? static_cast<int>(
                                  ceil(static_cast<double>(gameInfo.playerCount) / detailInfo.maxPlayers))
                              : 0;

        ImGuiTableFlags tableFlags = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
                                     ImGuiTableFlags_SizingFixedFit;

        PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0.0f, 4.0f));
        if (BeginTable("GameInfoTable", 2, tableFlags)) {
            TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, 110.f);
            TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);

            auto addRow = [&](const char *label, const string &valueString) {
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
                TextWrapped("%s", valueString.c_str());
                if (BeginPopupContextItem("CopyGameValue")) {
                    if (MenuItem("Copy")) {
                        SetClipboardText(valueString.c_str());
                    }
                    EndPopup();
                }
                PopID();
                Spacing();
                Unindent(desiredTextIndent);
            };

            addRow("Name:", gameInfo.name);
            addRow("Place ID:", to_string(gameInfo.placeId));
            addRow("Universe ID:", to_string(gameInfo.universeId));
            addRow("Creator:", detailInfo.creatorName + string(detailInfo.creatorVerified ? " \xEF\x80\x8C" : ""));
            addRow("Players:", formatWithCommas(gameInfo.playerCount));
            addRow("Max Players:", formatWithCommas(detailInfo.maxPlayers));
            addRow("Visits:", formatWithCommas(detailInfo.visits));
            addRow("Genre:", detailInfo.genre);
            if (serverCount > 0)
                addRow("Est. Servers:", formatWithCommas(serverCount));

            TableNextRow();
            TableSetColumnIndex(0);
            Indent(desiredTextIndent);
            Spacing();
            TextUnformatted("Description:");
            Spacing();
            Unindent(desiredTextIndent);

            TableSetColumnIndex(1);
            Indent(desiredTextIndent);
            Spacing();

            ImGuiStyle &style = GetStyle();

            float spaceForBottomSpacingInCell = style.ItemSpacing.y;

            float spaceForSeparator = style.ItemSpacing.y;

            float spaceForButtons = GetFrameHeightWithSpacing();

            float reservedHeightBelowDescContent = spaceForBottomSpacingInCell + spaceForSeparator + spaceForButtons;

            float availableHeightForDescAndBelow = GetContentRegionAvail().y;

            float descChildHeight = availableHeightForDescAndBelow - reservedHeightBelowDescContent;

            float minDescHeight = GetTextLineHeightWithSpacing() * 3.0f;
            if (descChildHeight < minDescHeight) {
                descChildHeight = minDescHeight;
            }

            const string descStr = detailInfo.description;
            PushID("GameDesc");
            BeginChild("##DescScroll", ImVec2(0, descChildHeight - 4), false,
                       ImGuiWindowFlags_HorizontalScrollbar);
            TextWrapped("%s", descStr.c_str());
            if (BeginPopupContextItem("CopyGameDesc")) {
                if (MenuItem("Copy")) {
                    SetClipboardText(descStr.c_str());
                }
                EndPopup();
            }
            EndChild();
            PopID();

            Spacing();
            Unindent(desiredTextIndent);

            EndTable();
        }
        PopStyleVar();

        Separator();

        Indent(desiredTextIndent / 2);
        if (Button("Launch Game")) {
            if (!g_selectedAccountIds.empty()) {
                vector<pair<int, string>> accounts;
                for (int id : g_selectedAccountIds) {
                    auto it = find_if(g_accounts.begin(), g_accounts.end(),
                                      [&](const AccountData &a) { return a.id == id; });
                    if (it != g_accounts.end())
                        accounts.emplace_back(it->id, it->cookie);
                }
                if (!accounts.empty()) {
                    thread([placeId = gameInfo.placeId, accounts]() {
                              launchRobloxSequential(placeId, "", accounts);
                          })
                        .detach();
                } else {
                    Status::Error("Selected account not found to launch game.");
                }
            } else {
                Status::Error("No account selected to launch game.");
                ModalPopup::Add("Select an account first.");
            }
        }
        SameLine();
        if (Button("View Servers")) {
            g_activeTab = Tab_Servers;
            g_targetPlaceId_ServersTab = gameInfo.placeId;
            g_targetUniverseId_ServersTab = gameInfo.universeId;
        }
        Unindent(desiredTextIndent / 2);
    } else {
        Indent(desiredTextIndent);
        TextWrapped("Select a game from the list to see details or add a favorite.");
        Unindent(desiredTextIndent);
    }

    EndChild();
}
