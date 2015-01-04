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

#include "LeagueOverseer.h"
#include "LeagueOverseer-Helpers.h"

bool LeagueOverseer::SlashCommand (int playerID, bz_ApiString command, bz_ApiString /*message*/, bz_APIStringList *params)
{
	std::shared_ptr<bz_BasePlayerRecord> playerData(bz_getPlayerByIndex(playerID));

	// For some reason, the player record could not be created
	if (!playerData)
	{
	    return true;
	}

	// If the player is not verified and does not have the spawn permission, they can't use any of the commands
	if (!playerData->verified || !isLeagueMember(playerID))
	{
	    bz_sendTextMessagef(BZ_SERVER, playerID, "You do not have permission to run the /%s command.", command.c_str());
	    return true;
	}

	if (command == "cancel")
	{
	    if (playerData->team == eObservers) // Observers can't cancel matches
	    {
	        bz_sendTextMessage(BZ_SERVER, playerID, "Observers are not allowed to cancel matches.");
	    }
	    else if (bz_isCountDownInProgress()) // There's no way to stop a countdown so let's not cancel during a countdown
	    {
	        // We're canceling an official match
	        if (isOfficialMatch())
	        {
	            officialMatch->canceled = true;
	            officialMatch->cancelationReason = "Official match cancellation requested by " + std::string(playerData->callsign.c_str());
	        }

	        bz_cancelCountdown(playerData->callsign.c_str());
	    }
	    else if (bz_isCountDownActive()) // We can only cancel a match if the countdown is active
	    {
	        // We're canceling an official match
	        if (isOfficialMatch())
	        {
	            officialMatch->canceled = true;
	            officialMatch->cancelationReason = "Official match cancellation requested by " + std::string(playerData->callsign.c_str());
	        }
	        else // Cancel the fun match like normal
	        {
	            bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Fun match ended by %s", playerData->callsign.c_str());
	        }

	        logMessage(pluginSettings.getDebugLevel(), "debug", "Match ended by %s (%s).", playerData->callsign.c_str(), playerData->ipAddress.c_str());
	        bz_gameOver(253, eObservers);
	    }
	    else
	    {
	        bz_sendTextMessage(BZ_SERVER, playerID, "There is no match in progress to cancel.");
	    }
	}
	else if (command == "finish")
	{
	    if (playerData->team == eObservers) // Observers can't cancel matches
	    {
	        bz_sendTextMessage(BZ_SERVER, playerID, "Observers are not allowed to cancel matches.");
	    }
	    else if (bz_isCountDownInProgress()) // There's no way to stop a countdown so let's not finish during a countdown
	    {
	        bz_sendTextMessage(BZ_SERVER, playerID, "You may only finish a match after it has started.");
	    }
	    else if (bz_isCountDownActive()) // Only finish if the countdown is active
	    {
	        // We can only '/finish' official matches because I wanted to have a command only dedicated to
	        // reporting partially completed matches
	        if (isOfficialMatch())
	        {
	            // Let's check if we can report the match, in other words, at least half of the match has been reported
	            if (getMatchProgress() >= officialMatch->duration / 2)
	            {
	                logMessage(pluginSettings.getDebugLevel(), "debug", "Official match ended early by %s (%s)", playerData->callsign.c_str(), playerData->ipAddress.c_str());
	                bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Official match ended early by %s", playerData->callsign.c_str());

	                bz_gameOver(253, eObservers);
	            }
	            else
	            {
	                bz_sendTextMessage(BZ_SERVER, playerID, "Sorry, I cannot automatically report a match less than half way through.");
	                bz_sendTextMessage(BZ_SERVER, playerID, "Please use the /cancel command and message a referee for review of this match.");
	            }
	        }
	        else
	        {
	            bz_sendTextMessage(BZ_SERVER, playerID, "You cannot /finish a fun match. Use /cancel instead.");
	        }
	    }
	    else
	    {
	        bz_sendTextMessage(BZ_SERVER, playerID, "There is no match in progress to end.");
	    }
	}
	else if (command == "f" || command == "fm")
	{
	    if (pluginSettings.areFunMatchesDisabled())
	    {
	        bz_sendTextMessage(BZ_SERVER, playerID, "Sorry, this server has not be configured for fun matches.");
	    }
	    else if (playerData->team == eObservers) // Observers can't start matches
	    {
	        bz_sendTextMessage(BZ_SERVER, playerID, "Observers are not allowed to start matches.");
	    }
	    else if (isOfficialMatch() || bz_isCountDownActive() || bz_isCountDownInProgress()) // There is already a countdown
	    {
	        bz_sendTextMessage(BZ_SERVER, playerID, "There is already a game in progress; you cannot start another.");
	    }
	    else // They are verified, not an observer, there is no match. So start one
	    {
	        // We signify an FM whenever the 'officialMatch' variable is set to NULL so set it to null
	        officialMatch = NULL;

	        // Log the actions
	        logMessage(pluginSettings.getDebugLevel(), "debug", "Fun match started by %s (%s).", playerData->callsign.c_str(), playerData->ipAddress.c_str());
	        bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Fun match started by %s.", playerData->callsign.c_str());

	        // The amount of seconds the countdown should take
	        int timeToStart = (params->size() == 1) ? atoi(params->get(0).c_str()) : 10;

	        // Sanity check...
	        if (timeToStart <= 60 && timeToStart >= 10)
	        {
	            bz_startCountdown(timeToStart, bz_getTimeLimit(), "Server"); // Start the countdown with a custom countdown time limit under 1 minute
	        }
	        else
	        {
	            bz_sendTextMessage(BZ_SERVER, playerID, "Holy sanity check, Batman! Let's not have a countdown last longer than 60 seconds or less than 10.");
	            bz_startCountdown(10, bz_getTimeLimit(), "Server"); // Start the countdown for the official match
	        }
	    }
	}
	else if (command == "leagueoverseer" || command == "los")
	{
	    if (bz_hasPerm(playerID, "shutdownserver"))
	    {
	        if (params->size() > 0)
	        {
	            std::string commandOption = params->get(0).c_str();

	            if (commandOption == "reload")
	            {
	                pluginSettings.readConfigurationFile(CONFIG_PATH.c_str());
	                bz_sendTextMessage(BZ_SERVER, playerID, "League Overseer plug-in configuration reloaded.");
	            }
	        }
	    }
	    else
	    {
	        bz_sendTextMessage(BZ_SERVER, playerID, "You do not have permission to modify the League Overseer configuration.");
	    }
	}
	else if (command == "lodbg")
	{
	    if (pluginSettings.isInGameDebugEnabled())
	    {
	        if (bz_hasPerm(playerID, "shutdownserver"))
	        {
	            if (params->size() > 0)
	            {
	                std::string commandOption = params->get(0).c_str();

	                if (commandOption == "grant_perm" || commandOption == "revoke_perm")
	                {
	                    if (params->size() == 3)
	                    {
	                        int victimID = getPlayerFromCallsignOrID(params->get(1).c_str())->playerID;

	                        if (commandOption == "grant_perm")
	                        {
	                            bz_grantPerm(victimID, params->get(2).c_str());
	                        }
	                        else if (commandOption == "revoke_perm")
	                        {
	                            bz_revokePerm(victimID, params->get(2).c_str());
	                        }
	                    }
	                    else
	                    {
	                        bz_sendTextMessagef(BZ_SERVER, playerID, "Syntax: /lodbg %s <player id or callsign> <permission name>", commandOption.c_str());
	                    }
	                }
	            }
	            else
	            {
	                bz_sendTextMessage(BZ_SERVER, playerID, "League Overseer In-Game Debug Commands");
	                bz_sendTextMessage(BZ_SERVER, playerID, "--------------------------------------");
	                bz_sendTextMessage(BZ_SERVER, playerID, "   /lodbg <option> <parameters>");
	                bz_sendTextMessage(BZ_SERVER, playerID, "");
	                bz_sendTextMessage(BZ_SERVER, playerID, "   Options");
	                bz_sendTextMessage(BZ_SERVER, playerID, "   -------");
	                bz_sendTextMessage(BZ_SERVER, playerID, "     - grant_perm <player id or callsign> <permission name>");
	                bz_sendTextMessage(BZ_SERVER, playerID, "     - revoke_perm <player id or callsign> <permission name>");
	                bz_sendTextMessage(BZ_SERVER, playerID, "     - set");
	                bz_sendTextMessage(BZ_SERVER, playerID, "         - config_option <option> <value>");
	                bz_sendTextMessage(BZ_SERVER, playerID, "     - show");
	                bz_sendTextMessage(BZ_SERVER, playerID, "         - match_stats");
	                bz_sendTextMessage(BZ_SERVER, playerID, "         - player_stats <player id or callsign>");
	                bz_sendTextMessage(BZ_SERVER, playerID, "         - config_options");
	            }
	        }
	        else
	        {
	            bz_sendTextMessage(BZ_SERVER, playerID, "You do not have permission to use the /lodbg command.");
	        }
	    }
	    else
	    {
	        bz_sendTextMessage(BZ_SERVER, playerID, "The League Overseer Debug command is disabled on production servers.");
	    }
	}
	else if (command == "o" || command == "offi" || command == "official")
	{
	    if (pluginSettings.areOfficialMatchesDisabled())
	    {
	        bz_sendTextMessage(BZ_SERVER, playerID, "Sorry, this server has not be configured for official matches.");
	    }
	    else if (bz_pollActive())
	    {
	        bz_sendTextMessage(BZ_SERVER, playerID, "You are not allowed to start a match while a poll is active.");
	    }
	    else if (playerData->team == eObservers) // Observers can't start matches
	    {
	        bz_sendTextMessage(BZ_SERVER, playerID, "Observers are not allowed to start matches.");
	    }
	    else if (bz_getTeamCount(TEAM_ONE) < 2 || bz_getTeamCount(TEAM_TWO) < 2) // An official match cannot be 1v1 or 2v1
	    {
	        bz_sendTextMessage(BZ_SERVER, playerID, "You may not have an official match with less than 2 players per team.");
	    }
	    else if (isOfficialMatch() || bz_isCountDownActive() || bz_isCountDownInProgress()) // A countdown is in progress already
	    {
	        bz_sendTextMessage(BZ_SERVER, playerID, "There is already a game in progress; you cannot start another.");
	    }
	    else // They are verified non-observer with valid team sizes and no existing match. Start one!
	    {
	        officialMatch.reset(new OfficialMatch()); // It's an official match

	        // Log the actions so admins can bug brad to look at detailed information
	        logMessage(pluginSettings.getDebugLevel(), "debug", "Official match started by %s (%s).", playerData->callsign.c_str(), playerData->ipAddress.c_str());
	        bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Official match started by %s.", playerData->callsign.c_str());

	        // The amount of seconds the countdown should take
	        int timeToStart = (params->size() == 1) ? atoi(params->get(0).c_str()) : 10;

	        // Sanity check...
	        if (timeToStart <= 60 && timeToStart >= 10)
	        {
	            bz_startCountdown(timeToStart, bz_getTimeLimit(), "Server"); // Start the countdown with a custom countdown time limit under 1 minute
	        }
	        else
	        {
	            bz_sendTextMessage(BZ_SERVER, playerID, "Holy sanity check, Batman! Let's not have a countdown last longer than 60 seconds or less than 10.");
	            bz_startCountdown(10, bz_getTimeLimit(), "Server"); // Start the countdown for the official match
	        }
	    }
	}
	else if (command == "p" || command == "pause")
	{
	    if (bz_isCountDownPaused())
	    {
	        bz_sendTextMessage(BZ_SERVER, playerID, "The match is already paused!");
	    }
	    else if (bz_isCountDownActive())
	    {
	        bz_pauseCountdown(playerData->callsign.c_str());
	    }
	    else
	    {
	        bz_sendTextMessage(BZ_SERVER, playerID, "There is no active match to pause right now.");
	    }
	}
	else if (command == "r" || command == "resume")
	{
	    if (bz_pollActive())
	    {
	        bz_sendTextMessage(BZ_SERVER, playerID, "You are not allowed to resume a match while a poll is active.");
	    }
	    else if (!bz_isCountDownPaused())
	    {
	        bz_sendTextMessage(BZ_SERVER, playerID, "The match is not paused!");
	    }
	    else if (bz_isCountDownActive())
	    {
	        bz_resumeCountdown(playerData->callsign.c_str());

	        if (isOfficialMatch())
	        {
	            logMessage(pluginSettings.getVerboseLevel(), "debug", "Match resumed by %s.", playerData->callsign.c_str());
	        }
	    }
	    else
	    {
	        bz_sendTextMessage(BZ_SERVER, playerID, "There is no active match to resume right now.");
	    }
	}
	else if (command == "showhidden")
	{
	    if (bz_hasPerm(playerID, "ban"))
	    {
	        std::shared_ptr<bz_APIIntList> playerList(bz_getPlayerIndexList());

	        // Our player list couldn't be created so exit out of here
	        if (!playerList)
	        {
	            logMessage(pluginSettings.getVerboseLevel(), "debug", "Oops. I couldn't create a playerlist for some odd reason.");
	            bz_sendTextMessage(BZ_SERVER, playerID, "Seems like I darn goofed, please execute your command again.");
	            return true;
	        }

	        bz_sendTextMessage(BZ_SERVER, playerID, "Hidden Admins Present");
	        bz_sendTextMessage(BZ_SERVER, playerID, "---------------------");

	        for (unsigned int i = 0; i < playerList->size(); i++)
	        {
	            // If the player is hidden, then show them in the list
	            if (bz_hasPerm(playerList->get(i), "hideadmin"))
	            {
	                bz_sendTextMessagef(BZ_SERVER, playerID, " - %s", bz_getPlayerByIndex(playerList->get(i))->callsign.c_str());
	            }
	        }
	    }
	    else
	    {
	        bz_sendTextMessage(BZ_SERVER, playerID, "You do not have permission to use the /showhidden command.");
	    }
	}
	else if (command == "spawn")
	{
	    if (bz_hasPerm(playerID, "ban"))
	    {
	        if (params->size() > 0)
	        {
	            logMessage(pluginSettings.getVerboseLevel(), "debug", "%s has executed the /spawn command.", playerData->callsign.c_str());

	            std::string callsignOrID = params->get(0).c_str(); // Store the callsign we're going to search for

	            logMessage(pluginSettings.getVerboseLevel(), "debug", "Callsign or Player slot to look for: %s", callsignOrID.c_str());

	            std::shared_ptr<bz_BasePlayerRecord> victim(getPlayerFromCallsignOrID(callsignOrID.c_str()));

	            if (victim)
	            {
	                bz_grantPerm(victim->playerID, "spawn");
	                bz_sendTextMessagef(BZ_SERVER, eAdministrators, "%s granted %s the ability to spawn.", playerData->callsign.c_str(), victim->callsign.c_str());
	            }
	            else
	            {
	                bz_sendTextMessagef(BZ_SERVER, playerID, "player %s not found", callsignOrID.c_str());
	                logMessage(pluginSettings.getVerboseLevel(), "debug", "Player %s was not found.", callsignOrID.c_str());
	            }
	        }
	        else
	        {
	            bz_sendTextMessage(BZ_SERVER, playerID, "/spawn <player id or callsign>");
	        }
	    }
	    else
	    {
	        bz_sendTextMessage(BZ_SERVER, playerID, "You do not have permission to use the /spawn command.");
	    }
	}
	else if (command == "s" || command == "stats")
	{
	    if (isLeagueMember(playerID))
	    {
	        if (isOfficialMatch())
	        {
	            bz_sendTextMessage(BZ_SERVER, playerID, "Match Data");
	            bz_sendTextMessage(BZ_SERVER, playerID, "----------");

	            for (unsigned int i = 0; i < officialMatch->matchEvents.size(); i++)
	            {
	                std::string timeStamp = officialMatch->matchEvents.at(i).match_time;
	                std::string message   = officialMatch->matchEvents.at(i).message;

	                bz_sendTextMessagef(BZ_SERVER, playerID, "  [%s] %s", timeStamp.c_str(), message.c_str());
	            }
	        }
	        else
	        {
	            bz_sendTextMessage(BZ_SERVER, playerID, "Match data is not recorded for fun matches.");
	        }
	    }
	    else
	    {
	        bz_sendTextMessage(BZ_SERVER, playerID, "You do not have permission to use the /stats command.");
	    }
	}
	else if (command == "timelimit")
	{
	    if (isMatchInProgress())
	    {
	        bz_sendTextMessage(BZ_SERVER, playerID, "A match is already in progress, you cannot change the match duration now.");
	        return true;
	    }

	    if (params->size() != 1)
	    {
	        bz_sendTextMessagef (BZ_SERVER, playerID, "Usage: /timelimit <minutes>|show|reset");
	        return true;
	    }

	    bool timeChanged = false;
	    std::string commandOption = params->get(0).c_str();

	    if (commandOption == "show")
	    {
	        bz_sendTextMessagef(BZ_SERVER, playerID, "Match duration is set to %.0f minute(s)", (bz_getTimeLimit() / 60));
	    }
	    else if (commandOption == "reset")
	    {
	        resetTimeLimit();
	        timeChanged = true;
	    }
	    else if (isInteger(commandOption))
	    {
	        bz_setTimeLimit(std::stoi(commandOption) * 60);
	        timeChanged = true;
	    }

	    if (timeChanged)
	    {
	        bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Match duration set to %.0f minute(s) by %s", (bz_getTimeLimit() / 60), bz_getPlayerCallsign(playerID));
	    }
	}

	return true;
}