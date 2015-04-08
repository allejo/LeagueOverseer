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
#include <sys/stat.h>

#include "bzfsAPI.h"

#include "LeagueOverseer-Helpers.h"

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

Match& Match::savePlayer (bz_BasePlayerRecord *pr, int teamID)
{
    Player currentPlayer;

    currentPlayer.bzID      = pr->bzID.c_str();
    currentPlayer.callsign  = pr->callsign.c_str();
    currentPlayer.ipAddress = pr->ipAddress.c_str();
    currentPlayer.teamColor = pr->team;
    currentPlayer.teamID    = teamID;

    matchParticipants.push_back(currentPlayer);

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

Match &Match::saveEvent(json_object *eventObject)
{
    json_object_array_add(jEvents, eventObject);

    return *this;
}

int Match::getTeamOneScore (void)
{
    return teamOneScore;
}

int Match::getTeamTwoScore (void)
{
    return teamTwoScore;
}

bool Match::matchCanceled (void)
{
    return isMatchCanceled;
}

bool Match::isRosterEmpty (void)
{
    return matchParticipants.empty();
}

void Match::save (std::string _timestamp, std::string _replayFile)
{
    json_object* jPlayerStats = json_object_new_array();

    for (const auto &stat : matchPlayerStats)
    {
        json_object* jStats = json_object_new_object();
        json_object* jKillsAgainst = json_object_new_object();
        json_object* jDeathsAgainst = json_object_new_object();

        PlayerStats  myStats = stat.second;

        for (const auto &kill : myStats.killsAgainst)
        {
            std::string killAgainstBZID = kill.first;
            int killsAgainst = kill.second;

            json_object_object_add(jKillsAgainst, killAgainstBZID.c_str(), json_object_new_int(killsAgainst));
        }

        for (const auto &death : myStats.deathsAgainst)
        {
            std::string deathsAgainstBZID = death.first;
            int deathsAgainst = death.second;

            json_object_object_add(jDeathsAgainst, deathsAgainstBZID.c_str(), json_object_new_int(deathsAgainst));
        }

        json_object_object_add(jStats, "BZID",          json_object_new_string(myStats.bzID.c_str()));
        json_object_object_add(jStats, "CapCount",      json_object_new_int(myStats.captureCount));
        json_object_object_add(jStats, "DeathCount",    json_object_new_int(myStats.deathCount));
        json_object_object_add(jStats, "KillCount",     json_object_new_int(myStats.killCount));
        json_object_object_add(jStats, "TeamKills",     json_object_new_int(myStats.teamKills));
        json_object_object_add(jStats, "SelfKills",     json_object_new_int(myStats.selfKills));
        json_object_object_add(jStats, "KillsAgainst",  jKillsAgainst);
        json_object_object_add(jStats, "DeathsAgainst", jDeathsAgainst);

        json_object_array_add(jPlayerStats, jStats);
    }

    json_object* jPlayerRoster = json_object_new_array();

    for (const auto &player : matchParticipants)
    {
        json_object* myPlayer = json_object_new_object();

        json_object_object_add(myPlayer, "TeamID",    json_object_new_int(player.teamID));
        json_object_object_add(myPlayer, "TeamColor", json_object_new_string(eTeamToString(player.teamColor).c_str()));
        json_object_object_add(myPlayer, "BZID",      json_object_new_string(player.bzID.c_str()));
        json_object_object_add(myPlayer, "Callsign",  json_object_new_string(player.callsign.c_str()));
        json_object_object_add(myPlayer, "IpAddress", json_object_new_string(player.ipAddress.c_str()));

        json_object_array_add(jPlayerRoster, myPlayer);
    }

    json_object_object_add(jMaster, "MatchOfficial",   json_object_new_boolean(official));
    json_object_object_add(jMaster, "MatchCanceled",   json_object_new_boolean(isMatchCanceled));
    json_object_object_add(jMaster, "MatchDuration",   json_object_new_int(matchDuration));
    json_object_object_add(jMaster, "MatchPlayed",     json_object_new_string(mapPlayed.c_str()));
    json_object_object_add(jMaster, "MatchTimestamp",  json_object_new_string(_timestamp.c_str()));
    json_object_object_add(jMaster, "MatchServer",     json_object_new_string(bz_getPublicAddr().c_str()));
    json_object_object_add(jMaster, "MatchReplayFile", json_object_new_string(_replayFile.c_str()));
    json_object_object_add(jMaster, "MatchEvents",     jEvents);
    json_object_object_add(jMaster, "PlayerRoster",    jPlayerRoster);
    json_object_object_add(jMaster, "PlayerStats",     jPlayerStats);
    json_object_object_add(jMaster, "TeamOneID",       json_object_new_int(teamOneID));
    json_object_object_add(jMaster, "TeamOneWins",     json_object_new_int(teamOneScore));
    json_object_object_add(jMaster, "TeamTwoID",       json_object_new_int(teamTwoID));
    json_object_object_add(jMaster, "TeamTwoWins",     json_object_new_int(teamTwoScore));

    logMessage(verboseLevel, "debug", "Match object saved: %s", json_object_to_json_string(jMaster));
}

Match &Match::setMatchDuration (int _duration)
{
    matchDuration = _duration;

    return *this;
}

Match &Match::setVerboseLevel (int _verbose)
{
    verboseLevel = _verbose;

    return *this;
}

Match &Match::setDebugLevel (int _debug)
{
    debugLevel = _debug;

    return *this;
}

std::string Match::toString (void)
{
    return json_object_to_json_string(jMaster);
}

Match &Match::cancelMatch (std::string reason)
{
    isMatchCanceled = true;
    cancelationReason = reason;

    return *this;
}

std::string Match::getCancelation (void)
{
    return cancelationReason;
}

int Match::getMatchRollCall (void)
{
    return matchRollCall;
}

int Match::incrementMatchRollCall (int _newRollCall)
{
    matchRollCall = matchRollCall + _newRollCall;

    return matchRollCall;
}

int Match::getMatchDuration ()
{
    return matchDuration;
}

void Match::clearPlayerRoster (void)
{
    matchParticipants.clear();
}

Match &Match::setTeamOneName(std::string _teamName)
{
    teamOneName = _teamName;

    return *this;
}

Match &Match::setTeamTwoName(std::string _teamName)
{
    teamTwoName = _teamName;

    return *this;
}

bool Match::reportMatch (void)
{
    return reportTheMatch;
}

Match& Match::reportMatch(bool _report)
{
    reportTheMatch = _report;

    return *this;
}