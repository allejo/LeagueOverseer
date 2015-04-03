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

#include <algorithm>

#include "LeagueOverseer.h"
#include "LeagueOverseer-Helpers.h"

#include "MatchEvent-Join.h"

void LeagueOverseer::Event (bz_EventData *eventData)
{
    switch (eventData->eventType)
    {
        case bz_eAllowFlagGrab: // This event is called each time a player attempts to grab a flag
        {
            bz_AllowFlagGrabData_V1* allowFlagGrabData = (bz_AllowFlagGrabData_V1*)eventData;

            // Data
            // ---
            //    (int)          playerID  - The ID of the player who is grabbing the flag
            //    (int)          flagID    - The ID of the flag that is going to be grabbed
            //    (const char*)  flagType  - The type of the flag about to be grabbed
            //    (bool)         allow     - Whether or not to allow the flag grab
            //    (double)       eventTime - The server time at which the event occurred (in seconds).

            std::string              flagAbbr          = allowFlagGrabData->flagType;
            int                      playerID          = allowFlagGrabData->playerID;

            if (pluginSettings.isPcProtectionEnabled()) // Is the server configured to protect against Pass Camping
            {
                // Check if the last capture was within the 'PC_PROTECTION_DELAY' amount of seconds
                if (LAST_CAP + PC_PROTECTION_DELAY > bz_getCurrentTime())
                {
                    // Check to see if the flag being grabbed belongs to the team that just had their flag captured AND check
                    // to see if someone not from the victim team grabbed it
                    if ((getTeamTypeFromFlag(flagAbbr) == CAP_VICTIM_TEAM && bz_getPlayerTeam(playerID) != CAP_VICTIM_TEAM))
                    {
                        // Disallow the flag grab if it's being grabbed by an enemy right after a flag capture
                        allowFlagGrabData->allow = false;
                    }
                }
            }
        }
        break;

        case bz_eAllowSpawn: // This event is called before a player respawns
        {
            bz_AllowSpawnData_V1* allowSpawnData = (bz_AllowSpawnData_V1*)eventData;

            // Data
            // ---
            //    (int)           playerID  - This value is the player ID for the joining player.
            //    (bz_eTeamType)  team      - The team the player belongs to.
            //    (bool)          handled   - Whether or not the plugin will be handling the respawn or not.
            //    (bool)          allow     - Set to false if the player should not be allowed to spawn.
            //    (double)        eventTime - The server time the event occurred (in seconds.)

            int playerID = allowSpawnData->playerID;

            if (!isLeagueMember(playerID)) // Is the player not part of the league?
            {
                // Disable their spawning privileges
                allowSpawnData->handled = true;
                allowSpawnData->allow   = false;

                // Send the player a message, either default or custom based on 'SPAWN_MSG_ENABLED'
                sendPluginMessage(playerID, pluginSettings.isSpawnMessageEnabled(), pluginSettings.getNoSpawnMessage(), SPAWN);
            }
        }
        break;

        case bz_eBZDBChange: // This event is called each time a BZDB variable is changed
        {
            bz_BZDBChangeData_V1* bzdbData = (bz_BZDBChangeData_V1*)eventData;

            // Data
            // ---
            //    (bz_ApiString)  key       - The variable that was changed
            //    (bz_ApiString)  value     - What the variable was changed too
            //    (double)        eventTime - This value is the local server time of the event.

            if (bzdbData->key == "_pcProtectionDelay")
            {
                // Save the proposed value in a variable for easy access
                int proposedValue = atoi(bzdbData->value.c_str());

                // Our PC protection delay should be between 3 and 15 seconds only, otherwise set it to the default 5 seconds
                if (proposedValue >= 3 && proposedValue <= 15)
                {
                    PC_PROTECTION_DELAY = proposedValue;
                }
                else
                {
                    PC_PROTECTION_DELAY = 5;
                    bz_setBZDBInt("_pcProtectionDelay", PC_PROTECTION_DELAY);
                }
            }
        }
        break;

        case bz_eCaptureEvent: // This event is called each time a team's flag has been captured
        {
            // We only need to keep track of the store if it's an official match
            if (isOfficialMatch())
            {
                bz_CTFCaptureEventData_V1* captureData = (bz_CTFCaptureEventData_V1*)eventData;

                // Data
                // ---
                //    (bz_eTeamType)  teamCapped    - The team whose flag was captured.
                //    (bz_eTeamType)  teamCapping   - The team who did the capturing.
                //    (int)           playerCapping - The player who captured the flag.
                //    (float[3])      pos           - The world position(X,Y,Z) where the flag has been captured
                //    (float)         rot           - The rotational orientation of the capturing player
                //    (double)        eventTime     - This value is the local server time of the event.

                (captureData->teamCapping == TEAM_ONE) ? officialMatch->teamOnePoints++ : officialMatch->teamTwoPoints++;

                // Log the information about the current score to the logs at the verbose level
                logMessage(pluginSettings.getVerboseLevel(), "debug", "%s team scored.", formatTeam(captureData->teamCapping).c_str());
                logMessage(pluginSettings.getVerboseLevel(), "debug", "Official Match Score %s [%i] vs %s [%i]",
                    formatTeam(TEAM_ONE).c_str(), officialMatch->teamOnePoints,
                    formatTeam(TEAM_TWO).c_str(), officialMatch->teamTwoPoints);

                CAP_VICTIM_TEAM = captureData->teamCapped;
                CAP_WINNER_TEAM = captureData->teamCapping;
                LAST_CAP        = captureData->eventTime;

                // Create a player record of the person who captured the flag
                std::shared_ptr<bz_BasePlayerRecord> playerData(bz_getPlayerByIndex(captureData->playerCapping));

                // Create a MatchEvent with the information relating to the capture
                MatchEvent capEvent(playerData->playerID, std::string(playerData->bzID.c_str()),
                                     std::string(playerData->callsign.c_str()) + " captured the " + formatTeam(captureData->teamCapped) + " flag",
                                     "{\"event\": {\"type\": \"capture\", \"color\": \"" + formatTeam(captureData->teamCapped) + "\"}}",
                                     getMatchTime());

                // Push the MatchEvent to the matchEvents vector stored in the officialMatch struct
                officialMatch->matchEvents.push_back(capEvent);
            }
        }
        break;

        case bz_eGameEndEvent: // This event is called each time a game ends
        {
            logMessage(pluginSettings.getVerboseLevel(), "debug", "A match has ended.");

            // Get the current standard UTC time
            bz_Time standardTime;
            bz_getUTCtime(&standardTime);
            std::string recordingFileName;

            // Only save the recording buffer if we actually started recording when the match started
            if (RECORDING)
            {
                logMessage(pluginSettings.getVerboseLevel(), "debug", "Recording was in progress during the match.");

                // We'll be formatting the file name, so create a variable to store it
                char tempRecordingFileName[512];

                // Let's get started with formatting
                if (isOfficialMatch())
                {
                    // If the official match was finished, then mark it as canceled
                    std::string matchCanceled = (officialMatch->canceled) ? "-Canceled" : "",
                                _teamOneName  = officialMatch->teamOneName.c_str(),
                                _teamTwoName  = officialMatch->teamTwoName.c_str(),
                                _matchTeams   = "";

                    // We want to standardize the names, so replace all spaces with underscores and
                    // any weird HTML symbols should have been stripped already by the PHP script
                    std::replace(_teamOneName.begin(), _teamOneName.end(), ' ', '_');
                    std::replace(_teamTwoName.begin(), _teamTwoName.end(), ' ', '_');

                    if (!officialMatch->matchParticipants.empty())
                    {
                        _matchTeams = _teamOneName + "-vs-" + _teamTwoName + "-";
                    }

                    sprintf(tempRecordingFileName, "offi-%d%02d%02d-%s%02d%02d%s.rec",
                        standardTime.year, standardTime.month, standardTime.day, _matchTeams.c_str(),
                        standardTime.hour, standardTime.minute, matchCanceled.c_str());
                }
                else
                {
                    sprintf(tempRecordingFileName, "fun-%d%02d%02d-%02d%02d.rec",
                        standardTime.year, standardTime.month, standardTime.day,
                        standardTime.hour, standardTime.minute);
                }

                // Move the char[] into a string to handle it better
                recordingFileName = tempRecordingFileName;
                logMessage(pluginSettings.getVerboseLevel(), "debug", "Replay file will be named: %s", recordingFileName.c_str());

                // Save the recording buffer and stop recording
                bz_saveRecBuf(recordingFileName.c_str(), 0);
                bz_stopRecBuf();
                logMessage(pluginSettings.getVerboseLevel(), "debug", "Replay file has been saved and recording has stopped.");

                // We're no longer recording, so set the boolean and announce to players that the file has been saved
                RECORDING = false;
                bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Match saved as: %s", recordingFileName.c_str());
            }

            if (pluginSettings.isMatchReportEnabled())
            {
                if (officialMatch == NULL)
                {
                    // It was a fun match, so there is no need to do anything

                    logMessage(pluginSettings.getDebugLevel(), "debug", "Fun match has completed.");
                }
                else if (officialMatch->canceled)
                {
                    // The match was canceled for some reason so output the reason to both the players and the server logs

                    logMessage(pluginSettings.getDebugLevel(), "debug", "%s", officialMatch->cancelationReason.c_str());
                    bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, officialMatch->cancelationReason.c_str());
                }
                else if (officialMatch->matchParticipants.empty())
                {
                    // Oops... I darn goofed. Somehow the players were not recorded properly

                    logMessage(pluginSettings.getDebugLevel(), "debug", "No recorded players for this official match.");
                    bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Official match could not be reported due to not having a list of valid match participants.");
                }
                else
                {
                    // This was an official match, so let's report it

                    // Format the date to -> year-month-day hour:minute:second
                    char matchDate[20];
                    sprintf(matchDate, "%02d-%02d-%02d %02d:%02d:%02d", standardTime.year, standardTime.month, standardTime.day, standardTime.hour, standardTime.minute, standardTime.second);

                    // Keep references to values for quick reference
                    std::string teamOnePointsFinal = std::to_string(officialMatch->teamOnePoints);
                    std::string teamTwoPointsFinal = std::to_string(officialMatch->teamTwoPoints);
                    std::string matchDuration      = std::to_string(officialMatch->duration/60);

                    // Store match data in the logs
                    bz_debugMessagef(0, "Match Data :: League Overseer Match Report");
                    bz_debugMessagef(0, "Match Data :: -----------------------------");
                    bz_debugMessagef(0, "Match Data :: Match Time      : %s", matchDate);
                    bz_debugMessagef(0, "Match Data :: Duration        : %s", matchDuration.c_str());
                    bz_debugMessagef(0, "Match Data :: %s  Score  : %s", formatTeam(TEAM_ONE, true).c_str(), teamOnePointsFinal.c_str());
                    bz_debugMessagef(0, "Match Data :: %s  Score  : %s", formatTeam(TEAM_TWO, true).c_str(), teamTwoPointsFinal.c_str());

                    MatchUrlRepo.set("query",       "matchReport")
                                .set("teamOneWins", officialMatch->teamOnePoints)
                                .set("teamTwoWins", officialMatch->teamTwoPoints)
                                .set("duration",    officialMatch->duration/60)
                                .set("matchTime",   matchDate)
                                .set("server",      bz_getPublicAddr())
                                .set("port",        bz_getPublicPort())
                                .set("replayFile",  recordingFileName);

                    // Only add this parameter if it's a rotational league such as OpenLeague
                    if (pluginSettings.isRotationalLeague())
                    {
                        MatchUrlRepo.set("mapPlayed", MAP_NAME);
                    }

                    // Build a string of BZIDs and also output the BZIDs to the server logs while we're at it
                    MatchUrlRepo.set("teamOnePlayers", buildBZIDString(TEAM_ONE));
                    MatchUrlRepo.set("teamTwoPlayers", buildBZIDString(TEAM_TWO));

                    // Finish prettifying the server logs
                    bz_debugMessagef(0, "Match Data :: -----------------------------");
                    bz_debugMessagef(0, "Match Data :: End of Match Report");
                    logMessage(pluginSettings.getDebugLevel(), "debug", "Reporting match data...");
                    bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Reporting match...");

                    // Send off the match
                    MatchUrlRepo.submit();

                    // Send the match data to the league website
                    MATCH_INFO_SENT = true;
                }
            }

            // We're done with the struct, so make it NULL until the next official match
            officialMatch = NULL;

            // Empty our list of players since we don't need a history
            activePlayerList.clear();
        }
        break;

        case bz_eGamePauseEvent:
        {
            bz_GamePauseResumeEventData_V1* gamePauseData = (bz_GamePauseResumeEventData_V1*)eventData;

            // Data
            // ---
            //    (bz_ApiString) actionBy  - The callsign of whoever triggered the event. By default, it's "SERVER"
            //    (double)       eventTime - The server time the event occurred (in seconds).

            // Get the current UTC time
            MATCH_PAUSED = time(NULL);

            // We've paused an official match, so we need to delay the approxTimeProgress in order to calculate the roll call time properly
            if (isOfficialMatch())
            {
                // Send the messages
                bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "    with %s remaining.", getMatchTime().c_str());
                logMessage(pluginSettings.getVerboseLevel(), "debug", "Match paused at %s by %s.", getMatchTime().c_str(), gamePauseData->actionBy.c_str());

                // Create a player record of the person who captured the flag
                std::shared_ptr<bz_BasePlayerRecord> playerData(bz_getPlayerByCallsign(gamePauseData->actionBy.c_str()));

                // Create a MatchEvent with the information relating to the capture
                MatchEvent pauseEvent(playerData->playerID, std::string(playerData->bzID.c_str()),
                                     std::string(playerData->callsign.c_str()) + " paused the match at " + getMatchTime(),
                                     "{\"event\": {\"type\": \"pause\"}}",
                                     getMatchTime());

                // Push the MatchEvent to the matchEvents vector stored in the officialMatch struct
                officialMatch->matchEvents.push_back(pauseEvent);
            }
        }
        break;

        case bz_eGameResumeEvent:
        {
            bz_GamePauseResumeEventData_V1* gameResumeData = (bz_GamePauseResumeEventData_V1*)eventData;

            // Data
            // ---
            //    (bz_ApiString) actionBy  - The callsign of whoever triggered the event. By default, it's "SERVER"
            //    (double)       eventTime - The server time the event occurred (in seconds).


            // Get the current UTC time
            time_t now = time(NULL);

            // Do the math to determine how long the match was paused
            double timePaused = difftime(now, MATCH_PAUSED);

            // Create a temporary variable to store and manipulate the match start time
            struct tm modMatchStart = *localtime(&MATCH_START);

            // Manipulate the time by adding the amount of seconds the match was paused in order to be able to accurately
            // calculate the amount of time remaining with LeagueOverseer::getMatchTime()
            modMatchStart.tm_sec += timePaused;

            // Save the manipulated match start time
            MATCH_START = mktime(&modMatchStart);
            logMessage(pluginSettings.getVerboseLevel(), "debug", "Match paused for %.f seconds. Match continuing at %s.", timePaused, getMatchTime().c_str());

            // We've resumed an official match, so we need to properly edit the start time so we can calculate the roll call
            if (isOfficialMatch())
            {
                // Create a player record of the person who captured the flag
                std::shared_ptr<bz_BasePlayerRecord> playerData(bz_getPlayerByCallsign(gameResumeData->actionBy.c_str()));

                // Create a MatchEvent with the information relating to the capture
                MatchEvent resumeEvent(playerData->playerID, std::string(playerData->bzID.c_str()),
                                     std::string(playerData->callsign.c_str()) + " resumed the match",
                                     "{\"event\": {\"type\": \"resume\"}}",
                                     getMatchTime());

                // Push the MatchEvent to the matchEvents vector stored in the officialMatch struct
                officialMatch->matchEvents.push_back(resumeEvent);
            }
        }
        break;

        case bz_eGameStartEvent: // This event is triggered when a timed game begins
        {
            logMessage(pluginSettings.getVerboseLevel(), "debug", "A match has started");

            // Empty our list of players since we don't need a history
            activePlayerList.clear();

            // We started recording a match, so save the status
            RECORDING = bz_startRecBuf();

            // We want to notify the logs if we couldn't start recording just in case an issue were to occur and the server
            // owner needs to check to see if players were lying about there no replay
            if (RECORDING)
            {
                logMessage(pluginSettings.getVerboseLevel(), "debug", "Match recording has started successfully");
            }
            else
            {
                logMessage(0, "error", "This match could not be recorded");
            }

            // Check if this is an official match
            if (isOfficialMatch())
            {
                // Reset scores in case Caps happened during countdown delay.
                officialMatch->teamOnePoints = officialMatch->teamTwoPoints = 0;
                officialMatch->duration = bz_getTimeLimit();
            }

            MATCH_START = time(NULL);
        }
        break;

        case bz_eGetAutoTeamEvent: // This event is called for each new player is added to a team
        {
            bz_GetAutoTeamEventData_V1* autoTeamData = (bz_GetAutoTeamEventData_V1*)eventData;

            // Data
            // ---
            //    (int)           playerID  - ID of the player that is being added to the game.
            //    (bz_ApiString)  callsign  - Callsign of the player that is being added to the game.
            //    (bz_eTeamType)  team      - The team that the player will be added to. Initialized to the team chosen by the
            //                                current server team rules, or the effects of a plug-in that has previously processed
            //                                the event. Plug-ins wishing to override the team should set this value.
            //    (bool)          handled   - The current state representing if other plug-ins have modified the default team.
            //                                Plug-ins that modify the team should set this value to true to inform other plug-ins
            //                                that have not processed yet.
            //    (double)        eventTime - This value is the local server time of the event.

            int playerID = autoTeamData->playerID;
            std::shared_ptr<bz_BasePlayerRecord> playerData(bz_getPlayerByIndex(playerID));

            // Only force new players to observer if a match is in progress
            if (isMatchInProgress())
            {
                // Automatically move non-league members or players who just joined to the observer team
                if (!isLeagueMember(playerID) || !playerAlreadyJoined(playerData->bzID.c_str()))
                {
                    autoTeamData->handled = true;
                    autoTeamData->team    = eObservers;
                }
            }
        }
        break;

        case bz_eGetPlayerMotto: // This event is called when the player joins. It gives us the motto of the player
        {
            bz_GetPlayerMottoData_V2* mottoData = (bz_GetPlayerMottoData_V2*)eventData;

            // Data
            // ---
            //    (bz_ApiString)         motto     - The motto of the joining player. This value may ve overwritten to change the motto of a player.
            //    (bz_BasePlayerRecord)  record    - The player record for the player using the motto.
            //    (double)               eventTime - The server time the event occurred (in seconds).

            if (pluginSettings.isMottoFetchEnabled())
            {
                mottoData->motto = getPlayerTeamNameByBZID(mottoData->record->bzID.c_str());
            }
        }
        break;

        case bz_ePlayerJoinEvent: // This event is called each time a player joins the game
        {
            bz_PlayerJoinPartEventData_V1* joinData = (bz_PlayerJoinPartEventData_V1*)eventData;

            // Data
            // ---
            //    (int)                   playerID  - The player ID that is joining
            //    (bz_BasePlayerRecord*)  record    - The player record for the joining player
            //    (double)                eventTime - Time of event.

            int playerID = joinData->playerID;
            std::shared_ptr<bz_BasePlayerRecord> playerData(bz_getPlayerByIndex(playerID));

            setLeagueMember(playerID);
            storePlayerInfo(playerID, playerData->bzID.c_str(), playerData->callsign.c_str());

            JoinMatchEvent joinEvent = JoinMatchEvent().setCallsign(playerData->callsign.c_str())
                                                       .setVerified(playerData->verified)
                                                       .setIpAddress(playerData->ipAddress.c_str())
                                                       .setBZID(playerData->bzID.c_str())
                                                       .save();

            // Only notify a player if they exist, have joined the observer team, and there is a match in progress
            if (isMatchInProgress() && isValidPlayerID(joinData->playerID) && playerData->team == eObservers)
            {
                bz_sendTextMessagef(BZ_SERVER, joinData->playerID, "*** There is currently %s match in progress, please be respectful. ***",
                                    ((isOfficialMatch()) ? "an official" : "a fun"));
            }

            if (pluginSettings.isMottoFetchEnabled())
            {
                // Only send a URL job if the user is verified
                if (playerData->verified)
                {
                    requestTeamName(playerData->callsign.c_str(), playerData->bzID.c_str());
                }
            }
        }
        break;

        case bz_ePlayerPartEvent: // This event is called each time a player leaves a game
        {
            bz_PlayerJoinPartEventData_V1* partData = (bz_PlayerJoinPartEventData_V1*)eventData;

            // Data
            // ---
            //    (int)                   playerID  - The player ID that is leaving
            //    (bz_BasePlayerRecord*)  record    - The player record for the leaving player
            //    (bz_ApiString)          reason    - The reason for leaving, such as a kick or a ban
            //    (double)                eventTime - Time of event.

            int playerID = partData->playerID;
            std::shared_ptr<bz_BasePlayerRecord> playerData(bz_getPlayerByIndex(playerID));

            removePlayerInfo(partData->record->bzID.c_str(), partData->record->callsign.c_str());

            // Only keep track of the parting player if they are a league member and there is a match in progress
            if (isLeagueMember(playerID) && isMatchInProgress())
            {
                if (!playerAlreadyJoined(playerData->bzID.c_str()))
                {
                    // Create a record for the player who just left
                    Player partingPlayer(playerData->bzID.c_str(), playerData->team, bz_getCurrentTime());

                    // Push the record to our vector
                    activePlayerList.push_back(partingPlayer);
                }
            }
        }
        break;

        case bz_eRawChatMessageEvent: // This event is called for each chat message the server receives. It is called before any filtering is done.
        {
            bz_ChatEventData_V1* chatData  = (bz_ChatEventData_V1*)eventData;

            // Data
            // ---
            //    (int)           from      - The player ID sending the message.
            //    (int)           to        - The player ID that the message is to if the message is to an individual, or a
            //                                broadcast. If the message is a broadcast the id will be BZ_ALLUSERS.
            //    (bz_eTeamType)  team      - The team the message is for if it not for an individual or a broadcast. If it
            //                                is not a team message the team will be eNoTeam.
            //    (bz_ApiString)  message   - The filtered final text of the message.
            //    (double)        eventTime - The time of the event.

            bz_eTeamType target    = chatData->team;
            int          playerID  = chatData->from,
                         recipient = chatData->to;

            // The server is always allowed to talk
            if (playerID == BZ_SERVER)
            {
                break;
            }

            // A non-league player is attempting to talk
            if (!isLeagueMember(playerID))
            {
                if (pluginSettings.isAllowLimitedChat()) // Are non-league members allowed limited talking functionality
                {
                    if (isMatchInProgress()) // A match is progress
                    {
                        if (bz_getPlayerTeam(playerID) == eObservers) // Only consider allowing non-league members to talk if they are in the observer team
                        {
                            // Only allow non-league members to talk if they're talking to the observer team chat or private messaging a player in the observer team
                            // this precaution is so non-league players do not private message players participating in a match, do not message an admin who may
                            // playing a match, and do not send messages to public chat to avoid match disturbances
                            if (target != eObservers || bz_getPlayerTeam(recipient) != eObservers)
                            {
                                chatData->message = ""; // We set the message to nothing so they won't send thing anything
                                sendPluginMessage(playerID, pluginSettings.isTalkMessageEnabled(), pluginSettings.getNoTalkMessage(), CHAT);
                            }
                        }
                        else // If they aren't in the observer team during a match, don't let them talk
                        {
                            chatData->message = "";
                            sendPluginMessage(playerID, pluginSettings.isTalkMessageEnabled(), pluginSettings.getNoTalkMessage(), CHAT);
                        }
                    }
                    else // A match is not in progress
                    {
                        if (target != eAdministrators)
                        {
                            chatData->message = "";
                            sendPluginMessage(playerID, pluginSettings.isTalkMessageEnabled(), pluginSettings.getNoTalkMessage(), CHAT);
                        }
                    }
                }
                else // Non-league members are not allowed limited talk functionality
                {
                    chatData->message = "";
                    sendPluginMessage(playerID, pluginSettings.isTalkMessageEnabled(), pluginSettings.getNoTalkMessage(), CHAT); // Send them a message
                }
            }
        }
        break;

        case bz_eTickEvent: // This event is called once for each BZFS main loop
        {
            // Get the total number of tanks playing
            int totaltanks = bz_getTeamCount(eRedTeam) + bz_getTeamCount(eGreenTeam) + bz_getTeamCount(eBlueTeam) + bz_getTeamCount(ePurpleTeam);

            // If there are no tanks playing, then we need to do some clean up
            if (totaltanks == 0)
            {
                // If there is an official match and no tanks playing, we need to cancel it
                if (isOfficialMatch())
                {
                    officialMatch->canceled = true;
                    officialMatch->cancelationReason = "Official match automatically canceled due to all players leaving the match.";
                }

                // If we have players recorded and there's no one around, empty the list
                if (!activePlayerList.empty())
                {
                    activePlayerList.clear();
                }

                // If there is a countdown active an no tanks are playing, then cancel it
                if (bz_isCountDownActive())
                {
                    bz_gameOver(253, eObservers);
                    logMessage(pluginSettings.getVerboseLevel(), "debug", "Game ended because no players were found playing with an active countdown.");
                }
            }

            // Let's get the roll call only if there is an official match
            if (isOfficialMatch())
            {
                // Check if the start time is not negative since our default value for the approxTimeProgress is -1. Also check
                // if it's time to do a roll call, which is defined as 90 seconds after the start of the match by default,
                // and make sure we don't have any match participants recorded and the match isn't paused
                if (getMatchProgress() > officialMatch->matchRollCall && officialMatch->matchParticipants.empty() &&
                    !bz_isCountDownPaused() && !bz_isCountDownInProgress())
                {
                    logMessage(pluginSettings.getVerboseLevel(), "debug", "Processing roll call...");

                    std::shared_ptr<bz_APIIntList> playerList(bz_getPlayerIndexList());
                    bool invalidateRollcall, teamOneError, teamTwoError;
                    std::string teamOneMotto, teamTwoMotto;

                    invalidateRollcall = teamOneError = teamTwoError = false;
                    teamOneMotto = teamTwoMotto = "";

                    // We can't do a roll call if the player list wasn't created
                    if (!playerList)
                    {
                        logMessage(pluginSettings.getVerboseLevel(), "error", "Failure to create player list for roll call.");
                        return;
                    }

                    for (unsigned int i = 0; i < playerList->size(); i++)
                    {
                        std::shared_ptr<bz_BasePlayerRecord> playerRecord(bz_getPlayerByIndex(playerList->get(i)));

                        if (playerRecord && isLeagueMember(playerRecord->playerID) && bz_getPlayerTeam(playerList->get(i)) != eObservers) // If player is not an observer
                        {
                            MatchParticipant currentPlayer(playerRecord->bzID.c_str(), playerRecord->callsign.c_str(),
                                                           playerRecord->ipAddress.c_str(), teamMottos[playerRecord->bzID.c_str()],
                                                           playerRecord->team);

                            // In order to see what is going wrong with the roll call if anything, display all of the player's information
                            logMessage(pluginSettings.getVerboseLevel(), "debug", "Adding player '%s' to roll call...", currentPlayer.callsign.c_str());
                            logMessage(pluginSettings.getVerboseLevel(), "debug", "  >  BZID       : %s", currentPlayer.bzID.c_str());
                            logMessage(pluginSettings.getVerboseLevel(), "debug", "  >  IP Address : %s", currentPlayer.ipAddress.c_str());
                            logMessage(pluginSettings.getVerboseLevel(), "debug", "  >  Team Name  : %s", currentPlayer.teamName.c_str());
                            logMessage(pluginSettings.getVerboseLevel(), "debug", "  >  Team Color : %s", formatTeam(currentPlayer.teamColor).c_str());

                            // Check if there is any need to invalidate a roll call from a team
                            validateTeamName(invalidateRollcall, teamOneError, currentPlayer, teamOneMotto, TEAM_ONE);
                            validateTeamName(invalidateRollcall, teamTwoError, currentPlayer, teamTwoMotto, TEAM_TWO);

                            if (currentPlayer.bzID.empty()) // Someone is playing without a BZID, how did this happen?
                            {
                                invalidateRollcall = true;
                                logMessage(pluginSettings.getVerboseLevel(), "error", "Roll call has been marked as invalid due to '%s' not having a valid BZID.", currentPlayer.callsign.c_str());
                            }

                            // Add the player to the struct of participants
                            officialMatch->matchParticipants.push_back(currentPlayer);
                            logMessage(pluginSettings.getVerboseLevel(), "debug", "Player '%s' successfully added to the roll call.", currentPlayer.callsign.c_str());
                        }
                    }

                    // We were asked to invalidate the roll call because of some issue so let's check if there is still time for
                    // another roll call
                    if (invalidateRollcall && officialMatch->matchRollCall + 60 < officialMatch->duration)
                    {
                        logMessage(pluginSettings.getDebugLevel(), "debug", "Invalid player found on field at %s.", getMatchTime().c_str());

                        // There was an error with one of the members of either team, so request a team name update for all of
                        // the team members to try to fix any inconsistencies of different team names
                        if (teamOneError) { requestTeamName(TEAM_ONE); }
                        if (teamTwoError) { requestTeamName(TEAM_TWO); }

                        // Delay the next roll call by 60 seconds
                        officialMatch->matchRollCall += 60;
                        logMessage(pluginSettings.getVerboseLevel(), "debug", "Match roll call time has been delayed by 60 seconds.");

                        // Clear the struct because it's useless data
                        officialMatch->matchParticipants.clear();
                        logMessage(pluginSettings.getVerboseLevel(), "debug", "Match participants have been cleared.");
                    }

                    // There is no need to invalidate the roll call so the team names must be right so save them in the struct
                    if (!invalidateRollcall)
                    {
                        officialMatch->teamOneName = teamOneMotto;
                        officialMatch->teamTwoName = teamTwoMotto;

                        logMessage(pluginSettings.getVerboseLevel(), "debug", "Team One set to: %s", officialMatch->teamOneName.c_str());
                        logMessage(pluginSettings.getVerboseLevel(), "debug", "Team Two set to: %s", officialMatch->teamTwoName.c_str());
                    }
                }
            }
        }
        break;

        default: break;
    }
}