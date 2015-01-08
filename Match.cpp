/*
League Overseer
    Copyright (C) 2013-2015 Vladimir Jimenez & Ned Anderson

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "Match.h"

#include <json/json.h>
#include <map>
#include <memory>
#include <string>

#include "bzfsAPI.h"

bool Match::isOfficial (void)
{
    return official;
}

bool Match::isFM (void)
{
    return !official;
}

Match& Match::setOfficial (void)
{
    official = true;

    return *this;
}

Match& Match::savePlayer (bz_BasePlayerRecord *pr)
{
    Player currentPlayer;

    currentPlayer.bzID      = pr->bzID.c_str();
    currentPlayer.callsign  = pr->callsign.c_str();
    currentPlayer.ipAddress = pr->ipAddress.c_str();
    currentPlayer.teamColor = pr->team;

    return *this;
}

Match& Match::setFM (void)
{
    official = false;

    return *this;
}

void Match::stats_createPlayer (int playerID)
{
    std::string bzID = bz_getPlayerByIndex(playerID)->bzID.c_str();

    PlayerStats playerStats;
    playerStats.bzID = bzID;

    matchPlayerStats[bzID] = playerStats;
}

void Match::stats_flagCapture (int playerID)
{
    std::string bzID = bz_getPlayerByIndex(playerID)->bzID.c_str();

    matchPlayerStats[bzID].captureCount++;
}

void Match::stats_playerKill (int killerID, int victimID)
{
    std::shared_ptr<bz_BasePlayerRecord> killer(bz_getPlayerByIndex(killerID));
    std::shared_ptr<bz_BasePlayerRecord> victim(bz_getPlayerByIndex(victimID));

    std::string killerBZID = killer->bzID.c_str();
    std::string victimBZID = victim->bzID.c_str();

    if (killerID == victimID)
    {
        matchPlayerStats[killerBZID].selfKills++;
    }
    else
    {
        if (killer->team == victim->team)
        {
            matchPlayerStats[killerBZID].teamKills++;
        }

        matchPlayerStats[killerBZID].killsAgainst[victimBZID]++;
        matchPlayerStats[victimBZID].deathsAgainst[killerBZID]++;

        matchPlayerStats[killerBZID].killCount++;
        matchPlayerStats[victimBZID].deathCount++;
    }
}

void Match::incrementTeamOneScore (void)
{
    teamTwoScore++;
}

void Match::incrementTeamTwoScore (void)
{
    teamOneScore++;
}
