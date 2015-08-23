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

    // Slash commands we'll be overloading and/or disabling
    if (command == "gameover")
    {
        bz_sendTextMessage(BZ_SERVER, playerID, "The '/gameover' command has been disabled on this server.");

        if (isFunMatchInProgress())
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "Please use the '/cancel' command instead.");
        }
        else if (isOfficialMatchInProgress())
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "Please use the '/cancel' command to cancel the match without reporting the match to the League website.");
            bz_sendTextMessage(BZ_SERVER, playerID, "Please use the '/finish' command to finish the match early and report it to the League website as an official match.");
        }

        return true;
    }
    else if (command == "countdown")
    {
        if (params->get(0) == "cancel" || params->get(0) == "pause" || params->get(0) == "resume")
        {
            bz_sendTextMessagef(BZ_SERVER, playerID, "The '/countdown %s' command has been disabled on this server. Please use '/%s' instead", params->get(0).c_str(), params->get(0).c_str());
        }
        else
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "The '/countdown TIME' command has disabled. Please use '/official' or '/fm' instead.");
        }

        return true;
    }
    else if (command == "poll")
    {
        if (isMatchInProgress())
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "To prevent distractions, you may not initiate polls while a match is progress.");
            bz_sendTextMessage(BZ_SERVER, playerID, "Please contact an administrator or '/pause' the match and start the poll while the match is paused.");

            return true;
        }

        return false;
    }

    // If the player is not verified and does not have the spawn permission, they can't use any of the commands
    if (!playerData->verified || !isLeagueMember(playerID))
    {
        bz_sendTextMessagef(BZ_SERVER, playerID, "You do not have permission to run the /%s command.", command.c_str());
        return true;
    }

    if (command == "ban")
    {
        if ((bz_hasPerm(playerID, "ban") || bz_hasPerm(playerID, "shortBan")) && bz_hasPerm(playerID, "hideAdmin"))
        {
            // @TODO Implement this functionality

            return true;
        }

        return false;
    }
    else if (command == "cancel")
    {
        if (playerData->team == eObservers) // Observers can't cancel matches
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "Observers are not allowed to cancel matches.");
        }
        else if (bz_isCountDownInProgress()) // We need to cancel differently if we're in a countdown
        {
            // We're canceling an official match
            if (isOfficialMatch())
            {
                currentMatch.reportMatch(false)
                            .cancelMatch("Official match cancellation requested by " + std::string(playerData->callsign.c_str()));
            }

            bz_cancelCountdown(playerData->callsign.c_str());
        }
        else if (bz_isCountDownActive()) // We can only cancel a match if the countdown is active
        {
            // We're canceling an official match
            if (isOfficialMatch())
            {
                currentMatch.cancelMatch("Official match cancellation requested by " + std::string(playerData->callsign.c_str()));
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

        return true;
    }
    else if (command == "finish")
    {
        if (playerData->team == eObservers) // Observers can't cancel matches
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "Observers are not allowed to cancel matches.");
        }
        else if (bz_isCountDownInProgress())
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "You may only finish a match after it's half way through. Use '/cancel' instead.");
        }
        else if (bz_isCountDownActive()) // Only finish if the countdown is active
        {
            // We can only '/finish' official matches because I wanted to have a command only dedicated to
            // reporting partially completed matches
            if (isOfficialMatch())
            {
                // Let's check if we can report the match, in other words, at least half of the match has been reported
                if (getMatchProgress() >= currentMatch.getMatchDuration() / 2)
                {
                    logMessage(pluginSettings.getDebugLevel(), "debug", "Official match ended early by %s (%s)", playerData->callsign.c_str(), playerData->ipAddress.c_str());
                    bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Official match ended early by %s", playerData->callsign.c_str());

                    bz_gameOver(253, eObservers);
                }
                else
                {
                    bz_sendTextMessage(BZ_SERVER, playerID, "Sorry, you cannot automatically report a match less than half way through.");
                    bz_sendTextMessage(BZ_SERVER, playerID, "Please use the /cancel command and message a referee for review of this match.");
                }
            }
            else
            {
                bz_sendTextMessage(BZ_SERVER, playerID, "You cannot /finish a fun match. Use '/cancel' instead.");
            }
        }
        else
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "There is no match in progress to end.");
        }

        return true;
    }
    else if (command == "kick")
    {
        if (bz_hasPerm(playerID, "kick") && bz_hasPerm(playerID, "hideAdmin"))
        {
            std::shared_ptr<bz_BasePlayerRecord> victim(bz_getPlayerBySlotOrCallsign(params->get(0).c_str()));
            std::string reason = "";

            if (params->size() > 1)
            {
                for (unsigned int i = 1; i < params->size(); i++)
                {
                    reason += params->get(i);

                    if (i + 1 < params->size())
                    {
                        reason += " ";
                    }
                }
            }
            else
            {
                reason += params->get(1);
            }

            bz_sendTextMessagef(BZ_SERVER, victim->playerID, "You were kicked for: %s", reason.c_str());
            bz_sendTextMessagef(BZ_SERVER, eAdministrators, "%s kicked %s for: %s", playerData->callsign.c_str(), victim->callsign.c_str(), reason.c_str());

            bz_kickUser(victim->playerID, reason.c_str(), false);

            return true;
        }

        return false;
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
                    pluginSettings.loadConfig(CONFIG_PATH.c_str());
                    bz_sendTextMessage(BZ_SERVER, playerID, "League Overseer plug-in configuration reloaded.");
                }
            }
        }
        else
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "You do not have permission to modify the League Overseer configuration.");
        }

        return true;
    }
    else if (command == "f" || command == "fm" || command == "o" || command == "offi" || command == "official")
    {
        bool officialMatchRequested = (command == "o" || command == "offi" || command == "official");

        if (officialMatchRequested)
        {
            if (pluginSettings.areOfficialMatchesDisabled())
            {
                bz_sendTextMessage(BZ_SERVER, playerID, "Sorry, this server has not been configured for official matches.");

                return true;
            }

            if (bz_getTeamCount(TEAM_ONE) < 2 || bz_getTeamCount(TEAM_TWO) < 2) // An official match cannot be 1v1 or 2v1
            {
                bz_sendTextMessage(BZ_SERVER, playerID, "You may not have an official match with less than 2 players per team.");

                return true;
            }
        }
        else
        {
            if (pluginSettings.areFunMatchesDisabled())
            {
                bz_sendTextMessage(BZ_SERVER, playerID, "Sorry, this server has not be configured for fun matches.");

                return true;
            }
        }

        if (bz_pollActive())
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "You are not allowed to start a match while a poll is active.");
        }
        else if (playerData->team == eObservers) // Observers can't start matches
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "Observers are not allowed to start matches.");
        }
        else if (isMatchInProgress()) // A countdown is in progress already
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "There is already a game in progress; you cannot start another.");
        }
        else
        {
            currentMatch = Match();

            (officialMatchRequested) ? currentMatch.setOfficial() : currentMatch.setFM();

            logMessage(pluginSettings.getDebugLevel(), "debug", "%s match started by %s (%s).",
                    (officialMatchRequested ? "Official" : "Fun"),
                    playerData->callsign.c_str(),
                    playerData->ipAddress.c_str());

            bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%s match started by %s.",
                    (officialMatchRequested ? "Official" : "Fun"),
                    playerData->callsign.c_str());

            // The default countdown will be 10 seconds
            int timeToStart = 10;
            std::string requestedTime = params->get(0).c_str();

            if (params->size() == 1)
            {
                try
                {
                    int proposedTime = std::stoi(requestedTime);

                    if (proposedTime <= 60 && proposedTime >= 10)
                    {
                        timeToStart = proposedTime;
                    }
                    else
                    {
                        bz_sendTextMessage(BZ_SERVER, playerID, "Holy sanity check, Batman! Let's not have a countdown last longer than 60 seconds or less than 10.");
                    }
                }
                catch (std::exception const &e)
                {
                    bz_sendTextMessagef(BZ_SERVER, playerID, "Unrecognized time '%s' for countdown provided. Using default countdown of %d seconds", requestedTime.c_str(), timeToStart);
                }
            }
            else if (params->size() > 1)
            {
                bz_sendTextMessagef(BZ_SERVER, playerID, "Unrecognized amount of parameters given. Using default countdown of %d seconds", timeToStart);
            }

            bz_startCountdown(timeToStart, bz_getTimeLimit(), "Server");
        }

        return true;
    }
    else if (command == "mute" || command == "unmute")
    {
        bool mute = (command == "mute");

        if (((bz_hasPerm(playerID, "mute") && mute) || (bz_hasPerm(playerID, "unmute") && !mute)) && bz_hasPerm(playerID, "hideAdmin"))
        {
            const char* callsignOrID = params->get(0).c_str();
            std::shared_ptr<bz_BasePlayerRecord> victim(bz_getPlayerBySlotOrCallsign(callsignOrID));

            if (victim)
            {
                if (mute)
                {
                    bz_revokePerm(victim->playerID, "privateMessage");
                    bz_revokePerm(victim->playerID, "talk");
                }
                else
                {
                    bz_grantPerm(victim->playerID, "privateMessage");
                    bz_grantPerm(victim->playerID, "talk");
                }

                bz_sendTextMessagef(BZ_SERVER, eAdministrators, "%s has %sd %s.", playerData->callsign.c_str(), command.c_str(), victim->callsign.c_str());
                bz_sendTextMessagef(BZ_SERVER, victim->playerID, "You have been %sd.", command.c_str());
            }
            else
            {
                bz_sendTextMessagef(BZ_SERVER, playerID, "player \"%s\" not found", callsignOrID);
            }

            return true;
        }

        return false;
    }
    else if (command == "p" || command == "pause")
    {
        if (bz_isCountDownPaused())
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "The match is already paused!");
        }
        else if (bz_isCountDownActive())
        {
            bz_pauseCountdown(playerID);
        }
        else
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "There is no active match to pause right now.");
        }

        return true;
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
            bz_resumeCountdown(playerID);
        }
        else
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "There is no active match to resume right now.");
        }

        return true;
    }
    else if (command == "showhidden")
    {
        if (bz_hasPerm(playerID, pluginSettings.getShowHiddenPerm().c_str()))
        {
            std::shared_ptr<bz_APIIntList> playerList(bz_getPlayerIndexList());

            if (!playerList)
            {
                bz_sendTextMessage(BZ_SERVER, playerID, "An internal error occurred. Try again.");
                return true;
            }

            bz_sendTextMessage(BZ_SERVER, playerID, "Hidden Admins Present");
            bz_sendTextMessage(BZ_SERVER, playerID, "---------------------");

            for (unsigned int i = 0; i < playerList->size(); i++)
            {
                // If the player is hidden, then show them in the list
                if (bz_hasPerm(playerList->get(i), "hideadmin"))
                {
                    bz_sendTextMessagef(BZ_SERVER, playerID, " - %s", bz_getPlayerCallsign(playerList->get(i)));
                }
            }
        }
        else
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "You do not have permission to use the /showhidden command.");
        }

        return true;
    }
    else if (command == "spawn")
    {
        if (bz_hasPerm(playerID, pluginSettings.getSpawnCommandPerm().c_str()))
        {
            if (params->size() > 0)
            {
                const char* callsignOrID = params->get(0).c_str();
                std::shared_ptr<bz_BasePlayerRecord> victim(bz_getPlayerBySlotOrCallsign(callsignOrID));

                if (victim)
                {
                    bz_grantPerm(victim->playerID, "spawn");
                    bz_sendTextMessagef(BZ_SERVER, eAdministrators, "%s granted %s the ability to spawn.", playerData->callsign.c_str(), victim->callsign.c_str());
                }
                else
                {
                    bz_sendTextMessagef(BZ_SERVER, playerID, "player \"%s\" not found", callsignOrID);
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

        return true;
    }
    else if (command == "s" || command == "stats")
    {
        if (isLeagueMember(playerID))
        {
            if (isMatchInProgress())
            {
                bz_sendTextMessage(BZ_SERVER, playerID, "Match Data");
                bz_sendTextMessage(BZ_SERVER, playerID, "----------");

                // @TODO Need to write this
            }
            else
            {
                bz_sendTextMessage(BZ_SERVER, playerID, "There is no match in progress.");
            }
        }
        else
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "You do not have permission to use the /stats command.");
        }

        return true;
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

        return true;
    }

    return false;
}