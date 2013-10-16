/*
League Over Seer Plug-in
    Copyright (C) 2013 Vladimir Jimenez & Ned Anderson

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
#include <cctype>
#include <fstream>
#include <iostream>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>

#include "bzfsAPI.h"
#include "plugin_utils.h"

//Define plugin version numbering
const int MAJOR = 1;
const int MINOR = 1;
const int REV = 0;
const int BUILD = 183;

// Log failed assertions at debug level 0 since this will work for non-member functions and it is important enough.
#define ASSERT(x) { if (!(x)) bz_debugMessagef(0, "DEBUG :: League Over Seer :: Failed assertion '%s' at %s:%d", #x, __FILE__, __LINE__); }

//Custom functions
//Functions that don't use the class variables have no need to be in the class.

/* ==== int functions ==== */

int getPlayerByCallsign(std::string callsign)
{
    std::unique_ptr<bz_APIIntList> list(bz_getPlayerIndexList());
    for (unsigned int i = 0; i < list->size(); i++) //Go through all the players
    {
      if (bz_getPlayerCallsign(list->get(i)) == callsign)
        return list->get(i);
    }
    return -1;
}

static std::string getString(int number) //Convert an integer into a string
{
    std::stringstream string;
    string << number;

    return string.str();
}

static std::string formatTeam(bz_eTeamType teamColor, bool addWhiteSpace)
{
    std::string color;

    switch (teamColor)
    {
        case eBlueTeam:
            color = "Blue";
            break;

        case eGreenTeam:
            color = "Green";
            break;

        case ePurpleTeam:
            color = "Purple";
            break;

        case eRedTeam:
            color = "Red";
            break;

        default:
            break;
    }

    if (addWhiteSpace)
        while (color.length() < 7)
          color += " ";

    return color;
}

bool isValidPlayerID(int playerID)
{
    std::unique_ptr<bz_APIIntList> list(bz_getPlayerIndexList());
    for (unsigned int i = 0; i < list->size(); i++)
    {
      if (list->get(i) == playerID) {
        return true;
      }
    }
    return false;
}

bool toBool(std::string var) //Turn std::string into a boolean value
{
    if (var == "true" || var == "TRUE" || var == "1") //If the value is true then the boolean is true
        return true;
    else //If it's not true, then it's false.
        return false;
}

class leagueOverSeer : public bz_Plugin, public bz_CustomSlashCommandHandler, public bz_BaseURLHandler
{
    virtual const char* Name ()
    {
        char buffer[100];
        sprintf(buffer, "League Over Seer %i.%i.%i (%i)", MAJOR, MINOR, REV, BUILD);
        return std::string(buffer).c_str();
    }
    virtual void Init(const char* config);
    virtual void Event(bz_EventData *eventData);
    virtual bool SlashCommand(int playerID, bz_ApiString, bz_ApiString, bz_APIStringList*);
    virtual void Cleanup();
    virtual void URLDone(const char* URL, const void* data, unsigned int size, bool complete);
    virtual void URLTimeout(const char* URL, int errorCode);
    virtual void URLError(const char* URL, int errorCode, const char *errorString);

    virtual const char* getMotto(const char* bzid);

    virtual std::string buildBZIDString(bz_eTeamType team);

    virtual void loadConfig(const char *cmdLine);
    virtual void updateTeamNames(void);

    struct MatchParticipant
    {
        std::string bzID;
        std::string callsign;
        std::string teamName;
        bz_eTeamType team;

        MatchParticipant(std::string _bzID, std::string _callsign, std::string _teamName, bz_eTeamType _teamColor) :
            bzID(_bzid),
            callsign(_callsign),
            teamName(_teamName),
            teamColor(_teamColor)
    };

    struct OfficialMatch
    {
        bool        playersRecorded,
                    canceled;

        std::string cancelationReason;

        double      startTime,
                    duration;

        int         teamOnePoints,
                    teamTwoPoints;

        std::vector<MatchParticipant> matchParticipants;

        OfficialMatch() :
            canceled(false),
            cancelationReason(""),
            playersRecorded(false),
            startTime(-1.0f),
            duration(-1.0f),
            teamOnePoints(0),
            teamTwoPoints(0),
            matchParticipants() {}
    };

    //All the variables that will be used in the plugin
    bool         ROTATION_LEAGUE;

    int          DEBUG_LEVEL;

    double       MATCH_ROLLCALL;

    std::string  LEAGUE_URL,
                 MAP_NAME,
                 MAPCHANGE_PATH;

    bz_eTeamType TEAM_ONE,
                 TEAM_TWO;

    // NULL if a fun match
    std::unique_ptr<OfficialMatch> officialMatch;

    // Handle team names for players
    typedef std::map<std::string, std::string> TeamNameMottoMap;
    TeamNameMottoMap teamMottos;
};

BZ_PLUGIN(leagueOverSeer)

void leagueOverSeer::Init (const char* commandLine)
{
    bz_debugMessagef(0, "League Over Seer %i.%i.%i (%i) loaded.", MAJOR, MINOR, REV, BUILD);

    Register(bz_eCaptureEvent);
    Register(bz_eGameEndEvent);
    Register(bz_eGameStartEvent);
    Register(bz_eGetPlayerMotto);
    Register(bz_ePlayerJoinEvent);
    Register(bz_eTickEvent);

    bz_registerCustomSlashCommand("official", this);
    bz_registerCustomSlashCommand("fm", this);
    bz_registerCustomSlashCommand("finish", this);
    bz_registerCustomSlashCommand("cancel", this);
    bz_registerCustomSlashCommand("spawn", this);
    bz_registerCustomSlashCommand("resume", this);
    bz_registerCustomSlashCommand("pause", this);

    //Set some default values
    MATCH_ROLLCALL = 90;
    officialMatch = NULL;

    loadConfig(commandLine); //Load the configuration data when the plugin is loaded

    if (MAPCHANGE_PATH != "" && ROTATION_LEAGUE) //Check to see if the plugin is for a rotational league
    {
        //Open the mapchange.out file to see what map is being used
        std::ifstream infile;
        infile.open(MAPCHANGE_PATH.c_str());
        getline(infile, MAP_NAME);
        infile.close();

        MAP_NAME = MAP_NAME.substr(0, MAP_NAME.length() - 5); //Remove the '.conf' from the mapchange.out file

        bz_debugMessagef(DEBUG_LEVEL, "DEBUG :: League Over Seer :: Current map being played: %s", MAP_NAME.c_str());
    }

    TEAM_ONE = eNoTeam;
    TEAM_TWO = eNoTeam;

    for (bz_eTeamType t = eRedTeam; t <= ePurpleTeam; t = (bz_eTeamType) (t + 1))
    {
        if (bz_getTeamPlayerLimit(t) > 0)
        {
            if (TEAM_ONE == eNoTeam)
                TEAM_ONE = t;
            else if (TEAM_TWO == eNoTeam)
                TEAM_TWO = t;
        }
    }

    ASSERT(TEAM_ONE != eNoTeam && TEAM_TWO != eNoTeam);

    updateTeamNames();
}

void leagueOverSeer::Cleanup (void)
{
    bz_debugMessagef(0, "League Over Seer %i.%i.%i (%i) unloaded.", MAJOR, MINOR, REV, BUILD);

    Flush(); // Clean up all the events

    // Remove all the slash commands
    bz_removeCustomSlashCommand("official");
    bz_removeCustomSlashCommand("fm");
    bz_removeCustomSlashCommand("finish");
    bz_removeCustomSlashCommand("cancel");
    bz_removeCustomSlashCommand("spawn");
    bz_removeCustomSlashCommand("resume");
    bz_removeCustomSlashCommand("pause");
}

void leagueOverSeer::Event(bz_EventData *eventData)
{
    switch (eventData->eventType)
    {
        case bz_eCaptureEvent: //Someone caps
        {
            if (officialMatch != NULL)
            {
                bz_CTFCaptureEventData_V1 *capData = (bz_CTFCaptureEventData_V1*)eventData;
                (capData->teamCapping == TEAM_ONE) ? officialMatch->teamOnePoints++ : officialMatch->teamTwoPoints++;
            }
        }
        break;

        case bz_eGameEndEvent: //A /gameover or a match has ended
        {
            // Match is created by command before GameStart so it must still exist.
            ASSERT(officialMatch);

            // If an FM do nothing...
            if (officialMatch == NULL)
            {
                bz_debugMessage(DEBUG_LEVEL, "DEBUG :: League Over Seer :: Fun match was not reported.");
                break;
            }

            if (officialMatch->canceled)
            {
                bz_debugMessagef(DEBUG_LEVEL, "DEBUG :: League Over Seer :: %s", officialMatch->cancelationReason.c_str());
                bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, officialMatch->cancelationReason.c_str());
            }
            else if (officialMatch->matchParticipants.empty())
            {
                bz_debugMessage(DEBUG_LEVEL, "DEBUG :: League Over Seer :: No recorded players for this official match.");
                bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Official match could not be reported due to not having a list of valid match participants.");
            }
            else
            {
                bz_Time standardTime;
                bz_getUTCtime(&standardTime);

                //Format the date to -> year-month-day hour:minute:second
                char match_date[20];
                sprintf(match_date, "%02d-%02d-%02d %02d:%02d:%02d", standardTime.year, standardTime.month, standardTime.day, standardTime.hour, standardTime.minute, standardTime.second);

                //Keep references to values for quick reference
                std::string teamOnePointsFinal = getString(officialMatch->teamOnePoints);
                std::string teamTwoPointsFinal = getString(officialMatch->teamTwoPoints);
                std::string matchTimeFinal     = getString(officialMatch->duration/60);

                // Store match data in the logs
                bz_debugMessagef(DEBUG_LEVEL, "Match Data :: League Over Seer Match Report");
                bz_debugMessagef(DEBUG_LEVEL, "Match Data :: -----------------------------");
                bz_debugMessagef(DEBUG_LEVEL, "Match Data :: Match Time      : %s", match_date);
                bz_debugMessagef(DEBUG_LEVEL, "Match Data :: Duration        : %s", matchTimeFinal.c_str());
                bz_debugMessagef(DEBUG_LEVEL, "Match Data :: %s  Score  : %s", formatTeam(TEAM_ONE, true).c_str(), teamOnePointsFinal.c_str());
                bz_debugMessagef(DEBUG_LEVEL, "Match Data :: %s  Score  : %s", formatTeam(TEAM_TWO, true).c_str(), teamTwoPointsFinal.c_str());

                // Start building POST data to be sent to the league website
                std::string matchToSend = "query=reportMatch";
                            matchToSend += "&teamOneWins=" + std::string(bz_urlEncode(teamOnePointsFinal.c_str()));
                            matchToSend += "&teamTwoWins=" + std::string(bz_urlEncode(teamTwoPointsFinal.c_str()));
                            matchToSend += "&duration="    + std::string(bz_urlEncode(matchTimeFinal.c_str()));
                            matchToSend += "&matchTime="   + std::string(bz_urlEncode(match_date));

                if (ROTATION_LEAGUE) //Only add this parameter if it's a rotational league such as Open League
                    matchToSend += "&mapPlayed=" + std::string(bz_urlEncode(MAP_NAME.c_str()));

                matchToSend += "&teamOnePlayers=" + buildBZIDString(TEAM_ONE);
                matchToSend += "&teamTwoPlayers=" + buildBZIDString(TEAM_TWO);

                bz_debugMessagef(DEBUG_LEVEL, "Match Data :: -----------------------------");
                bz_debugMessagef(DEBUG_LEVEL, "Match Data :: End of Match Report");
                bz_debugMessagef(DEBUG_LEVEL, "DEBUG :: League Over Seer :: Reporting match data...");
                bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Reporting match...");

                //Send the match data to the league website
                bz_addURLJob(LEAGUE_URL.c_str(), this, matchToSend.c_str());
            }

            officialMatch = NULL;
        }
        break;

        case bz_eGameStartEvent: //The countdown has started
        {
            if (officialMatch != NULL)
            {
                // Reset scores in case Caps happened during countdown delay.
                officialMatch->teamOnePoints = officialMatch->teamTwoPoints = 0;
                officialMatch->startTime = bz_getCurrentTime();
                officialMatch->duration = bz_getTimeLimit();
            }
        }
        break;

        case bz_eGetPlayerMotto: // Change the motto of a player when they join
        {
            bz_GetPlayerMottoData_V2* mottoEvent = (bz_GetPlayerMottoData_V2*)eventData;

            mottoEvent->motto = getMotto(mottoEvent->record->bzID.c_str());
        }

        case bz_ePlayerJoinEvent: //A player joins
        {
            bz_PlayerJoinPartEventData_V1 *joinData = (bz_PlayerJoinPartEventData_V1*)eventData;

            if (!joinData)
                return;

            if ((bz_isCountDownActive() || bz_isCountDownInProgress()) && isValidPlayerID(joinData->playerID) && joinData->record->team == eObservers)
            {
                bz_sendTextMessagef(BZ_SERVER, joinData->playerID, "*** There is currently %s match in progress, please be respectful. ***",
                                    ((officialMatch != NULL) ? "an official" : "a fun"));
            }

            if (joinData->record->verified)
            {
                // Build the POST data for the URL job
                std::string teamMotto = "query=teamNameQuery";
                teamMotto += "&teamPlayers=" + std::string(joinData->record->bzID.c_str());

                bz_debugMessagef(DEBUG_LEVEL, "DEBUG :: League Over Seer :: Getting motto for %s...", joinData->record->callsign.c_str());

                bz_addURLJob(LEAGUE_URL.c_str(), this, teamMotto.c_str()); //Send the team update request to the league website
            }
        }
        break;

        case bz_eSlashCommandEvent: //Someone uses a slash command
        {
            bz_SlashCommandEventData_V1 *commandData = (bz_SlashCommandEventData_V1*)eventData;
            std::unique_ptr<bz_BasePlayerRecord> playerData(bz_getPlayerByIndex(commandData->from));
            std::string command = commandData->message.c_str(); //Use std::string for quick reference

            if (strncmp("/gameover", command.c_str(), 9) == 0)
                bz_sendTextMessagef(BZ_SERVER, commandData->from, "** '/gameover' is disabled, please use /finish or /cancel instead **");
            else if (strncmp("/countdown pause", command.c_str(), 16) == 0)
                bz_sendTextMessagef(BZ_SERVER, commandData->from, "** '/countdown pause' is disabled, please use /pause instead **");
            else if (strncmp("/countdown resume", command.c_str(), 17 ) == 0)
                bz_sendTextMessagef(BZ_SERVER, commandData->from, "** '/countdown resume' is disabled, please use /resume instead **");
            else if (isdigit(atoi(command.c_str()) + 12))
                bz_sendTextMessage(BZ_SERVER, commandData->from, "** '/countdown TIME' is disabled, please use /official or /fm instead **");
        }
        break;

        case bz_eTickEvent: //Tick tock tick tock...
        {
            int totaltanks = bz_getTeamCount(eRedTeam) + bz_getTeamCount(eGreenTeam) + bz_getTeamCount(eBlueTeam) + bz_getTeamCount(ePurpleTeam);

            if (totaltanks == 0)
            {
                if (officialMatch != NULL)
                {
                    officialMatch->canceled = true;
                    officialMatch->cancelationReason = "Official match automatically canceled due to all players leaving the match.";
                }

                if (bz_isCountDownActive())
                {
                    bz_gameOver(253, eObservers);
                }
            }

            if (officialMatch != NULL)
            {
                if (officialMatch->startTime >= 0.0f &&
                    officialMatch->startTime + MATCH_ROLLCALL < bz_getCurrentTime() &&
                    officialMatch->matchParticipants.empty())
                {
                    bool invalidateRollcall = false;
                    std::unique_ptr<bz_APIIntList> playerList(bz_getPlayerIndexList());
                    std::string teamOneMotto = teamTwoMotto = "";

                    for (unsigned int i = 0; i < playerList->size(); i++)
                    {
                        std::unique_ptr<bz_BasePlayerRecord> playerRecord(bz_getPlayerByIndex(playerList->get(i)));

                        if (bz_getPlayerTeam(playerList->get(i)) != eObservers) //If player is not an observer
                        {
                            MatchParticipant currentPlayer(playerRecord->bzID.c_str(), playerRecord->callsign.c_str(), getMotto(playerRecord->bzID.c_str()), playerRecord->team);

                            if (currentPlayer.team == TEAM_ONE)
                            {
                                if (teamOneMotto == "")
                                {
                                    teamOneMotto = currentPlayer.teamName;
                                }
                                else if (teamOneMotto != currentPlayer.teamName)
                                {
                                    invalidateRollcall = true;
                                    break;
                                }
                            }
                            else if (currentPlayer.team == TEAM_TWO)
                            {
                                if (teamTwoMotto == "")
                                {
                                    teamTwoMotto = currentPlayer.teamName;
                                }
                                else if (teamTwoMotto != currentPlayer.teamName)
                                {
                                    invalidateRollcall = true;
                                    break;
                                }
                            }
                            else if (currentPlayer.bzid.empty())
                            {
                                invalidateRollcall = true;
                                break;
                            }

                            officialMatch->matchParticipants.push_back(currentPlayer);
                        }
                    }

                    if (invalidateRollcall && MATCH_ROLLCALL < officialMatch->duration)
                    {
                        bz_debugMessagef(DEBUG_LEVEL, "DEBUG :: League Over Seer :: Invalid player found on field at %i:%i.", (int)(MATCH_ROLLCALL/60), (int)(fmod(MATCH_ROLLCALL,60.0)));
                        MATCH_ROLLCALL += 30;
                        match->matchPlayers.clear();
                    }
                }
            }
        }
        break;

        default:
        break;
    }
}

bool leagueOverSeer::SlashCommand(int playerID, bz_ApiString command, bz_ApiString /*message*/, bz_APIStringList *params)
{
    std::unique_ptr<bz_BasePlayerRecord> playerData(bz_getPlayerByIndex(playerID));

    if (!playerData->verified || !bz_hasPerm(playerID,"spawn"))
    {
        bz_sendTextMessagef(BZ_SERVER, playerID, "You do not have permission to run the /%s command.", command.c_str());
    }

    if (command == "official") //Someone used the /official command
    {
        if (playerData->team == eObservers) //Observers can't start matches
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "Observers are not allowed to start matches.");
        }
        else if (bz_getTeamCount(teamOne) < 2 || bz_getTeamCount(teamTwo) < 2) //An official match cannot be 1v1 or 2v1
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "You may not have an official match with less than 2 players per team.");
        }
        else if (officialMatch != NULL || bz_isCountDownActive() || bz_isCountDownInProgress()) //A countdown is in progress already
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "There is already a game in progress; you cannot start another.");
        }
        else //They are verified non-observer with valid team sizes and no existing match. Start one!
        {
            officialMatch.reset(new OfficialMatch()); //It's an official match

            bz_debugMessagef(DEBUG_LEVEL, "DEBUG :: League Over Seer :: Official match started by %s (%s).", playerData->callsign.c_str(), playerData->ipAddress.c_str());
            bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Official match started by %s.", playerData->callsign.c_str());

            int timeToStart = (params->size() == 1) ? atoi(params->get(0).c_str()) : 10;

            if (timeToStart <= 120 && timeToStart >= 5)
            {
                bz_startCountdown (timeToStart, bz_getTimeLimit(), "Server"); //Start the countdown with a custom countdown time limit under 2 minutes
            }
            else
            {
                bz_startCountdown (10, bz_getTimeLimit(), "Server"); //Start the countdown for the official match
            }
        }

        return true;
    }
    else if (command == "fm") //Someone uses the /fm command
    {
        if (playerData->team == eObservers) //Observers can't start matches
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "Observers are not allowed to start matches.");
        }
        else if (match || bz_isCountDownActive() || bz_isCountDownInProgress()) //There is already a countdown
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "There is already a game in progress; you cannot start another.");
        }
        else //They are verified, not an observer, there is no match so start one!
        {
            officialMatch = NULL;

            bz_debugMessagef(DEBUG_LEVEL, "DEBUG :: League Over Seer :: Fun match started by %s (%s).", playerData->callsign.c_str(), playerData->ipAddress.c_str());
            bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Fun match started by %s.", playerData->callsign.c_str());

            int timeToStart = (params->size() == 1) ? atoi(params->get(0).c_str()) : 10;

            if (timeToStart <= 120 && timeToStart >= 5)
            {
                bz_startCountdown (timeToStart, bz_getTimeLimit(), "Server"); //Start the countdown with a custom countdown time limit under 2 minutes
            }
            else
            {
                bz_startCountdown (10, bz_getTimeLimit(), "Server"); //Start the countdown for the official match
            }
        }

        return true;
    }
    else if (command == "cancel")
    {
        if (playerData->team == eObservers) //Observers can't cancel matches
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "Observers are not allowed to cancel matches.");
        }
        else if (bz_isCountDownInProgress())
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "You may only cancel a match after it has started.");
        }
        else if (bz_isCountDownActive()) //Cannot cancel during countdown before match
        {
            ASSERT(officialMatch);

            if (officialMatch != NULL)
            {
                officialMatch->canceled = true;
                officialMatch->cancelationReason = "Official match cancellation requested by " + std::string(playerData->callsign.c_str());
            }
            else
            {
                bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Fun match ended by %s", playerData->callsign.c_str());
            }

            bz_debugMessagef(DEBUG_LEVEL, "DEBUG :: League Over Seer :: Match ended by %s (%s).", playerData->callsign.c_str(), playerData->ipAddress.c_str());
            bz_gameOver(253, eObservers);
        }
        else
            bz_sendTextMessage(BZ_SERVER, playerID, "There is no match in progress to cancel.");

        return true;
    }
    else if (command == "finish")
    {
        if (playerData->team == eObservers) //Observers can't cancel matches
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "Observers are not allowed to cancel matches.");
        }
        else if (bz_isCountDownInProgress())
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "You may only cancel a match after it has started.");
        }
        else if (bz_isCountDownActive())
        {
            ASSERT(officialMatch);

            if (officialMatch != NULL)
            {
                if (officialMatch->startTime >= 0.0f && officialMatch->startTime + (officialMatch->duration/2) < bz_getCurrentTime())
                {
                    bz_debugMessagef(DEBUG_LEVEL, "DEBUG :: Match Over Seer :: Official match ended early by %s (%s)", playerData->callsign.c_str(), playerData->ipAddress.c_str());
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
            bz_sendTextMessage(BZ_SERVER, playerID, "There is no match in progress to end.");

        return true;
    }
    else if (command == "pause")
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

        return true;
    }
    else if (command == "resume")
    {
        if (!bz_isCountDownPaused())
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "The match is not paused!");
        }
        else if (bz_isCountDownActive())
        {
            bz_resumeCountdown(playerData->callsign.c_str());
        }
        else
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "There is no active match to resume right now.");
        }

        return true;
    }
    else if (command == "spawn")
    {
        if (bz_hasPerm(playerID, "ban"))
        {
            if (params->size() > 0)
            {
                std::string callsignToLookup; //store the callsign we're going to search for

                for (unsigned int i = 0; i < params->size(); i++) //piece together the callsign from the slash command parameters
                {
                    callsignToLookup += params->get(i).c_str();
                    if (i != params->size() - 1) // so we don't stick a whitespace on the end
                        callsignToLookup += " "; // add a whitespace between each chat text parameter
                }

                if (std::string::npos != std::string(params->get(0).c_str()).find("#") && isValidPlayerID(atoi(std::string(params->get(0).c_str()).erase(0, 1).c_str())))
                {
                    bz_grantPerm(atoi(std::string(params->get(0).c_str()).erase(0, 1).c_str()), "spawn");
                    bz_sendTextMessagef(BZ_SERVER, eAdministrators, "%s gave spawn perms to %s", bz_getPlayerByIndex(playerID)->callsign.c_str(), bz_getPlayerByIndex(atoi(std::string(params->get(0).c_str()).substr(0, 1).c_str()))->callsign.c_str());
                }
                else if (getPlayerByCallsign(callsignToLookup) >= 0)
                {
                    bz_grantPerm(getPlayerByCallsign(callsignToLookup), "spawn");
                    bz_sendTextMessagef(BZ_SERVER, eAdministrators, "%s gave spawn perms to %s", bz_getPlayerByIndex(playerID)->callsign.c_str(),
                    bz_getPlayerByIndex(getPlayerByCallsign(callsignToLookup))->callsign.c_str());
                }
                else
                    bz_sendTextMessagef(BZ_SERVER, playerID, "player %s not found", params->get(0).c_str());
            }
            else
                bz_sendTextMessage(BZ_SERVER, playerID, "/spawn <player id or callsign>");
        }
        else if (!playerData->admin)
            bz_sendTextMessage(BZ_SERVER,playerID,"You do not have permission to use the /spawn command.");

        return true;
    }

    return false;
}

void leagueOverSeer::URLDone(const char* /*URL*/, const void* data, unsigned int /*size*/, bool /*complete*/) //Everything went fine with the report
{
    std::string siteData = (const char*)(data); //Convert the data to a std::string
    bz_debugMessagef(1, "URL Job Successful! Data returned: %s", siteData.c_str());

    if (strcmp(siteData.substr(0, 6).c_str(), "INSERT") == 0 || strcmp(siteData.substr(0, 6).c_str(), "DELETE") == 0)
        doQuery(siteData);
    else if (siteData.find("<html>") == std::string::npos)
    {
        bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%s", siteData.c_str());
        bz_debugMessagef(DEBUG_LEVEL, "%s", siteData.c_str());
    }
}

void leagueOverSeer::URLTimeout(const char* /*URL*/, int /*errorCode*/) //The league website is down or is not responding, the request timed out
{
    bz_debugMessage(DEBUG_LEVEL, "DEBUG :: League Over Seer :: The request to the league site has timed out.");
}

void leagueOverSeer::URLError(const char* /*URL*/, int errorCode, const char *errorString) //The server owner must have set up the URLs wrong because this shouldn't happen
{
    bz_debugMessage(DEBUG_LEVEL, "DEBUG :: League Over Seer :: Match report failed with the following error:");
    bz_debugMessagef(DEBUG_LEVEL, "DEBUG :: League Over Seer :: Error code: %i - %s", errorCode, errorString);
}

/* ==== const char* functions ==== */

const char* leagueOverSeer::getMotto(const char* bzid)
{
    const char* motto;

    // Prepare the SQL query with the BZID of the player
    sqlite3_bind_text(getPlayerMotto, 1, bzid, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(getPlayerMotto) == SQLITE_ROW) // If returns a team name, use it
        motto = (const char*)sqlite3_column_text(getPlayerMotto, 0);
    else
        motto = "";

    sqlite3_reset(getPlayerMotto); //Clear the prepared statement so it can be reused

    return motto;
}

/* ==== std::string functions ==== */

std::string leagueOverSeer::buildBZIDString(bz_eTeamType team)
{
    std::string teamString;
    bz_debugMessagef(DEBUG_LEVEL, "Match Data :: %s Team Players", formatTeam(team, false).c_str());

    for (unsigned int i = 0; i < match->matchPlayers.size(); i++) //Add all the red players to the match report
    {
        if (match->matchPlayers.at(i).team == team)
        {
            teamString += std::string(bz_urlEncode(match->matchPlayers.at(i).bzid.c_str())) + ",";
            bz_debugMessagef(DEBUG_LEVEL, "Match Data ::  %s (%s)", match->matchPlayers.at(i).callsign.c_str(), match->matchPlayers.at(i).bzid.c_str());
        }
    }

    return teamString.erase(teamString.size() - 1);
}

/* ==== sqlite3_stmt* functions ==== */

sqlite3_stmt* leagueOverSeer::prepareQuery(std::string sql)
{
    /*
        Thanks to blast for this function
    */

    // Search our std::map for this statement
    PreparedStatementMap::iterator itr = preparedStatements.find(sql);

    // If it doesn't exist, create it
    if (itr == preparedStatements.end())
    {
        sqlite3_stmt* newStatement;

        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &newStatement, 0) != SQLITE_OK)
        {
            bz_debugMessagef(2, "DEBUG :: League Over Seer :: SQLite :: Failed to generate prepared statement for '%s' :: Error #%i: %s", sql.c_str(), sqlite3_errcode(db), sqlite3_errmsg(db));
            return NULL;
        }
        else
        {
            preparedStatements[sql] = newStatement;
        }
    }

    return preparedStatements[sql];
}

/* ==== void functions ==== */

void leagueOverSeer::doQuery(std::string query)
{
    /*
        Execute a SQL query without the need of any return values
    */

    bz_debugMessage(2, "DEBUG :: League Over Seer :: Executing following SQL query...");
    bz_debugMessagef(2, "DEBUG :: League Over Seer :: %s", query.c_str());

    char* db_err = 0; //a place to store the error
    sqlite3_exec(db, query.c_str(), NULL, 0, &db_err); //execute

    if (db_err != 0) //print out any errors
    {
        bz_debugMessage(2, "DEBUG :: League Over Seer :: SQL ERROR!");
        bz_debugMessagef(2, "DEBUG :: League Over Seer :: %s", db_err);
    }
}

void leagueOverSeer::loadConfig(const char* cmdLine) //Load the plugin configuration file
{
    PluginConfig config = PluginConfig(cmdLine);
    std::string section = "leagueOverSeer";

    if (config.errors) bz_shutdown(); //Shutdown the server

    //Extract all the data in the configuration file and assign it to plugin variables
    ROTATION_LEAGUE = toBool(config.item(section, "ROTATIONAL_LEAGUE"));
    mapchangePath = config.item(section, "MAPCHANGE_PATH");
    SQLiteDB = config.item(section, "SQLITE_DB");
    LEAGUE_URL = config.item(section, "LEAGUE_OVER_SEER_URL");
    DEBUG_LEVEL = atoi((config.item(section, "DEBUG_LEVEL")).c_str());

    //Check for errors in the configuration data. If there is an error, shut down the server
    if (strcmp(LEAGUE_URL.c_str(), "") == 0)
    {
        bz_debugMessage(0, "*** DEBUG :: League Over Seer :: No URLs were choosen to report matches or query teams. ***");
        bz_shutdown();
    }
    if (DEBUG_LEVEL > 4 || DEBUG_LEVEL < 0)
    {
        bz_debugMessage(0, "*** DEBUG :: League Over Seer :: Invalid debug level in the configuration file. ***");
        bz_shutdown();
    }
}

void leagueOverSeer::updateTeamNames(void)
{
    int totaltanks = bz_getTeamCount(eRogueTeam) + bz_getTeamCount(eRedTeam) + bz_getTeamCount(eGreenTeam) + bz_getTeamCount(eBlueTeam) + bz_getTeamCount(ePurpleTeam) + bz_getTeamCount(eObservers);

    if (totaltanks > 0) //FIXME: Wut?
        return;

    // Build the POST data for the URL job
    std::string teamNameDump = "query=teamDump";
    bz_debugMessagef(DEBUG_LEVEL, "DEBUG :: League Over Seer :: Updating Team name database...");

    bz_addURLJob(LEAGUE_URL.c_str(), this, teamNameDump.c_str()); //Send the team update request to the league website
}
