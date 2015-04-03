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

#include <cmath>
#include <string>

#include "LeagueOverseer.h"
#include "LeagueOverseer-Helpers.h"

// We are building a string of BZIDs from the people who matched in the match that just occurred
// and we're also writing the player information to the server logs while we're at it. Efficiency!
std::string LeagueOverseer::buildBZIDString (bz_eTeamType team)
{
    // The string of BZIDs separated by commas
    std::string teamString;

    // Send a debug message of the players on the specified team
    bz_debugMessagef(0, "Match Data :: %s Team Players", formatTeam(team).c_str());

    // Add all the players from the specified team to the match report
    for (unsigned int i = 0; i < officialMatch->matchParticipants.size(); i++)
    {
        // If the player current player is part of the team we're formatting
        if (officialMatch->matchParticipants.at(i).teamColor == team)
        {
            // Add the BZID of the player to string with a comma at the end
            teamString += std::string(bz_urlEncode(officialMatch->matchParticipants.at(i).bzID.c_str())) + ",";

            // Output their information to the server logs
            bz_debugMessagef(0, "Match Data ::  %s [%s] (%s)", officialMatch->matchParticipants.at(i).callsign.c_str(),
                                                               officialMatch->matchParticipants.at(i).bzID.c_str(),
                                                               officialMatch->matchParticipants.at(i).ipAddress.c_str());
        }
    }

    // Return the comma separated string minus the last character because the loop will always
    // add an extra comma at the end. If we leave it, it will cause issues with the PHP counterpart
    // which tokenizes the BZIDs by commas and we don't want an empty BZID
    return teamString.erase(teamString.size() - 1);
}

bz_BasePlayerRecord* LeagueOverseer::bz_getPlayerByCallsign (const char* callsign)
{
    return bz_getPlayerByIndex(CALLSIGN_MAP[callsign]);
}

bz_BasePlayerRecord* LeagueOverseer::bz_getPlayerByBZID (const char* bzID)
{
    return bz_getPlayerByIndex(BZID_MAP[bzID]);
}

/**
 * Get a player record of a player from either a slot ID or a callsign
 *
 * @param  callsignOrID The callsign of the player or the player slot of the player
 *
 * @return              A player record gotten from the specified information
 */
bz_BasePlayerRecord* LeagueOverseer::getPlayerFromCallsignOrID(std::string callsignOrID)
{
    // We have a pound sign followed by a valid player index
    if (std::string::npos != callsignOrID.find("#") && isValidPlayerID(atoi(callsignOrID.erase(0, 1).c_str())))
    {
        // Let's make some easy reference variables
        int victimPlayerID = atoi(callsignOrID.erase(0, 1).c_str());

        return bz_getPlayerByIndex(victimPlayerID);
    }

    // Attempt to return a player record from a callsign if it isn't a player slot, this will return NULL
    // if no player is found with the respective callsign
    return bz_getPlayerByCallsign(callsignOrID.c_str());
}

// Return the progress of a match in seconds. For example, 20:00 minutes remaining would return 600
int LeagueOverseer::getMatchProgress (void)
{
    if (isMatchInProgress())
    {
        time_t now = time(NULL);

        return difftime(now, MATCH_START);
    }

    return -1;
}

// Get the literal time remaining in a match in the format of MM:SS
std::string LeagueOverseer::getMatchTime (void)
{
    int time = getMatchProgress();

    // We could not get the current match progress
    if (getMatchProgress() < 0)
    {
        return "-00:00";
    }

    // Let's covert the seconds of a match's progress into minutes and seconds
    int minutes = (officialMatch->duration/60) - ceil(time / 60.0);
    int seconds = 60 - (time % 60);

    // We need to store the literal values
    std::string minutesLiteral,
                secondsLiteral;

    // If the minutes remaining are less than 10 (only has one digit), then prepend a 0 to keep the format properly
    minutesLiteral = (minutes < 10) ? "0" : "";
    minutesLiteral += std::to_string(minutes);

    // Do some formatting for seconds similarly to minutes
    if (seconds == 60)
    {
        secondsLiteral = "00";
    }
    else
    {
        secondsLiteral = (seconds < 10) ? "0" : "";
        secondsLiteral += std::to_string(seconds);
    }

    return minutesLiteral + ":" + secondsLiteral;
}

std::string LeagueOverseer::getPlayerTeamNameByID (int playerID)
{
    std::shared_ptr<bz_BasePlayerRecord> playerData(bz_getPlayerByIndex(playerID));

    return getPlayerTeamNameByBZID(playerData->bzID.c_str());
}

std::string LeagueOverseer::getPlayerTeamNameByBZID (std::string bzID)
{
    return teamMottos[bzID];
}

// A convenience method to check if it's not an official match
bool LeagueOverseer::isFunMatch (void)
{
    return (!isOfficialMatch());
}

// A convenience method to check an official match is in progress
bool LeagueOverseer::isFunMatchInProgress (void)
{
    return (isMatchInProgress() && isFunMatch());
}

// Check if a player is part of the league
bool LeagueOverseer::isLeagueMember (int playerID)
{
    return IS_LEAGUE_MEMBER[playerID];
}

// Check if there is a match in progress; even if it's paused
bool LeagueOverseer::isMatchInProgress (void)
{
    return (bz_isCountDownActive() || bz_isCountDownPaused() || bz_isCountDownInProgress());
}

// Check if there is currently an active official match
bool LeagueOverseer::isOfficialMatch(void)
{
    return (officialMatch != NULL);
}

// Check if there is currently an active official match
bool LeagueOverseer::isOfficialMatchInProgress (void)
{
    return (isOfficialMatch() && isMatchInProgress());
}

// Check if a player was already on the server within 5 minutes of their last part
bool LeagueOverseer::playerAlreadyJoined (std::string bzID)
{
    // Loop through all of the players on the list
    for (unsigned int i = 0; i < activePlayerList.size(); i++)
    {
        // The player has their BZID saved as being active
        if (activePlayerList.at(i).bzID == bzID)
        {
            // If they left at least 1 minute ago, then update their last active time and consider them active
            if (activePlayerList.at(i).lastActive + 60 > bz_getCurrentTime())
            {
                activePlayerList.at(i).lastActive = bz_getCurrentTime();
                return true;
            }
            else // Their BZID was saved but it's been more than 5 minutes, remove them and mark them as they were not active
            {
                activePlayerList.erase(activePlayerList.begin() + i, activePlayerList.begin() + i + 1);
                return false;
            }
        }
    }

    // They weren't saved on the list
    return false;
}

// Forget a player from the local database of player information
void LeagueOverseer::removePlayerInfo(std::string bzID, std::string callsign)
{
    if (BZID_MAP.find(bzID) != BZID_MAP.end())
    {
        BZID_MAP.erase(bzID);
    }

    if (CALLSIGN_MAP.find(callsign) != CALLSIGN_MAP.end())
    {
        CALLSIGN_MAP.erase(callsign);
    }
}

// Request a team name update for all the members of a team
void LeagueOverseer::requestTeamName (bz_eTeamType team)
{
    logMessage(pluginSettings.getVerboseLevel(), "debug", "A team name update for the '%s' team has been requested.", formatTeam(team).c_str());
    std::shared_ptr<bz_APIIntList> playerList(bz_getPlayerIndexList());

    // Our player list couldn't be created so exit out of here
    if (!playerList)
    {
        logMessage(pluginSettings.getVerboseLevel(), "debug", "The player list to process team names failed to be created.");
        return;
    }

    for (unsigned int i = 0; i < playerList->size(); i++)
    {
        std::shared_ptr<bz_BasePlayerRecord> playerRecord(bz_getPlayerByIndex(playerList->get(i)));

        if (playerRecord && playerRecord->team == team) // Only request a new team name for the players of a certain team
        {
            logMessage(pluginSettings.getVerboseLevel(), "debug", "Player '%s' is a part of the '%s' team.", playerRecord->callsign.c_str(), formatTeam(team).c_str());
            requestTeamName(playerRecord->callsign.c_str(), playerRecord->bzID.c_str());
        }
    }
}

// Because there will be different times where we request a team name motto, let's make into a function
void LeagueOverseer::requestTeamName (std::string callsign, std::string bzID)
{
    logMessage(pluginSettings.getDebugLevel(), "debug", "Sending motto request for '%s'", callsign.c_str());

    // Build the POST data for the URL job
    TeamUrlRepo.set("query", "teamName")
               .set("bzid", bzID)
               .submit();
}

// Reset the time limit to what is in the plug-in configuration
void LeagueOverseer::resetTimeLimit()
{
    bz_setTimeLimit(pluginSettings.getDefaultTimeLimit() * 60);
}

// Check the player's user groups to see if they belong to the league and save that value
void LeagueOverseer::setLeagueMember (int playerID)
{
    std::shared_ptr<bz_BasePlayerRecord> playerData(bz_getPlayerByIndex(playerID));

    IS_LEAGUE_MEMBER[playerID] = false;

    // If a player isn't verified, then they are for sure not a registered player
    if (playerData->verified)
    {
        for (unsigned int i = 0; i < playerData->groups.size(); i++) // Go through all the groups a player belongs to
        {
            std::string group = playerData->groups.get(i).c_str(); // Convert the group into a string

            if (group == pluginSettings.getLeagueGroup()) // Player is a part of the *.LEAGUE group
            {
                IS_LEAGUE_MEMBER[playerID] = true;
                break;
            }
        }
    }
}

// Store player information in a local database to avoid looping through a playerlist
void LeagueOverseer::storePlayerInfo(int playerID, std::string bzID, std::string callsign)
{
    if (isLeagueMember(playerID))
    {
        BZID_MAP[bzID] = playerID;
        CALLSIGN_MAP[callsign] = playerID;
    }
}

// Check if there is any need to invalidate a roll call team
void LeagueOverseer::validateTeamName (bool &invalidate, bool &teamError, MatchParticipant currentPlayer, std::string &teamName, bz_eTeamType team)
{
    logMessage(pluginSettings.getVerboseLevel(), "debug", "Starting validation of the %s team.", formatTeam(team).c_str());

    // Check if the player is a part of the team we're validating
    if (currentPlayer.teamColor == team)
    {
        // Check if the team name of team one has been set yet, if it hasn't then set it
        // and we'll be able to set it so we can conclude that we have the same team for
        // all of the players
        if (teamName == "")
        {
            teamName = currentPlayer.teamName;
            logMessage(pluginSettings.getVerboseLevel(), "debug", "The team name for the %s team has been set to: %s", formatTeam(team).c_str(), teamName.c_str());
        }
        // We found someone with a different team name, therefore we need invalidate the
        // roll call and check all of the member's team names for sanity
        else if (teamName != currentPlayer.teamName)
        {
            logMessage(pluginSettings.getVerboseLevel(), "error", "Player '%s' is not part of the '%s' team.", currentPlayer.callsign.c_str(), teamName.c_str());
            invalidate = true; // Invalidate the roll call
            teamError = true;  // We need to check team one's members for their teams
        }
    }

    if (!teamError)
    {
        logMessage(pluginSettings.getVerboseLevel(), "debug", "Player '%s' belongs to the '%s' team.", currentPlayer.callsign.c_str(), currentPlayer.teamName.c_str());
    }
}