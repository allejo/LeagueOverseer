/*
Copyright (c) 2011 Vladimir Jimenez
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Author:
Vlad Jimenez (allejo)

Description:
Automatically reports official matches to league websites to make referee's
jobs easier.
	League Support: Ducati, GU, Open League

Slash Commands:
/official
/fm
/teamlist

Parameters:
/path/to/matchOverSeer.so,/path/to/matchOverSeer.cfg

License:
BSD

Version:
1.0.0 [Codename: I blame brad]
*/

#include <stdio.h>
#include <iostream>
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
const int MAJOR = 1;
const int MINOR = 0;
const int REV = 0;
const int BUILD = 61;

class matchOverSeer : public bz_Plugin, public bz_CustomSlashCommandHandler, public bz_BaseURLHandler
{
	virtual const char* Name (){return "Match Over Seer 1.0.0 (61)";}
	virtual void Init ( const char* config);	
	virtual void Event( bz_EventData *eventData );
	virtual bool SlashCommand( int playerID, bz_ApiString, bz_ApiString, bz_APIStringList*);
	virtual void Cleanup ();
	virtual void URLDone( const char* URL, void* data, unsigned int size, bool complete );
	virtual void URLTimeout(const char* URL, int errorCode);
	virtual void URLError(const char* URL, int errorCode, const char *errorString);
	virtual int loadConfig(const char *cmdLine);
	virtual bool toBool(std::string var);
	
	//All the variables that will be used in the plugin
	bool officialMatch, matchCanceled, countDownStarted, funMatch, rotLeague, gameoverReport;
	double matchStartTime;
	int DEBUG, gracePeriod, RTW, GTW;
	std::string REPORT_URL, QUERY_URL, map;
	const char* mapchangePath;
	
	struct teamQueries { //Stores all the queries that a player request
		int _playerID;
	};
	std::vector<teamQueries> _playerIDs;
	
	struct urlQueries { //Stores the order of match reports and team queries
		std::string _URL;
	};
	std::vector<urlQueries> _urlQuery;
	
	struct matchRedPlayers { //Maintains the players that started the match on the red team
		bz_ApiString callsign;
	};
	std::vector<matchRedPlayers> matchRedParticipants;
	
	struct matchGreenPlayers { //Maintains the players that started the match on the green team
		bz_ApiString callsign;
	};
	std::vector<matchGreenPlayers> matchGreenParticipants;
};

BZ_PLUGIN(matchOverSeer);

double lastQuery[256] = {0}; //Initialize the array with all 0s

void matchOverSeer::URLDone( const char* URL, void* data, unsigned int size, bool complete ) //Everything went fine with the report
{
	std::string siteData = (char*)(data); //Convert the data to a std::string
	
	if(_urlQuery.at(0)._URL.compare("query") == 0 && URL == QUERY_URL) //Someone queried for teams
	{	
		char* token; //Store tokens
		bz_sendTextMessage(BZ_SERVER,_playerIDs.at(0)._playerID," Team List");
		bz_sendTextMessage(BZ_SERVER,_playerIDs.at(0)._playerID," ---------");
		
		token = strtok ((char*)data,"\n"); //Tokenize
		while (token != NULL)
		{
			if(strchr(token,'*') != NULL)
				bz_sendTextMessagef(BZ_SERVER,_playerIDs.at(0)._playerID," %s",token); //Print out team name
			else
				bz_sendTextMessagef(BZ_SERVER,_playerIDs.at(0)._playerID,"   %s",token); //Print out callsign
			token = strtok (NULL, "\n");
		}
		
		_urlQuery.erase(_urlQuery.begin(),_urlQuery.begin()+1); //Tell the plugin that the the match query has been delt with, move to the next url job
		_playerIDs.erase(_playerIDs.begin(),_playerIDs.begin()+1); //Tell the plugin that this player has received his/her information, move to the next player
	}
	else if(_urlQuery.at(0)._URL.compare("match") == 0 && URL == REPORT_URL) //The plugin reported the match successfully
	{
		bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS,"%s",(char*)data);
		bz_debugMessagef(DEBUG,"%s",(char*)data);
		
		_urlQuery.erase(_urlQuery.begin(),_urlQuery.begin()+1); //Tell the plugin that the the match query has been delt with, move to the next url job
	}
}

void matchOverSeer::URLTimeout(const char* URL, int errorCode) //The league website is down or is not responding, the request timed out
{
	if(_urlQuery.at(0)._URL.compare("match") == 0 && URL == REPORT_URL) //Something went wrong while reporting the match, it timed out
	{
		bz_sendTextMessage(BZ_SERVER,BZ_ALLUSERS,">-- WARNING --<");
		bz_sendTextMessage(BZ_SERVER,BZ_ALLUSERS,">-- The request to report the match has timed out. --<");
		bz_sendTextMessage(BZ_SERVER,BZ_ALLUSERS,">-- Please contact a league admin or referee with the match result. --<");
		bz_debugMessage(DEBUG,"DEBUG::Match Over Seer::The request to report the match has timed out.");
		
		_urlQuery.erase(_urlQuery.begin(),_urlQuery.begin()+1); //Tell the plugin that the the match query has been delt with, move to the next url job
	}
	else if(_urlQuery.at(0)._URL.compare("query") == 0 && URL == QUERY_URL) //Something went wrong with the team query, it timed out
	{
		bz_sendTextMessage(BZ_SERVER,_playerIDs.at(0)._playerID, "The request to query the league website has timed out. Please try again later.");
		
		_urlQuery.erase(_urlQuery.begin(),_urlQuery.begin()+1); //Tell the plugin that the the match query has been delt with, move to the next url job
		_playerIDs.erase(_playerIDs.begin(),_playerIDs.begin()+1); //Tell the plugin that this player has received his/her information, move to the next player
	}
}

void matchOverSeer::URLError(const char* URL, int errorCode, const char *errorString) //The server owner must have set up the URLs wrong because this shouldn't happen
{
	if(_urlQuery.at(0)._URL.compare("match") == 0 && URL == REPORT_URL) //Something went wrong while reporting the match, no website found
	{
		bz_sendTextMessage(BZ_SERVER,BZ_ALLUSERS,">-- WARNING --<");
		bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS,">-- Match report failed with error code %i - %s --<",errorCode,errorString);
		bz_sendTextMessage(BZ_SERVER,BZ_ALLUSERS,">-- Please contact a league admin or referee with this error and the match result. --<");
		bz_debugMessage(DEBUG,"DEBUG::Match Over Seer::Match report failed with the following error:");
		bz_debugMessagef(DEBUG,"DEBUG::Match Over Seer::Error code: %i - %s",errorCode,errorString);
		
		_urlQuery.erase(_urlQuery.begin(),_urlQuery.begin()+1); //Tell the plugin that the the match query has been delt with, move to the next url job
	}
	else if(_urlQuery.at(0)._URL.compare("query") == 0 && URL == QUERY_URL) //Something went wrong with the team query, no website found
	{
		bz_sendTextMessagef(BZ_SERVER,_playerIDs.at(0)._playerID, "Your team query failed with error code %i - %s",errorCode,errorString);
		bz_sendTextMessagef(BZ_SERVER,_playerIDs.at(0)._playerID, "Please contact a league admin with this error as this should not happen.");
		
		_urlQuery.erase(_urlQuery.begin(),_urlQuery.begin()+1); //Tell the plugin that the the match query has been delt with, move to the next url job
		_playerIDs.erase(_playerIDs.begin(),_playerIDs.begin()+1); //Tell the plugin that this player has received his/her information, move to the next player
	}
}

bool matchOverSeer::toBool(std::string var) //Turn std::string into a boolean value
{
   if(var == "true" || var == "TRUE" || var == "1") //If the value is true then the boolean is true
      return true;
   else //If it's not true, then it's false.
      return false;
}

int matchOverSeer::loadConfig(const char* cmdLine) //Load the plugin configuration file
{
	PluginConfig config = PluginConfig(cmdLine);
	std::string section = "matchOverSeer";
	
	if (config.errors) bz_shutdown(); //Shutdown the server
	
	//Extract all the data in the configuration file and assign it to plugin variables
	rotLeague = toBool(config.item(section, "ROTATIONAL_LEAGUE"));
	mapchangePath = (config.item(section, "MAPCHANGE_PATH")).c_str();
	gameoverReport = toBool(config.item(section, "GAMEOVER_REPORT"));
	REPORT_URL = config.item(section, "MATCH_REPORT_URL");
	QUERY_URL = config.item(section, "TEAM_QUERY_URL");
	gracePeriod = atoi((config.item(section, "GRACE_PERIOD")).c_str());
	DEBUG = atoi((config.item(section, "DEBUG_LEVEL")).c_str());
	
	//Check for errors in the configuration data. If there is an error, shut down the server
	if (REPORT_URL == "" || QUERY_URL == "")
	{
		bz_debugMessage(0, "*** DEBUG::Match Over Seer::No URLs were choosen to report matches or query teams. ***");
		bz_shutdown();
	}
	if (gracePeriod > bz_getTimeLimit())
	{
		bz_debugMessage(0, "*** DEBUG::Match Over Seer::Invalid grace period in the configuration file. ***");
		bz_shutdown();
	}
	if (DEBUG > 4 || DEBUG < 0)
	{
		bz_debugMessage(0, "*** DEBUG::Match Over Seer::Invalid debug level in the configuration file. ***");
		bz_shutdown();
	}
}

void matchOverSeer::Init ( const char* commandLine )
{
	bz_debugMessagef(0, "Match Over Seer %i.%i.%i (%i) loaded.", MAJOR, MINOR, REV, BUILD);
	
	Register(bz_eCaptureEvent);
	Register(bz_eGameEndEvent);
	Register(bz_eGameStartEvent);
	Register(bz_eSlashCommandEvent);
	Register(bz_ePlayerJoinEvent);
	Register(bz_ePlayerPartEvent);
	Register(bz_eTickEvent);

	bz_registerCustomSlashCommand("official", this);
	bz_registerCustomSlashCommand("fm",this);
	bz_registerCustomSlashCommand("teamlist",this);
	
	//Set all boolean values for the plugin to false
	officialMatch=false;
	matchCanceled=false;
	countDownStarted=false;
	funMatch=false;
	RTW = 0;
	GTW = 0;

	loadConfig(commandLine); //Load the configuration data when the plugin is loaded
	
	if(mapchangePath != "" && rotLeague) //Check to see if the plugin is for a rotational league
	{
		//Open the mapchange.out file to see what map is being used
		std::ifstream infile;
		infile.open (mapchangePath);
		getline(infile,map);
		infile.close();
	
		map = map.substr(0, map.length()-5); //Remove the '.conf' from the mapchange.out file

		bz_debugMessagef(DEBUG, "DEBUG::Match Over Seer::Current map being played: %s", map.c_str());
	}
}

void matchOverSeer::Cleanup (void)
{
	bz_debugMessagef(0, "Match Over Seer %i.%i.%i (%i) unloaded.", MAJOR, MINOR, REV, BUILD);
	
	Flush();
	
	bz_removeCustomSlashCommand("official");
	bz_removeCustomSlashCommand("fm");
	bz_removeCustomSlashCommand("teamlist");
}

void matchOverSeer::Event(bz_EventData *eventData)
{
	switch (eventData->eventType)
	{
		case bz_eSlashCommandEvent: //Someone uses a slash command
		{
			bz_SlashCommandEventData_V1 *commandData = (bz_SlashCommandEventData_V1*)eventData;
			bz_BasePlayerRecord *playerData = bz_getPlayerByIndex(commandData->from);
			std::string command = commandData->message.c_str(); //Use std::string for quick reference
			
			if((command.compare("/gameover") == 0 || command.compare("/superkill") == 0) && (bz_hasPerm(commandData->from,"ENDGAME") || bz_hasPerm(commandData->from,"SUPERKILL")) && gameoverReport) //Check if they did a /gameover or /superkill
			{
				if(officialMatch && bz_isCountDownActive) //Only announce that the match was canceled if it's an official match
				{
					bz_debugMessagef(DEBUG,"DEBUG::Match Over Seer::Official match canceled by %s (%s)",playerData->callsign.c_str(),playerData->ipAddress.c_str());
					bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS, "Official match canceled by %s",playerData->callsign.c_str());
					
					matchCanceled = true; //To prevent reporting a canceled match, let plugin know the match was canceled
				}
			}
			
			bz_freePlayerRecord(playerData);
		}
		break;
		
		case bz_eGameEndEvent: //A /gameover or a match has ended
		{
			//Clear the bool variables
			countDownStarted = false;
			funMatch = false;
			
			if(matchCanceled && officialMatch && !gameoverReport) //The match was canceled via /gameover or /superkill and we do not want to report these matches
			{
				officialMatch = false; //Match is over
				matchCanceled = false; //Reset the variable for next usage
				RTW = 0;
				GTW = 0;
				bz_debugMessage(DEBUG,"DEBUG::Match Over Seer::Official match was not reported.");
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
				std::ostringstream myRTW;
				myRTW << (RTW);
				std::ostringstream myGTW;
				myGTW << (GTW);
				std::ostringstream MT;
				MT << (bz_getTimeLimit());
				
				//Keep references to values for quick reference
				std::string matchToSend = "";
				std::string redTeamWins = myRTW.str();
				std::string greenTeamWins = myGTW.str();
				std::string matchTime = MT.str();
				
				//Create the syntax of the parameters that is going to be sent via a URL
				matchToSend = std::string("redTeamWins=") + std::string(bz_urlEncode(redTeamWins.c_str())) + 
				std::string("&greenTeamWins=") + std::string(bz_urlEncode(greenTeamWins.c_str())) + 
				std::string("&matchTime=") + std::string(bz_urlEncode(matchTime.c_str())) + 
				std::string("&matchDate=") + std::string(bz_urlEncode(match_date));
				
				if(rotLeague) //Only add this parameter if it's a rotational league such as OpenLeague
					matchToSend += std::string("&mapPlayed=") + std::string(bz_urlEncode(map.c_str()));
				
				matchToSend += std::string("&redPlayers=");
				
				for (unsigned int i = 0; i < matchRedParticipants.size(); i++) //Add all the red players to the match report
				{
					matchToSend += std::string(bz_urlEncode(matchRedParticipants.at(i).callsign.c_str()));
					if(i+1 < matchRedParticipants.size()) //Only add a quote if there is another player on the list
						matchToSend += "\"";
				}
				
				matchToSend += std::string("&greenPlayers=");
				
				for (unsigned int i = 0; i < matchGreenParticipants.size(); i++) //Now add all the green players
				{
					matchToSend += std::string(bz_urlEncode(matchGreenParticipants.at(i).callsign.c_str()));
					if(i+1 < matchGreenParticipants.size()) //Only add a quote if there is another player on the list
						matchToSend += "\"";
				}
			
				//Match data stored in server logs
				bz_debugMessage(DEBUG,"*** Final Match Data ***");
				bz_debugMessagef(DEBUG,"Match Time Limit: %s",matchTime.c_str());
				bz_debugMessagef(DEBUG,"Match Date: %s", match_date);
				if(rotLeague)
					bz_debugMessagef(DEBUG,"Map Played: %s",map.c_str());
				bz_debugMessagef(DEBUG, "Red Team (%s)",redTeamWins.c_str());
				for (unsigned int i = 0; i < matchRedParticipants.size(); i++)
					bz_debugMessagef(DEBUG, "  %s",matchRedParticipants.at(i).callsign.c_str());
				bz_debugMessagef(DEBUG, "Green Team (%s)",greenTeamWins.c_str());
				for (unsigned int i = 0; i < matchGreenParticipants.size(); i++)
					bz_debugMessagef(DEBUG, "  %s",matchGreenParticipants.at(i).callsign.c_str());
				bz_debugMessagef(DEBUG,"*** End of Match Data ***");
				
				bz_debugMessagef(DEBUG,"DEBUG::Match Over Seer::Reporting match data...");
				bz_sendTextMessage(BZ_SERVER,BZ_ALLUSERS, "Reporting match...");
				
				urlQueries uq; //Make a reference to the url query list
				uq._URL = "match"; //Tell the query list that we have a match to report on the todo list
				_urlQuery.push_back(uq); //Push the information to the todo list
				
				bz_addURLJob(REPORT_URL.c_str(), this, matchToSend.c_str()); //Send the match data to the league website

				//Clear all the structures and scores for next match
				matchRedParticipants.clear();
				matchGreenParticipants.clear();
				RTW = 0;
				GTW = 0;
			}
			else
			{
				bz_debugMessage(DEBUG,"DEBUG::Match Over Seer::Fun match was not reported.");
			}
		}
		break;
		
		case bz_eGameStartEvent: //The countdown has started
		{
			bz_GameStartEndEventData_V1 *startData = (bz_GameStartEndEventData_V1*)eventData;
			
			if(officialMatch) //Don't waste memory if the match isn't official
			{
				bz_APIIntList *playerList = bz_newIntList();
				bz_getPlayerIndexList(playerList);
				
				//Set the team scores to zero just in case
				RTW = 0;
				GTW = 0;
				
				for ( unsigned int i = 0; i < playerList->size(); i++ ){
					bz_BasePlayerRecord *playerTeam = bz_getPlayerByIndex(playerList->get(i));
					
					if (bz_getPlayerTeam(playerList->get(i)) == eRedTeam) //Check if the player is on the red team
					{
						matchRedPlayers matchRedData;
						matchRedData.callsign = playerTeam->callsign.c_str(); //Add callsign to structure
					
						matchRedParticipants.push_back(matchRedData);
					}
					else if (bz_getPlayerTeam(playerList->get(i)) == eGreenTeam) //Check if the player is on the green team
					{
						matchGreenPlayers matchGreenData;
						matchGreenData.callsign = playerTeam->callsign.c_str(); //Add callsign to structure
					
						matchGreenParticipants.push_back(matchGreenData);
					}
					
					bz_freePlayerRecord(playerTeam);
				}
			
				bz_deleteIntList(playerList);
			}
			else if(!countDownStarted && !funMatch) //Match was started with /countdown
			{
				matchStartTime = bz_getCurrentTime();
			}
		}
		break;

		case bz_ePlayerJoinEvent: //A player joins
		{
			bz_PlayerJoinPartEventData_V1 *joinData = (bz_PlayerJoinPartEventData_V1*)eventData;
			
			if((bz_isCountDownActive() || bz_isCountDownInProgress()) && officialMatch) //If there is an official match in progress, notify others who join
				bz_sendTextMessage(BZ_SERVER,joinData->playerID, "*** There is currently an official match in progress, please be respectful. ***");
			else if((bz_isCountDownActive() || bz_isCountDownInProgress()) && (funMatch || !countDownStarted)) //If there is a fun match in progress, notify others who join
				bz_sendTextMessage(BZ_SERVER,joinData->playerID, "*** There is currently a fun match in progress, please be respectful. ***");
		}
		break;
		
		case bz_ePlayerPartEvent: //A player leaves
		{
			bz_PlayerJoinPartEventData_V1 *partData = (bz_PlayerJoinPartEventData_V1*)eventData;
			lastQuery[partData->playerID] = 0; //Reset the last query time for the player slot
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
				if (countDownStarted) countDownStarted = false;
				if (funMatch) funMatch = false;
				if (RTW > 0) RTW = 0;
				if (GTW > 0) GTW = 0;
				
				//This should never happen but just incase the countdown is going when there are no tanks 
				if(bz_isCountDownActive())
					bz_gameOver(253,eObservers);
			}
		}
		break;
		
		case bz_eCaptureEvent: //Someone caps
		{
			bz_CTFCaptureEventData_V1 *capData = (bz_CTFCaptureEventData_V1*)eventData;
			
			if(officialMatch) //Only keep score if it's official
			{
				if(capData->teamCapped == eRedTeam) //Green team caps
					GTW++;
				else if(capData->teamCapped == eGreenTeam) //Red team caps
					RTW++;
			}
		}
		break;
		
		default:break; //I never really understand the point of this... -.-"
	}
}

bool matchOverSeer::SlashCommand(int playerID, bz_ApiString command, bz_ApiString /*message*/, bz_APIStringList *params)
{
	int timeToStart = atoi(params->get(0).c_str());
	bz_BasePlayerRecord *playerData = bz_getPlayerByIndex(playerID);
	
	if(command == "official") //Someone used the /official command
	{	
		if(playerData->team == eObservers) //Observers can't start matches
			bz_sendTextMessage(BZ_SERVER,playerID,"Observers are not allowed to start matches.");
		else if(bz_getTeamCount(eRedTeam) < 2 || bz_getTeamCount(eGreenTeam) < 2) //An official match cannot be 1v1 or 2v1
			bz_sendTextMessage(BZ_SERVER,playerID,"You may not have an official match with less than 2 players per team.");
		else if(playerData->verified && playerData->team != eObservers && bz_hasPerm(playerID,"spawn") && !bz_isCountDownActive() && !countDownStarted) //Check the user is not an obs and is a league member
		{
			officialMatch = true; //Notify the plugin that the match is official
			bz_debugMessagef(DEBUG,"DEBUG::Match Over Seer::Official match started by %s (%s).",playerData->callsign.c_str(),playerData->ipAddress.c_str());
			bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS, "Official match started by %s.",playerData->callsign.c_str());
			if(timeToStart <= 120 && timeToStart > 5)
				bz_startCountdown (timeToStart, bz_getTimeLimit(), "Server"); //Start the countdown with a custom countdown time limit under 2 minutes
			else
				bz_startCountdown (10, bz_getTimeLimit(), "Server"); //Start the countdown for the official match
			countDownStarted = true; //Variable used to notify the plugin that the match was not started with /countdown
		}	
		else if(!(bz_getCurrentTime()>matchStartTime+60) && playerData->team != eObservers && bz_hasPerm(playerID,"spawn") && !funMatch && !officialMatch) //A match started with /countdown has not been declared official
		{
			officialMatch = true; //Notify the plugin that the match is official
			bz_debugMessagef(DEBUG,"DEBUG::Match Over Seer::/countdown Match has been turned into an official match by %s (%s).",playerData->callsign.c_str(),playerData->ipAddress.c_str());
			bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS, "This is now an official match requested by %s.",playerData->callsign.c_str());
		}
		else if((bz_getCurrentTime()>matchStartTime+60) && playerData->team != eObservers && bz_hasPerm(playerID,"spawn") && !funMatch) //Grace period is over
			bz_sendTextMessage(BZ_SERVER,playerID,"You may no longer request an official match.");
		else if(funMatch) //A fun match cannot be declared an official match
			bz_sendTextMessage(BZ_SERVER,playerID,"Fun matches cannot be turned into official matches.");
		else if(!playerData->verified || !bz_hasPerm(playerID,"spawn")) //If they can't spawn, they aren't a league player so they can't start a match
			bz_sendTextMessage(BZ_SERVER,playerID,"Only registered league players may start an official match.");
		else if((bz_isCountDownActive() || bz_isCountDownInProgress()) && countDownStarted) //A countdown is in progress already
			bz_sendTextMessage(BZ_SERVER,playerID,"There is currently a countdown active, you may not start another.");
		else
			bz_sendTextMessage(BZ_SERVER,playerID,"You do not have permission to run the /official command.");
	}
	else if(command == "fm") //Someone uses the /fm command
	{
		if(!bz_isCountDownActive() && !countDownStarted && playerData->team != eObservers && bz_hasPerm(playerID,"spawn") && playerData->verified)
		{
			bz_debugMessagef(DEBUG,"DEBUG::Match Over Seer::Fun match started by %s (%s).",playerData->callsign.c_str(),playerData->ipAddress.c_str());
			bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS, "Fun match started by %s.",playerData->callsign.c_str());
			if(timeToStart <= 120 && timeToStart > 5)
				bz_startCountdown (timeToStart, bz_getTimeLimit(), "Server"); //Start the countdown with a custom countdown time limit under 2 minutes
			else
				bz_startCountdown (10, bz_getTimeLimit(), "Server"); //Start the countdown for the official match
			countDownStarted = true; //Variable used to notify the plugin that the match was not started with /countdown
			funMatch = true; //It's a fun match
		}
		else if(bz_isCountDownActive() || countDownStarted) //There is already a countdown
			bz_sendTextMessage(BZ_SERVER,playerID,"There is currently a countdown active, you may not start another.");
		else if(playerData->team == eObservers) //Observers can't start matches
			bz_sendTextMessage(BZ_SERVER,playerID,"Observers are not allowed to start matches.");
		else if(!playerData->verified || !bz_hasPerm(playerID,"spawn")) //If they can't spawn, they aren't a league player so they can't start a match
			bz_sendTextMessage(BZ_SERVER,playerID,"Only registered league players may start an official match.");
		else
			bz_sendTextMessage(BZ_SERVER,playerID,"You do not have permission to run the /fm command.");
	}
	else if(command == "teamlist") //Someone uses the /teamlist command
	{
		if(bz_hasPerm(playerID,"spawn")) //Check to make sure the player is a registered for the league
		{
			if(lastQuery[playerID] + 60 > bz_getCurrentTime() && lastQuery[playerID] != 0) //The player has alrady sent a query in the past 60 seconds so don't allow spam
				bz_sendTextMessagef(BZ_SERVER,playerID,"Please wait %i seconds few you query the server again.",(int)((lastQuery[playerID] + 60) - bz_getCurrentTime()));
			else
			{
				teamQueries tq; //Make a reference to the team query structure
				tq._playerID = playerID; //Add the player to the list of players who have requested a query
				_playerIDs.push_back(tq); //Push the player id into a structure
				
				urlQueries uq; //Make a reference to the url query structure
				uq._URL = "query"; //Tell the plugin that we have a query in our todo list
				_urlQuery.push_back(uq); //Tell the structure that a team query was requested
				
				lastQuery[playerID] = bz_getCurrentTime(); //Set the time of the query in order to avoid spam or attacking the server
				std::string playersToSend = std::string("players="); //Create the string to send
				
				bz_APIIntList *playerList = bz_newIntList(); //Get the list of valid player ids
				bz_getPlayerIndexList(playerList);
				
				for ( unsigned int i = 0; i < playerList->size(); i++ )
				{
					bz_BasePlayerRecord *playerTeam = bz_getPlayerByIndex(playerList->get(i)); //Get the player record for each player
					
					playersToSend += std::string(bz_urlEncode(playerTeam->callsign.c_str())); //Add the callsign to the string
					if(i+1 < playerList->size()) //Only add a quote if there is another player on the list
						playersToSend += "\"";
						
					bz_freePlayerRecord(playerTeam);
				}
				
				bz_addURLJob(QUERY_URL.c_str(), this, playersToSend.c_str()); //Send a list of players to the team query script
				
				bz_deleteIntList(playerList);
			}
		}
		else //Not a league player
			bz_sendTextMessage(BZ_SERVER,playerID,"You do not have permission to run the /teamlist command.");
	}
	
	bz_freePlayerRecord(playerData);	
}
