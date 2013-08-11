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
#include <ctype.h>
#include <fstream>
#include <iostream>
#include <math.h>
#include <sqlite3.h>
#include <sstream>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "bzfsAPI.h"
#include "plugin_utils.h"

//Define plugin version numbering
const int MAJOR = 1;
const int MINOR = 0;
const int REV = 0;
const int BUILD = 156;

class leagueOverSeer : public bz_Plugin, public bz_CustomSlashCommandHandler, public bz_BaseURLHandler
{
    sqlite3* db; //sqlite database we'll be using

    virtual const char* Name (){return "League Over Seer 1.0";}
    virtual void Init(const char* config);
    virtual void Event(bz_EventData *eventData);
    virtual bool SlashCommand(int playerID, bz_ApiString, bz_ApiString, bz_APIStringList*);
    virtual void Cleanup();
    virtual void URLDone(const char* URL, const void* data, unsigned int size, bool complete);
    virtual void URLTimeout(const char* URL, int errorCode);
    virtual void URLError(const char* URL, int errorCode, const char *errorString);

    //Custom functions
    virtual bool isDigit(std::string myString);
    virtual bool isValidPlayerID(int playerID);
    virtual bool toBool(std::string var);

    virtual const char* getMotto(const char* bzid);

    virtual int isValidCallsign(std::string callsign);

    virtual std::string buildBZIDString(bz_eTeamType team);
    virtual std::string formatTeam(bz_eTeamType teamColor, bool addWhiteSpace);
    virtual std::string getString(int number);

    virtual sqlite3_stmt* prepareQuery(std::string sql);

    virtual void doQuery(std::string query);
    virtual void loadConfig(const char *cmdLine);
    virtual void updateTeamNames(void);

    //All the variables that will be used in the plugin
    bool         doNotReportMatch,
                 funMatch,
                 matchParticipantsRecorded,
                 officialMatch,
                 rotLeague;

    int          DEBUG_LEVEL,
                 matchDuration,
                 teamOnePoints,
                 teamTwoPoints;

    double       matchRollCall,
                 matchStartTime;

    std::string  LEAGUE_URL,
                 map,
                 mapchangePath,
                 SQLiteDB;

    bz_eTeamType teamOne,
                 teamTwo;

    struct playersInMatch //Maintains the players that started the match
    {
        std::string bzid;
        std::string callsign;
        bz_eTeamType team;
    };
    std::vector<playersInMatch> matchPlayers;

    typedef std::map<std::string, sqlite3_stmt*> PreparedStatementMap; // Define the type as a shortcut
    PreparedStatementMap preparedStatements; // Create the object to store prepared statements

    sqlite3_stmt *getPlayerMotto;
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

    //Set all boolean values for the plugin to false
    officialMatch             = false;
    doNotReportMatch          = false;
    matchParticipantsRecorded = false;
    funMatch                  = false;
    teamOnePoints             = 0;
    teamTwoPoints             = 0;
    matchDuration             = bz_getTimeLimit();
    matchStartTime            = 0;
    matchRollCall             = 90;

    loadConfig(commandLine); //Load the configuration data when the plugin is loaded

    if (mapchangePath != "" && rotLeague) //Check to see if the plugin is for a rotational league
    {
        //Open the mapchange.out file to see what map is being used
        std::ifstream infile;
        infile.open(mapchangePath.c_str());
        getline(infile,map);
        infile.close();

        map = map.substr(0, map.length() - 5); //Remove the '.conf' from the mapchange.out file

        bz_debugMessagef(DEBUG_LEVEL, "DEBUG :: League Over Seer :: Current map being played: %s", map.c_str());
    }

    teamOne = eNoTeam;
    teamTwo = eNoTeam;

    while (teamOne == eNoTeam || teamTwo == eNoTeam)
    {
        if (bz_getTeamPlayerLimit(eRedTeam) > 0 && teamOne == eNoTeam)
        {
            teamOne = eRedTeam;
            continue;
        }
        if (bz_getTeamPlayerLimit(eGreenTeam) > 0 && teamOne == eNoTeam)
        {
            teamOne = eGreenTeam;
            continue;
        }
        if (bz_getTeamPlayerLimit(eBlueTeam) > 0 && teamOne == eNoTeam)
        {
            teamOne = eBlueTeam;
            continue;
        }
        if (bz_getTeamPlayerLimit(ePurpleTeam) > 0 && teamOne == eNoTeam)
        {
            teamOne = ePurpleTeam;
            continue;
        }

        // Figure out the other team
        if (bz_getTeamPlayerLimit(eRedTeam) > 0 && teamOne != eRedTeam && teamTwo == eNoTeam)
        {
            teamTwo = eRedTeam;
            continue;
        }
        if (bz_getTeamPlayerLimit(eGreenTeam) > 0 && teamOne != eGreenTeam && teamTwo == eNoTeam)
        {
            teamTwo = eGreenTeam;
            continue;
        }
        if (bz_getTeamPlayerLimit(eBlueTeam) > 0 && teamOne != eBlueTeam && teamTwo == eNoTeam)
        {
            teamTwo = eBlueTeam;
            continue;
        }
        if (bz_getTeamPlayerLimit(ePurpleTeam) > 0 && teamOne != ePurpleTeam && teamTwo == eNoTeam)
        {
            teamTwo = ePurpleTeam;
            continue;
        }
    }

    bz_debugMessagef(0, "DEBUG :: League Over Seer :: Using the following database: %s", SQLiteDB.c_str());
    sqlite3_open(SQLiteDB.c_str(), &db);

    if (db == 0) //we couldn't read the database provided
        bz_debugMessagef(0, "DEBUG :: League Over Seer :: Error! Could not connect to: %s", SQLiteDB.c_str());

    if (db != 0) //if the database connection succeed and the database is empty, let's create the tables needed
        doQuery("CREATE TABLE IF NOT EXISTS [Players] (BZID INTEGER NOT NULL PRIMARY KEY DEFAULT 0, TEAM TEXT NOT NULL DEFAULT Teamless, SQUAD TEXT);");

    // Prepare the SQL query to get the team names based on a BZID
    getPlayerMotto = prepareQuery("SELECT team FROM players WHERE bzid = ?");

    updateTeamNames();
}

void leagueOverSeer::Cleanup (void)
{
    bz_debugMessagef(0, "League Over Seer %i.%i.%i (%i) unloaded.", MAJOR, MINOR, REV, BUILD);

    Flush(); // Clean up all the events

    // Remove the prepared SQLite statement from memory
    sqlite3_finalize(getPlayerMotto);

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
            bz_CTFCaptureEventData_V1 *capData = (bz_CTFCaptureEventData_V1*)eventData;

            if (officialMatch)
                (capData->teamCapped == teamOne) ? teamOnePoints++ : teamTwoPoints++;
        }
        break;

        case bz_eGameEndEvent: //A /gameover or a match has ended
        {
            //Clear the bool variables
            funMatch                  = false;
            matchStartTime            = 0;
            matchParticipantsRecorded = false;

            if (doNotReportMatch && officialMatch) //The match was canceled via /gameover or /superkill and we do not want to report these matches
            {
                officialMatch    = false; //Match is over
                doNotReportMatch = false; //Reset the variable for next usage
                teamOnePoints    = 0;
                teamTwoPoints    = 0;
                matchPlayers.clear();
                bz_debugMessage(DEBUG_LEVEL, "DEBUG :: League Over Seer :: Official match was not reported.");
                bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Official match was not reported.");
            }
            else if (officialMatch)
            {
                officialMatch = false; //Match is over
                bz_Time standardTime;
                bz_getUTCtime(&standardTime);

                //Format the date to -> year-month-day hour:minute:second
                char match_date[20];
                sprintf(match_date, "%02d-%02d-%02d %02d:%02d:%02d", standardTime.year, standardTime.month, standardTime.day, standardTime.hour, standardTime.minute, standardTime.second);

                //Keep references to values for quick reference
                std::string teamOnePointsFinal = getString(teamOnePoints);
                std::string teamTwoPointsFinal = getString(teamTwoPoints);
                std::string matchTimeFinal     = getString(matchDuration/60);

                // Store match data in the logs
                bz_debugMessagef(DEBUG_LEVEL, "Match Data :: League Over Seer Match Report");
                bz_debugMessagef(DEBUG_LEVEL, "Match Data :: -----------------------------");
                bz_debugMessagef(DEBUG_LEVEL, "Match Data :: Match Time      : %s", match_date);
                bz_debugMessagef(DEBUG_LEVEL, "Match Data :: Duration        : %s", matchTimeFinal.c_str());
                bz_debugMessagef(DEBUG_LEVEL, "Match Data :: Team One Score  : %s", teamOnePointsFinal.c_str());
                bz_debugMessagef(DEBUG_LEVEL, "Match Data :: Team Two Score  : %s", teamTwoPointsFinal.c_str());

                // Start building POST data to be sent to the league website
                std::string matchToSend = "query=reportMatch";
                            matchToSend += "&teamOneWins=" + std::string(bz_urlEncode(teamOnePointsFinal.c_str()));
                            matchToSend += "&teamTwoWins=" + std::string(bz_urlEncode(teamTwoPointsFinal.c_str()));
                            matchToSend += "&duration="    + std::string(bz_urlEncode(matchTimeFinal.c_str()));
                            matchToSend += "&matchTime="   + std::string(bz_urlEncode(match_date));

                if (rotLeague) //Only add this parameter if it's a rotational league such as Open League
                    matchToSend += "&mapPlayed=" + std::string(bz_urlEncode(map.c_str()));

                matchToSend += "&teamOnePlayers=" + buildBZIDString(teamOne);
                matchToSend += "&teamTwoPlayers=" + buildBZIDString(teamTwo);

                bz_debugMessagef(DEBUG_LEVEL, "Match Data :: -----------------------------");
                bz_debugMessagef(DEBUG_LEVEL, "Match Data :: End of Match Report");
                bz_debugMessagef(DEBUG_LEVEL, "DEBUG :: League Over Seer :: Reporting match data...");
                bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Reporting match...");

                //Send the match data to the league website
                bz_addURLJob(LEAGUE_URL.c_str(), this, matchToSend.c_str());

                //Clear all the structures and scores for next match
                matchPlayers.clear();
                teamOnePoints = 0;
                teamTwoPoints = 0;
            }
            else
                bz_debugMessage(DEBUG_LEVEL, "DEBUG :: League Over Seer :: Fun match was not reported.");
        }
        break;

        case bz_eGameStartEvent: //The countdown has started
        {
            if (officialMatch) //Don't waste memory if the match isn't official
            {
                //Set the team scores to zero just in case
                teamOnePoints    = 0;
                teamTwoPoints    = 0;
                matchDuration    = bz_getTimeLimit();
                matchStartTime   = bz_getCurrentTime();
                doNotReportMatch = false;
                matchPlayers.clear();
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
                if (officialMatch)
                    bz_sendTextMessage(BZ_SERVER, joinData->playerID, "*** There is currently an official match in progress, please be respectful. ***");
                if (funMatch)
                    bz_sendTextMessage(BZ_SERVER, joinData->playerID, "*** There is currently a fun match in progress, please be respectful. ***");
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
            bz_BasePlayerRecord *playerData = bz_getPlayerByIndex(commandData->from);
            std::string command = commandData->message.c_str(); //Use std::string for quick reference

            if (strncmp("/gameover", command, 9) == 0)
                bz_sendTextMessagef(BZ_SERVER, commandData->from, "** '/gameover' is disabled, please use /finish or /cancel instead **");
            else if (strncmp("/countdown pause", command, 16) == 0)
                bz_sendTextMessagef(BZ_SERVER, commandData->from, "** '/countdown pause' is disabled, please use /pause instead **");
            else if (strncmp("/countdown resume", command, 17 ) == 0)
                bz_sendTextMessagef(BZ_SERVER, commandData->from, "** '/countdown resume' is disabled, please use /resume instead **");
            else if (isdigit(atoi(command) + 12))
                bz_sendTextMessage(BZ_SERVER, commandData->from, "** '/countdown TIME' is disabled, please use /official or /fm instead **");

            bz_freePlayerRecord(playerData);
        }
        break;

        case bz_eTickEvent: //Tick tock tick tock...
        {
            int totaltanks = bz_getTeamCount(eRogueTeam) + bz_getTeamCount(eRedTeam) + bz_getTeamCount(eGreenTeam) + bz_getTeamCount(eBlueTeam) + bz_getTeamCount(ePurpleTeam);

            if (totaltanks == 0)
            {
                //Incase a boolean gets messed up in the plugin, reset all the plugin variables when there are no players (Observers excluded)
                officialMatch = false;
                doNotReportMatch = false;
                funMatch = false;
                teamOnePoints = 0;
                teamTwoPoints = 0;
                matchParticipantsRecorded = false;
                matchStartTime = 0;

                //This should never happen but just incase the countdown is going when there are no tanks
                if (bz_isCountDownActive())
                    bz_gameOver(253, eObservers);
            }

            if (officialMatch && matchStartTime > 0 && matchStartTime + matchRollCall < bz_getCurrentTime() && !matchParticipantsRecorded)
            {
                bool invalidateRollCall = false;
                bz_APIIntList *playerList = bz_newIntList();
                bz_getPlayerIndexList(playerList);

                for (unsigned int i = 0; i < playerList->size(); i++)
                {
                    bz_BasePlayerRecord *playerRecord = bz_getPlayerByIndex(playerList->get(i));

                    if (bz_getPlayerTeam(playerList->get(i)) != eObservers) //If player is not an observer
                    {
                        playersInMatch currentPlayer;
                        currentPlayer.team = playerRecord->team; //Add team to structure
                        currentPlayer.callsign = playerRecord->callsign.c_str(); //Add team to structure
                        currentPlayer.bzid = playerRecord->bzID.c_str(); //Add bzid to structure

                        if (std::string(playerRecord->bzID.c_str()).empty())
                            invalidateRollCall = true;

                        matchPlayers.push_back(currentPlayer);
                    }

                    bz_freePlayerRecord(playerRecord);

                    if (invalidateRollCall)
                        break;
                }

                bz_deleteIntList(playerList);

                if (invalidateRollCall && matchRollCall < matchDuration)
                {
                    bz_debugMessagef(DEBUG_LEVEL, "DEBUG :: League Over Seer :: Invalid player found on field at %i:%i.", (int)(matchRollCall/60), (int)(fmod(matchRollCall,60.0)));

                    matchParticipantsRecorded = false;
                    matchRollCall += 30;
                    matchPlayers.clear();
                }
                else
                    matchParticipantsRecorded = true;
            }
        }
        break;

        default:
        break;
    }
}

bool leagueOverSeer::SlashCommand(int playerID, bz_ApiString command, bz_ApiString /*message*/, bz_APIStringList *params)
{
    int timeToStart = atoi(params->get(0).c_str());
    bz_BasePlayerRecord *playerData = bz_getPlayerByIndex(playerID);

    if (command == "official") //Someone used the /official command
    {
        if (playerData->team == eObservers) //Observers can't start matches
            bz_sendTextMessage(BZ_SERVER, playerID, "Observers are not allowed to start matches.");
        else if ((bz_getTeamCount(eRedTeam) < 2 && bz_getTeamPlayerLimit(eRedTeam) > 0) ||
                (bz_getTeamCount(eGreenTeam) < 2 && bz_getTeamPlayerLimit(eGreenTeam) > 0) ||
                (bz_getTeamCount(eBlueTeam) < 2 && bz_getTeamPlayerLimit(eBlueTeam) > 0) ||
                (bz_getTeamCount(ePurpleTeam) < 2 && bz_getTeamPlayerLimit(ePurpleTeam) > 0)) //An official match cannot be 1v1 or 2v1
            bz_sendTextMessage(BZ_SERVER, playerID, "You may not have an official match with less than 2 players per team.");
        else if (funMatch) //A fun match cannot be declared an official match
            bz_sendTextMessage(BZ_SERVER,playerID,"Fun matches cannot be turned into official matches.");
        else if (!playerData->verified || !bz_hasPerm(playerID,"spawn")) //If they can't spawn, they aren't a league player so they can't start a match
            bz_sendTextMessage(BZ_SERVER,playerID,"Only registered league players may start an official match.");
        else if (bz_isCountDownActive() || bz_isCountDownInProgress()) //A countdown is in progress already
            bz_sendTextMessage(BZ_SERVER,playerID,"There is currently a countdown active, you may not start another.");
        else if (playerData->verified && playerData->team != eObservers && bz_hasPerm(playerID,"spawn") && !bz_isCountDownActive() && !funMatch) //Check the user is not an obs and is a league member
        {
            officialMatch = true; //Notify the plugin that the match is official
            bz_debugMessagef(DEBUG_LEVEL, "DEBUG :: League Over Seer :: Official match started by %s (%s).", playerData->callsign.c_str(), playerData->ipAddress.c_str());
            bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Official match started by %s.", playerData->callsign.c_str());

            if (timeToStart <= 120 && timeToStart > 5)
                bz_startCountdown (timeToStart, bz_getTimeLimit(), "Server"); //Start the countdown with a custom countdown time limit under 2 minutes
            else
                bz_startCountdown (10, bz_getTimeLimit(), "Server"); //Start the countdown for the official match
        }
        else
            bz_sendTextMessage(BZ_SERVER, playerID, "You do not have permission to run the /official command.");

        return true;
    }
    else if (command == "fm") //Someone uses the /fm command
    {
        if (bz_isCountDownActive() || bz_isCountDownInProgress() || funMatch || officialMatch) //There is already a countdown
            bz_sendTextMessage(BZ_SERVER, playerID, "There is currently a countdown active, you may not start another.");
        else if (playerData->team == eObservers) //Observers can't start matches
            bz_sendTextMessage(BZ_SERVER,playerID,"Observers are not allowed to start matches.");
        else if (!playerData->verified || !bz_hasPerm(playerID,"spawn")) //If they can't spawn, they aren't a league player so they can't start a match
            bz_sendTextMessage(BZ_SERVER,playerID,"Only registered league players may start an official match.");
        else if (!bz_isCountDownActive() && playerData->team != eObservers && bz_hasPerm(playerID,"spawn") && playerData->verified && !officialMatch)
        {
            funMatch = true; //It's a fun match

            bz_debugMessagef(DEBUG_LEVEL, "DEBUG :: League Over Seer :: Fun match started by %s (%s).", playerData->callsign.c_str(), playerData->ipAddress.c_str());
            bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Fun match started by %s.", playerData->callsign.c_str());

            if (timeToStart <= 120 && timeToStart > 5)
                bz_startCountdown (timeToStart, bz_getTimeLimit(), "Server"); //Start the countdown with a custom countdown time limit under 2 minutes
            else
                bz_startCountdown (10, bz_getTimeLimit(), "Server"); //Start the countdown for the official match
        }
        else
            bz_sendTextMessage(BZ_SERVER,playerID,"You do not have permission to run the /fm command.");

        return true;
    }
    else if (command == "cancel")
    {
        if (bz_hasPerm(playerID,"spawn") && bz_isCountDownActive())
        {
            if (officialMatch)
            {
                bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Official match ended by %s", playerData->callsign.c_str());
                bz_debugMessagef(DEBUG_LEVEL, "DEBUG :: League Over Seer :: Match ended by %s (%s).", playerData->callsign.c_str(),playerData->ipAddress.c_str());
            }
            else
            {
                bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Fun match ended by %s", playerData->callsign.c_str());
                bz_debugMessagef(DEBUG_LEVEL, "DEBUG :: League Over Seer :: Match ended by %s (%s).", playerData->callsign.c_str(),playerData->ipAddress.c_str());
            }

            //Reset the server. Cleanly ends a match
            officialMatch = false;
            doNotReportMatch = true;
            funMatch = false;
            teamOnePoints = 0;
            teamTwoPoints = 0;

            //End the countdown
            if (bz_isCountDownActive())
                bz_gameOver(253, eObservers);
        }
        else if (!bz_isCountDownActive())
            bz_sendTextMessage(BZ_SERVER, playerID, "There is no match in progress to cancel.");
        else //Not a league player
            bz_sendTextMessage(BZ_SERVER, playerID, "You do not have permission to run the /cancel command.");

        return true;
    }
    else if (command == "finish")
    {
        if (bz_hasPerm(playerID,"spawn") && bz_isCountDownActive() && officialMatch)
        {
            bz_debugMessagef(DEBUG_LEVEL, "DEBUG :: Match Over Seer :: Official match ended early by %s (%s)", playerData->callsign.c_str(), playerData->ipAddress.c_str());
            bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Official match ended early by %s", playerData->callsign.c_str());

            doNotReportMatch = false; //To prevent reporting a canceled match, let plugin know the match was canceled

            //End the countdown
            if (bz_isCountDownActive())
                bz_gameOver(253, eObservers);
        }
        else if (!bz_isCountDownActive())
            bz_sendTextMessage(BZ_SERVER, playerID, "There is no match in progress to end.");
        else if (!officialMatch)
            bz_sendTextMessage(BZ_SERVER, playerID, "You cannot /finish a fun match. Use /cancel instead.");
        else //Not a league player
            bz_sendTextMessage(BZ_SERVER, playerID, "You do not have permission to run the /finish command.");

        return true;
    }
    else if (command == "pause")
    {
        if (bz_isCountDownActive() && bz_hasPerm(playerID,"spawn") && playerData->verified)
            bz_pauseCountdown(playerData->callsign.c_str());
        else if (!bz_isCountDownActive())
            bz_sendTextMessage(BZ_SERVER, playerID, "There is no active match to pause right now.");
        else
            bz_sendTextMessage(BZ_SERVER, playerID, "You are not have permission to run the /pause command.");

        return true;
    }
    else if (command == "resume")
    {
        if (bz_hasPerm(playerID,"spawn") && playerData->verified && bz_isCountDownActive())
            bz_resumeCountdown(playerData->callsign.c_str());
        else if (!bz_isCountDownActive())
            bz_sendTextMessage(BZ_SERVER, playerID, "The current match is not paused.");
        else
            bz_sendTextMessage(BZ_SERVER, playerID, "You are not have permission to run the /resume command.");

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
                else if (isValidCallsign(callsignToLookup) >= 0)
                {
                    bz_grantPerm(isValidCallsign(callsignToLookup), "spawn");
                    bz_sendTextMessagef(BZ_SERVER, eAdministrators, "%s gave spawn perms to %s", bz_getPlayerByIndex(playerID)->callsign.c_str(), bz_getPlayerByIndex(isValidCallsign(callsignToLookup))->callsign.c_str());
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

    bz_freePlayerRecord(playerData);
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

std::string leagueOverSeer::buildBZIDString(bz_eTeamType team)
{
    std::string teamString;
    bz_debugMessagef(DEBUG_LEVEL, "Match Data :: %s Team Players", formatTeam(team, false).c_str());

    for (unsigned int i = 0; i < matchPlayers.size(); i++) //Add all the red players to the match report
    {
        if (matchPlayers.at(i).team == team)
        {
            teamString += std::string(bz_urlEncode(matchPlayers.at(i).bzid.c_str())) + ",";
            bz_debugMessagef(DEBUG_LEVEL, "Match Data ::  %s (%s)", matchPlayers.at(i).callsign.c_str(), matchPlayers.at(i).bzid.c_str());
        }
    }

    return teamString.erase(teamString.size() - 1);
}

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

std::string leagueOverSeer::formatTeam(bz_eTeamType teamColor, bool addWhiteSpace)
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
    }

    if (addWhiteSpace)
        while (color.length() < 7)
          color += " ";

    return color;
}

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

std::string leagueOverSeer::getString(int number) //Convert an integer into a string
{
    std::stringstream string;
    string << number;

    return string.str();
}

bool leagueOverSeer::isDigit(std::string myString)
{
    for (unsigned int i = 0; i < myString.size(); i++) //Go through entire string
    {
        if (!isdigit(myString[i])) //If one character is not a digit, then the string is not a digit
            return false;
    }
    return true; //All characters are digits
}

int leagueOverSeer::isValidCallsign(std::string callsign)
{
    bz_APIIntList *playerList = bz_newIntList();
    bz_getPlayerIndexList(playerList);

    for (unsigned int i = 0; i < playerList->size(); i++) //Go through all the players
    {
        if (strcmp(bz_getPlayerByIndex(playerList->get(i))->callsign.c_str(), callsign.c_str()) == 0)
        {
            int playerID = playerList->get(i);
            bz_deleteIntList(playerList);
            return playerID;
        }
    }

    bz_deleteIntList(playerList);
    return -1;
}

bool leagueOverSeer::isValidPlayerID(int playerID)
{
    bz_APIIntList *playerList = bz_newIntList();
    bz_getPlayerIndexList(playerList);

    for (unsigned int i = 0; i < playerList->size(); i++) //Go through all the players
    {
        if (playerList->get(i) == playerID)
        {
            bz_deleteIntList(playerList);
            return true;
        }
    }

    bz_deleteIntList(playerList);
    return false;
}

void leagueOverSeer::loadConfig(const char* cmdLine) //Load the plugin configuration file
{
    PluginConfig config = PluginConfig(cmdLine);
    std::string section = "leagueOverSeer";

    if (config.errors) bz_shutdown(); //Shutdown the server

    //Extract all the data in the configuration file and assign it to plugin variables
    rotLeague = toBool(config.item(section, "ROTATIONAL_LEAGUE"));
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
    if (DEBUG > 4 || DEBUG < 0)
    {
        bz_debugMessage(0, "*** DEBUG :: League Over Seer :: Invalid debug level in the configuration file. ***");
        bz_shutdown();
    }
}

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

bool leagueOverSeer::toBool(std::string var) //Turn std::string into a boolean value
{
    if (var == "true" || var == "TRUE" || var == "1") //If the value is true then the boolean is true
        return true;
    else //If it's not true, then it's false.
        return false;
}

void leagueOverSeer::updateTeamNames(void)
{
    int totaltanks = bz_getTeamCount(eRogueTeam) + bz_getTeamCount(eRedTeam) + bz_getTeamCount(eGreenTeam) + bz_getTeamCount(eBlueTeam) + bz_getTeamCount(ePurpleTeam) + bz_getTeamCount(eObservers);

    if (totaltanks > 0)
        return;

    // Build the POST data for the URL job
    std::string teamNameDump = "query=teamDump";
    bz_debugMessagef(DEBUG_LEVEL, "DEBUG :: League Over Seer :: Updating Team name database...");

    bz_addURLJob(LEAGUE_URL.c_str(), this, teamNameDump.c_str()); //Send the team update request to the league website
}
