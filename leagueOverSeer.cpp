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

#include <stdio.h>
#include <iostream>
#include <sqlite3.h>
#include <sstream>
#include <string>
#include <stdexcept>
#include <fstream>
#include <algorithm>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bzfsAPI.h"
#include "plugin_utils.h"

//Define plugin version numbering
const int MAJOR = 0;
const int MINOR = 9;
const int REV = 3;
const int BUILD = 80;

class leagueOverSeer : public bz_Plugin, public bz_CustomSlashCommandHandler, public bz_BaseURLHandler
{
    sqlite3* db; //sqlite database we'll be using

    virtual const char* Name (){return "League Over Seer 0.9.3 (80)";}
    virtual void Init ( const char* config);
    virtual void Event( bz_EventData *eventData );
    virtual bool SlashCommand( int playerID, bz_ApiString, bz_ApiString, bz_APIStringList*);
    virtual void Cleanup ();
    virtual void URLDone( const char* URL, void* data, unsigned int size, bool complete );
    virtual void URLTimeout(const char* URL, int errorCode);
    virtual void URLError(const char* URL, int errorCode, const char *errorString);
    virtual void doQuery(std::string query);
    virtual bool isDigit(std::string myString);
    virtual int isValidCallsign(std::string callsign);
    virtual bool isValidPlayerID(int playerID);
    virtual int loadConfig(const char *cmdLine);
    virtual bool toBool(std::string var);

    //All the variables that will be used in the plugin
    bool officialMatch, matchCanceled, funMatch, rotLeague, gameoverReport;
    int DEBUG, teamOnePoints, teamTwoPoints;
    std::string LEAGUE_URL, map, SQLiteDB, teamOneName, teamTwoName;
    const char* mapchangePath;
    bz_eTeamType teamOne, teamTwo;

    struct teamQueries { //Stores all the queries that a player request
        int _playerID;
    };
    std::vector<teamQueries> _playerIDs;

    struct urlQueries { //Stores the order of match reports and team queries
        std::string _URL;
    };
    std::vector<urlQueries> _urlQuery;

    struct playersInMatch //Maintains the players that started the match on the red team
    {
        std::string bzid;
        bz_eTeamType team;
    };
    std::vector<playersInMatch> matchPlayers;
};

BZ_PLUGIN(leagueOverSeer);

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
    bz_registerCustomSlashCommand("fm",this);
    bz_registerCustomSlashCommand("cancel",this);
    bz_registerCustomSlashCommand("spawn",this);
    bz_registerCustomSlashCommand("resume",this);
    bz_registerCustomSlashCommand("pause",this);

    //Set all boolean values for the plugin to false
    officialMatch = false;
    matchCanceled = false;
    funMatch = false;
    teamOnePoints = 0;
    teamTwoPoints = 0;

    loadConfig(commandLine); //Load the configuration data when the plugin is loaded

    if (mapchangePath != "" && rotLeague) //Check to see if the plugin is for a rotational league
    {
        //Open the mapchange.out file to see what map is being used
        std::ifstream infile;
        infile.open (mapchangePath);
        getline(infile,map);
        infile.close();

        map = map.substr(0, map.length() - 5); //Remove the '.conf' from the mapchange.out file

        bz_debugMessagef(DEBUG, "DEBUG :: League Over Seer :: Current map being played: %s", map.c_str());
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

        //Figure out the other team
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
    sqlite3_open(SQLiteDB.c_str(),&db);

    if (db == 0) //we couldn't read the database provided
    {
        bz_debugMessagef(0, "DEBUG :: League Over Seer :: Error! Could not connect to: %s", SQLiteDB.c_str());
        bz_debugMessage(0, "DEBUG :: League Over Seer :: Unloading League Over Seer plugin...");
        bz_unloadPlugin(Name());
    }

    if (db != 0) //if the database connection succeed and the database is empty, let's create the tables needed
    {
        doQuery("CREATE TABLE IF NOT EXISTS [Players] (BZID INTEGER NOT NULL PRIMARY KEY DEFAULT 0, TEAM TEXT NOT NULL DEFAULT Teamless, SQUAD TEXT);");
    }
}

void leagueOverSeer::Cleanup (void)
{
    bz_debugMessagef(0, "League Over Seer %i.%i.%i (%i) unloaded.", MAJOR, MINOR, REV, BUILD);

    Flush();

    bz_removeCustomSlashCommand("official");
    bz_removeCustomSlashCommand("fm");
    bz_removeCustomSlashCommand("cancel");
    bz_removeCustomSlashCommand("spawn");
    bz_removeCustomSlashCommand("resume");
    bz_removeCustomSlashCommand("pause");
}

void leagueOverSeer::Event(bz_EventData *eventData)
{
    switch (eventData->eventType)
    {
        case bz_eGameEndEvent: //A /gameover or a match has ended
        {
            //Clear the bool variables
            funMatch = false;

            if (matchCanceled && officialMatch && !gameoverReport) //The match was canceled via /gameover or /superkill and we do not want to report these matches
            {
                officialMatch = false; //Match is over
                matchCanceled = false; //Reset the variable for next usage
                teamOnePoints = 0;
                teamTwoPoints = 0;
                bz_debugMessage(DEBUG, "DEBUG :: League Over Seer :: Official match was not reported.");
                bz_sendTextMessage(BZ_SERVER,BZ_ALLUSERS, "Official match was not reported.");
            }
            else if (officialMatch)
            {
                officialMatch = false; //Match is over
                time_t t = time(NULL); //Get the current time
                tm * now = gmtime(&t);
                char match_date[20];

                sprintf(match_date, "%02d-%02d-%02d %02d:%02d:%02d", now->tm_year+1900, now->tm_mon+1, now->tm_mday, now->tm_hour, now->tm_min, now->tm_sec); //Format the date to -> year-month-day hour:minute:second

                //Convert ints to std::string with std::ostringstream
                std::ostringstream teamOnePointsConversion;
                teamOnePointsConversion << (teamOnePoints);
                std::ostringstream teamTwoPointsConversion;
                teamTwoPointsConversion << (teamTwoPoints);
                std::ostringstream matchTimeConversion;
                matchTimeConversion << (bz_getTimeLimit());

                //Keep references to values for quick reference
                std::string matchToSend = "query=reportMatch";
                std::string teamOnePointsFinal = teamOnePointsConversion.str();
                std::string teamTwoPointsFinal = teamTwoPointsConversion.str();
                std::string matchTimeFinal = matchTimeConversion.str();

                //Create the syntax of the parameters that is going to be sent via a URL
                matchToSend += std::string("&teamOneWins=") + std::string(bz_urlEncode(teamOnePointsFinal.c_str()));
                matchToSend += std::string("&teamTwoWins=") + std::string(bz_urlEncode(teamTwoPointsFinal.c_str()));

                matchToSend += std::string("&matchTime=") + std::string(bz_urlEncode(matchTimeFinal.c_str())) +
                std::string("&matchDate=") + std::string(bz_urlEncode(match_date));

                if (rotLeague) //Only add this parameter if it's a rotational league such as OpenLeague
                    matchToSend += std::string("&mapPlayed=") + std::string(bz_urlEncode(map.c_str()));

                matchToSend += std::string("&teamOnePlayers=");

                for (unsigned int i = 0; i < matchPlayers.size(); i++) //Add all the red players to the match report
                {
                    if (matchPlayers.at(i).team == teamOne)
                    {
                        matchToSend += std::string(bz_urlEncode(matchPlayers.at(i).bzid.c_str()));
                        if (i+1 < matchPlayers.size()) //Only add a quote if there is another player on the list
                            matchToSend += ",";
                    }
                }

                matchToSend += std::string("&teamTwoPlayers=");

                for (unsigned int i = 0; i < matchPlayers.size(); i++) //Add all the red players to the match report
                {
                    if (matchPlayers.at(i).team == teamTwo)
                    {
                        matchToSend += std::string(bz_urlEncode(matchPlayers.at(i).bzid.c_str()));
                        if (i+1 < matchPlayers.size()) //Only add a quote if there is another player on the list
                            matchToSend += ",";
                    }
                }

                bz_debugMessagef(DEBUG, "DEBUG :: League Over Seer :: Reporting match data...");
                bz_sendTextMessage(BZ_SERVER,BZ_ALLUSERS, "Reporting match...");

                urlQueries uq; //Make a reference to the url query list
                uq._URL = "match"; //Tell the query list that we have a match to report on the todo list
                _urlQuery.push_back(uq); //Push the information to the todo list

                bz_addURLJob(LEAGUE_URL.c_str(), this, matchToSend.c_str()); //Send the match data to the league website

                //Clear all the structures and scores for next match
                matchPlayers.clear();
                teamOnePoints = 0;
                teamTwoPoints = 0;
            }
            else
                bz_debugMessage(DEBUG, "DEBUG :: League Over Seer :: Fun match was not reported.");
        }
        break;

        case bz_eGameStartEvent: //The countdown has started
        {
            bz_GameStartEndEventData_V1 *startData = (bz_GameStartEndEventData_V1*)eventData;

            if (officialMatch) //Don't waste memory if the match isn't official
            {
                bz_APIIntList *playerList = bz_newIntList();
                bz_getPlayerIndexList(playerList);

                //Set the team scores to zero just in case
                teamOnePoints = 0;
                teamTwoPoints = 0;

                for (unsigned int i = 0; i < playerList->size(); i++)
                {
                    bz_BasePlayerRecord *playerRecord = bz_getPlayerByIndex(playerList->get(i));

                    if (bz_getPlayerTeam(playerList->get(i)) != eObservers) //Check if the player is on the red team
                    {
                        playersInMatch currentPlayer;
                        currentPlayer.team = playerRecord->team; //Add callsign to structure
                        currentPlayer.bzid = playerRecord->bzID.c_str(); //Add bzid to structure

                        matchPlayers.push_back(currentPlayer);
                    }

                    bz_freePlayerRecord(playerRecord);
                }

                bz_deleteIntList(playerList);
            }
        }
        break;

        case bz_ePlayerJoinEvent: //A player joins
        {
            bz_PlayerJoinPartEventData_V1 *joinData = (bz_PlayerJoinPartEventData_V1*)eventData;

            if ((bz_isCountDownActive() || bz_isCountDownInProgress()) && officialMatch) //If there is an official match in progress, notify others who join
                bz_sendTextMessage(BZ_SERVER, joinData->playerID, "*** There is currently an official match in progress, please be respectful. ***");
            else if ((bz_isCountDownActive() || bz_isCountDownInProgress()) && funMatch) //If there is a fun match in progress, notify others who join
                bz_sendTextMessage(BZ_SERVER, joinData->playerID, "*** There is currently a fun match in progress, please be respectful. ***");
        }
        break;

        case bz_eTickEvent: //Tick tock tick tock...
        {
            int totaltanks = bz_getTeamCount(eRogueTeam) + bz_getTeamCount(eRedTeam) + bz_getTeamCount(eGreenTeam) + bz_getTeamCount(eBlueTeam) + bz_getTeamCount(ePurpleTeam);

            if (totaltanks == 0)
            {
                //Incase a boolean gets messed up in the plugin, reset all the plugin variables when there are no players (Observers excluded)
                if (officialMatch) officialMatch = false;
                if (matchCanceled) matchCanceled = false;
                if (funMatch) funMatch = false;
                if (teamOnePoints > 0) teamOnePoints = 0;
                if (teamTwoPoints > 0) teamTwoPoints = 0;

                //This should never happen but just incase the countdown is going when there are no tanks
                if (bz_isCountDownActive())
                    bz_gameOver(253, eObservers);
            }
        }
        break;

        case bz_eCaptureEvent: //Someone caps
        {
            bz_CTFCaptureEventData_V1 *capData = (bz_CTFCaptureEventData_V1*)eventData;

            if (officialMatch) //Only keep score if it's official
            {
                if (capData->teamCapped == teamOne) teamOnePoints++;
                else if (capData->teamCapped == teamTwo) teamTwoPoints++;
            }
        }
        break;

        case bz_eGetPlayerMotto:
        {
            bz_GetPlayerMottoData_V2* mottoEvent = (bz_GetPlayerMottoData_V2*)eventData;
            sqlite3_stmt *getPlayerMotto;

            if (sqlite3_prepare_v2(db, "SELECT `TEAM` FROM `Players` WHERE `BZID` = ?", -1, &getPlayerMotto, 0) == SQLITE_OK)
            {
                //prepare the query
                sqlite3_bind_text(getPlayerMotto, 1, mottoEvent->record->bzID.c_str(), -1, SQLITE_TRANSIENT);

                if (sqlite3_step(getPlayerMotto) == SQLITE_ROW)
                    mottoEvent->motto = (char*)sqlite3_column_text(getPlayerMotto, 0);
                else
                    mottoEvent->motto = "";

                sqlite3_finalize(getPlayerMotto);
            }
            else
            {
                bz_debugMessagef(DEBUG, "DEBUG :: League Over Seer :: SQLite :: bz_eGetPlayerMotto :: Error #%i: %s", sqlite3_errcode(db), sqlite3_errmsg(db));
            }
        }

        default:
        break;
    }
}

bool leagueOverSeer::SlashCommand(int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList *params)
{
    int timeToStart = atoi(params->get(0).c_str());
    bz_BasePlayerRecord *playerData = bz_getPlayerByIndex(playerID);

    if (command == "official") //Someone used the /official command
    {
        if (playerData->team == eObservers) //Observers can't start matches
            bz_sendTextMessage(BZ_SERVER,playerID,"Observers are not allowed to start matches.");
        else if (bz_getTeamCount(eRedTeam) < 2 || bz_getTeamCount(eGreenTeam) < 2 || bz_getTeamCount(eBlueTeam) < 2 || bz_getTeamCount(ePurpleTeam) < 2) //An official match cannot be 1v1 or 2v1
            bz_sendTextMessage(BZ_SERVER,playerID,"You may not have an official match with less than 2 players per team.");
        else if (playerData->verified && playerData->team != eObservers && bz_hasPerm(playerID,"spawn") && !bz_isCountDownActive()) //Check the user is not an obs and is a league member
        {
            officialMatch = true; //Notify the plugin that the match is official
            bz_debugMessagef(DEBUG, "DEBUG :: League Over Seer :: Official match started by %s (%s).",playerData->callsign.c_str(),playerData->ipAddress.c_str());
            bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS, "Official match started by %s.",playerData->callsign.c_str());
            if (timeToStart <= 120 && timeToStart > 5)
                bz_startCountdown (timeToStart, bz_getTimeLimit(), "Server"); //Start the countdown with a custom countdown time limit under 2 minutes
            else
                bz_startCountdown (10, bz_getTimeLimit(), "Server"); //Start the countdown for the official match
        }
        else if (funMatch) //A fun match cannot be declared an official match
            bz_sendTextMessage(BZ_SERVER,playerID,"Fun matches cannot be turned into official matches.");
        else if (!playerData->verified || !bz_hasPerm(playerID,"spawn")) //If they can't spawn, they aren't a league player so they can't start a match
            bz_sendTextMessage(BZ_SERVER,playerID,"Only registered league players may start an official match.");
        else if (bz_isCountDownActive() || bz_isCountDownInProgress()) //A countdown is in progress already
            bz_sendTextMessage(BZ_SERVER,playerID,"There is currently a countdown active, you may not start another.");
        else
            bz_sendTextMessage(BZ_SERVER,playerID,"You do not have permission to run the /official command.");
    }
    else if (command == "fm") //Someone uses the /fm command
    {
        if (!bz_isCountDownActive() && playerData->team != eObservers && bz_hasPerm(playerID,"spawn") && playerData->verified)
        {
            bz_debugMessagef(DEBUG, "DEBUG :: League Over Seer :: Fun match started by %s (%s).",playerData->callsign.c_str(),playerData->ipAddress.c_str());
            bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS, "Fun match started by %s.",playerData->callsign.c_str());
            if (timeToStart <= 120 && timeToStart > 5)
                bz_startCountdown (timeToStart, bz_getTimeLimit(), "Server"); //Start the countdown with a custom countdown time limit under 2 minutes
            else
                bz_startCountdown (10, bz_getTimeLimit(), "Server"); //Start the countdown for the official match
            funMatch = true; //It's a fun match
        }
        else if (bz_isCountDownActive()) //There is already a countdown
            bz_sendTextMessage(BZ_SERVER,playerID,"There is currently a countdown active, you may not start another.");
        else if (playerData->team == eObservers) //Observers can't start matches
            bz_sendTextMessage(BZ_SERVER,playerID,"Observers are not allowed to start matches.");
        else if (!playerData->verified || !bz_hasPerm(playerID,"spawn")) //If they can't spawn, they aren't a league player so they can't start a match
            bz_sendTextMessage(BZ_SERVER,playerID,"Only registered league players may start an official match.");
        else
            bz_sendTextMessage(BZ_SERVER,playerID,"You do not have permission to run the /fm command.");
    }
    else if (command == "cancel")
    {
        if (bz_hasPerm(playerID,"spawn") && bz_isCountDownActive())
        {
            bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS,"Match ended by %s",playerData->callsign.c_str());
            bz_debugMessagef(DEBUG, "DEBUG :: League Over Seer :: Match ended by %s (%s).",playerData->callsign.c_str(),playerData->ipAddress.c_str());

            //Reset the server. Cleanly ends a match
            if (officialMatch) officialMatch = false;
            if (matchCanceled) matchCanceled = false;
            if (funMatch) funMatch = false;
            if (RTW > 0) RTW = 0;
            if (GTW > 0) GTW = 0;
            if (BTW > 0) BTW = 0;
            if (PTW > 0) PTW = 0;

            //End the countdown
            if (bz_isCountDownActive())
                bz_gameOver(253, eObservers);
        }
        else if (!bz_isCountDownActive())
            bz_sendTextMessage(BZ_SERVER,playerID,"There is no match in progress to cancel.");
        else //Not a league player
            bz_sendTextMessage(BZ_SERVER,playerID,"You do not have permission to run the /cancel command.");
    }
    else if (command == "pause")
    {
        if (bz_isCountDownActive() && playerData->team != eObservers && bz_hasPerm(playerID,"spawn") && playerData->verified)
        {
            bz_pauseCountdown(playerData->callsign.c_str());

            bz_setBZDBDouble("_tankSpeed", 0, 0, false);
            bz_setBZDBDouble("_shotSpeed", 0, 0, false);

            bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS,"Countdown paused by ", playerData->callsign.c_str());
        }
    }
    else if (command == "resume")
    {
        bz_resumeCountdown(playerData->callsign.c_str());

        bz_resetBZDBVar("_tankSpeed");
        bz_resetBZDBVar("_shotSpeed");

        bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS,"Countdown Resumed by ", playerData->callsign.c_str());
    }
    else if (command == "spawn")
    {
        if (bz_hasPerm(playerID, "kick"))
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

                if (std::string::npos != std::string(params->get(0).c_str()).find("#"))
                {
                    bz_grantPerm(atoi(std::string(params->get(0).c_str()).substr(0, 1).c_str()), "spawn");
                    bz_sendTextMessagef(BZ_SERVER, eAdministrators, "%s gave spawn perms to %s", bz_getPlayerByIndex(playerID)->callsign.c_str(), bz_getPlayerByIndex(atoi(std::string(params->get(0).c_str()).substr(0, 1).c_str()))->callsign.c_str());
                }
                else if (isDigit(params->get(0).c_str()))
                {
                    bz_grantPerm(atoi(params->get(0).c_str()), "spawn");
                    bz_sendTextMessagef(BZ_SERVER, eAdministrators, "%s gave spawn perms to %s", bz_getPlayerByIndex(playerID)->callsign.c_str(), bz_getPlayerByIndex(atoi(params->get(0).c_str()))->callsign.c_str());
                }
                else if (isValidCallsign(callsignToLookup) >= 0)
                {
                    bz_grantPerm(isValidCallsign(callsignToLookup), "spawn");
                    bz_sendTextMessagef(BZ_SERVER, eAdministrators, "%s gave spawn perms to %s", bz_getPlayerByIndex(playerID)->callsign.c_str(), bz_getPlayerByIndex(isValidCallsign(callsignToLookup))->callsign.c_str());
                }
            }
            else
                bz_sendTextMessage(BZ_SERVER, playerID, "/spawn <player id or callsign>");
        }
        else if (!playerData->admin)
        {
            bz_sendTextMessage(BZ_SERVER,playerID,"You do not have permission to use the /spawn command.");
        }
    }

    bz_freePlayerRecord(playerData);
}

void leagueOverSeer::URLDone(const char* URL, void* data, unsigned int size, bool complete) //Everything went fine with the report
{
    std::string siteData = (char*)(data); //Convert the data to a std::string

    if (_urlQuery.at(0)._URL.compare("match") == 0 && URL == LEAGUE_URL) //The plugin reported the match successfully
    {
        bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS,"%s",(char*)data);
        bz_debugMessagef(DEBUG, "%s",(char*)data);

        _urlQuery.erase(_urlQuery.begin(),_urlQuery.begin()+1); //Tell the plugin that the the match query has been delt with, move to the next url job
    }
}

void leagueOverSeer::URLTimeout(const char* URL, int errorCode) //The league website is down or is not responding, the request timed out
{
    if (_urlQuery.at(0)._URL.compare("match") == 0 && URL == LEAGUE_URL) //Something went wrong while reporting the match, it timed out
    {
        bz_sendTextMessage(BZ_SERVER,BZ_ALLUSERS,">-- WARNING --<");
        bz_sendTextMessage(BZ_SERVER,BZ_ALLUSERS,">-- The request to report the match has timed out. --<");
        bz_sendTextMessage(BZ_SERVER,BZ_ALLUSERS,">-- Please contact a league admin or referee with the match result. --<");
        bz_debugMessage(DEBUG, "DEBUG :: League Over Seer :: The request to report the match has timed out.");

        _urlQuery.erase(_urlQuery.begin(),_urlQuery.begin()+1); //Tell the plugin that the the match query has been delt with, move to the next url job
    }
}

void leagueOverSeer::URLError(const char* URL, int errorCode, const char *errorString) //The server owner must have set up the URLs wrong because this shouldn't happen
{
    if (_urlQuery.at(0)._URL.compare("match") == 0 && URL == LEAGUE_URL) //Something went wrong while reporting the match, no website found
    {
        bz_sendTextMessage(BZ_SERVER,BZ_ALLUSERS,">-- WARNING --<");
        bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS,">-- Match report failed with error code %i - %s --<",errorCode,errorString);
        bz_sendTextMessage(BZ_SERVER,BZ_ALLUSERS,">-- Please contact a league admin or referee with this error and the match result. --<");
        bz_debugMessage(DEBUG, "DEBUG :: League Over Seer :: Match report failed with the following error:");
        bz_debugMessagef(DEBUG, "DEBUG :: League Over Seer :: Error code: %i - %s",errorCode,errorString);

        _urlQuery.erase(_urlQuery.begin(),_urlQuery.begin()+1); //Tell the plugin that the the match query has been delt with, move to the next url job
    }
}

void leagueOverSeer::doQuery(std::string query)
{
    /*
        Execute a SQL query without the need of any return values
    */

    bz_debugMessage(2, "DEBUG :: MoFo Cup :: Executing following SQL query...");
    bz_debugMessagef(2, "DEBUG :: MoFo Cup :: %s", query.c_str());

    char* db_err = 0; //a place to store the error
    int ret = sqlite3_exec(db, query.c_str(), NULL, 0, &db_err); //execute

    if (db_err != 0) //print out any errors
    {
        bz_debugMessage(2, "DEBUG :: MoFo Cup :: SQL ERROR!");
        bz_debugMessagef(2, "DEBUG :: MoFo Cup :: %s", db_err);
    }
}

bool leagueOverSeer::isDigit(std::string myString)
{
    for (int i = 0; i < myString.size(); i++) //Go through entire string
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
            return playerList->get(i);
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
            return true;
    }

    bz_deleteIntList(playerList);

    return false;
}

int leagueOverSeer::loadConfig(const char* cmdLine) //Load the plugin configuration file
{
    PluginConfig config = PluginConfig(cmdLine);
    std::string section = "leagueOverSeer";

    if (config.errors) bz_shutdown(); //Shutdown the server

    //Extract all the data in the configuration file and assign it to plugin variables
    rotLeague = toBool(config.item(section, "ROTATIONAL_LEAGUE"));
    mapchangePath = (config.item(section, "MAPCHANGE_PATH")).c_str();
    SQLiteDB = config.item(section, "SQLITE_DB");
    gameoverReport = toBool(config.item(section, "GAMEOVER_REPORT"));
    LEAGUE_URL = config.item(section, "LEAGUE_OVER_SEER_URL");
    DEBUG = atoi((config.item(section, "DEBUG_LEVEL")).c_str());

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

bool leagueOverSeer::toBool(std::string var) //Turn std::string into a boolean value
{
    if (var == "true" || var == "TRUE" || var == "1") //If the value is true then the boolean is true
        return true;
    else //If it's not true, then it's false.
        return false;
}