#pragma once

#include <string>
#include "data.h"
#include "accounts/accounts.h"
#include "friends/friends.h"
#include "games/games.h"
#include "history/history.h"
#include "servers/servers.h"
#include "settings/settings.h"
#include "console/console.h"

void RenderAccountsTable(std::vector<AccountData>&, const char*, float);

bool RenderMainMenu();

struct PublicServerInfo
{
	std::string jobId;
	int currentPlayers = 0;
	int maximumPlayers = 0;
	double averagePing = 0.0;
	double averageFps = 0.0;
};

struct GameInfo
{
	std::string name;
	uint64_t universeId = 0;
	uint64_t placeId = 0;
	int playerCount = 0;
	int upVotes = 0;
	int downVotes = 0;
	std::string creatorName;
	bool creatorVerified = false;
};
