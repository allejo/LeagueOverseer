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

Parameters:
/path/to/matchOverSeer.so,/path/to/matchOverSeer.cfg

License:
BSD

Version:
0.9.5 [Codename: Why Me?]
*/

#include <stdio.h>
#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <fstream>
#include <algorithm>

#include "bzfsAPI.h"
#include "plugin_utils.h"

//Define plugin version numbering
const int MAJOR = 0;
const int MINOR = 9;
const int REV = 5;
const int BUILD = 53;

class matchOverSeer : public bz_Plugin, public bz_CustomSlashCommandHandler, public bz_BaseURLHandler
{
	virtual const char* Name (){return "Match Over Seer 0.9.5 (53)";}
	virtual void Init ( const char* config);	
	virtual void Event( bz_EventData *eventData );
	virtual bool SlashCommand( int playerID, bz_ApiString, bz_ApiString, bz_APIStringList*);
	virtual void Cleanup ();
	virtual void URLDone( const char* /*URL*/, void* data, unsigned int size, bool complete ) {
		std::string siteData = (char*)(data); //Convert the data to a std::string
		siteData = bz_tolower(siteData.c_str());
		
		if(std::string::npos != siteData.find("success")) //The plugin reported the match successfully
		{
			bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS,"%s",data);
			bz_debugMessagef(DEBUG,"%s",data);
		}
		else //Something went wrong
		{
			bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS,"The following error has occured while reporting the match:");
			bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS,"%s",data);
			bz_sendTextMessage(BZ_SERVER,BZ_ALLUSERS,"Please contact a league admin or referee with the error and the match result.");
			bz_debugMessage(DEBUG,"The following error has occured while reporting the match:");
			bz_debugMessagef(DEBUG,"%s",data);
		}
	}
	int loadConfig(const char *cmdLine);
	bool toBool(std::string var);
	
	bool officialMatch, matchCanceled, countDownStarted, funMatch, rotLeague;
	double matchStartTime;
	int DEBUG, gracePeriod;
	std::string URL, map;
	const char* mapchangePath; 
	
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
	URL = config.item(section, "WEBSITE_URL");
	gracePeriod = atoi((config.item(section, "GRACE_PERIOD")).c_str());
	DEBUG = atoi((config.item(section, "DEBUG_LEVEL")).c_str());
	
	//Check for errors in the configuration data. If there is an error, shut down the server
	if (rotLeague && (mapchangePath == "/path/to/mapchange.out" || mapchangePath == ""))
	{
		bz_debugMessage(0, "*** Match Over Seer: The path to the mapchange.out file was not choosen. ***");
		bz_shutdown();
	}
	if (URL == "")
	{
		bz_debugMessage(0, "*** Match Over Seer: No URL was choosen to report matches. ***");
		bz_shutdown();
	}
	if (gracePeriod == -999)
	{
		bz_debugMessage(0, "*** Match Over Seer: No grace period was choosen. ***");
		bz_shutdown();
	}
	if (DEBUG > 4 || DEBUG < 0)
	{
		bz_debugMessage(0, "*** Match Over Seer: You have selected an invalid debug level. ***");
		bz_shutdown();
	}
}

void matchOverSeer::Init ( const char* commandLine )
{
	bz_debugMessagef(0, "Match Over Seer %i.%i.%i (%i) loaded.", MAJOR, MINOR, REV, BUILD);
	
	Register(bz_eGameEndEvent);
	Register(bz_eGameStartEvent);
	Register(bz_eSlashCommandEvent);
	Register(bz_ePlayerJoinEvent);
	Register(bz_eTickEvent);

	bz_registerCustomSlashCommand("official", this);
	bz_registerCustomSlashCommand("fm",this);
	
	//Set all boolean values for the plugin to false
	officialMatch=false;
	matchCanceled=false;
	countDownStarted=false;
	funMatch=false;

	loadConfig(commandLine); //Load the configuration data when the plugin is loaded
	
	if(mapchangePath != "" && rotLeague) //Check to see if the plugin is for a rotational league
	{
		//Open the mapchange.out file to see what map is being used
		std::ifstream infile;
		infile.open (mapchangePath);
		getline(infile,map);
		infile.close();
	
		map = map.substr(0, map.length()-5); //Remove the '.conf' from the mapchange.out file

		bz_debugMessagef(DEBUG, "Match Over Seer: Current map being played: %s", map.c_str());
	}
}

void matchOverSeer::Cleanup (void)
{
	bz_debugMessagef(0, "Match Over Seer %i.%i.%i (%i) unloaded.", MAJOR, MINOR, REV, BUILD);
	
	Flush();
	
	bz_removeCustomSlashCommand("official");
	bz_removeCustomSlashCommand("fm");
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
			
			if((command.compare("/gameover") == 0 || command.compare("/superkill") == 0) && (bz_hasPerm(commandData->from,"ENDGAME") || bz_hasPerm(commandData->from,"SUPERKILL"))) //Check if they did a /gameover or /superkill
			{
				if(officialMatch) //Only announce that the match was canceled if it's an official match
				{
					bz_debugMessagef(DEBUG,"Match Over Seer: Official match canceled by %s (%s)",playerData->callsign.c_str(),playerData->ipAddress.c_str());
					bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS, "Official match canceled by %s",playerData->callsign.c_str());
					
					matchCanceled = true; //To prevent reporting a canceled match, let plugin know the match was canceled
				}
			}
			
			bz_freePlayerRecord(playerData);
		}
		break;
		
		case bz_eGameEndEvent:
		{
			//Clear the bool variables
			countDownStarted = false;
			funMatch = false;
			
			if(matchCanceled && officialMatch) //The match was canceled via /gameover or /superkill
			{
				officialMatch = false; //Match is over
				matchCanceled = false; //Reset the variable for next usage
				bz_debugMessage(DEBUG,"Match Over Seer: Official match was not reported.");
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
				std::ostringstream RTW;
				RTW << (bz_getTeamLosses(eRedTeam));
				std::ostringstream GTW;
				GTW << (bz_getTeamLosses(eGreenTeam));
				std::ostringstream MT;
				MT << (bz_getTimeLimit());
				
				//Keep references to values for quick reference
				std::string matchToSend = "";
				std::string redTeamWins = RTW.str();
				std::string greenTeamWins = GTW.str();
				std::string matchTime = MT.str();
				
				//Create the syntax of the parameters that is going to be sent via a URL
				matchToSend = std::string("redTeamWins=") + std::string(bz_urlEncode(greenTeamWins.c_str())) + 
				std::string("&greenTeamWins=") + std::string(bz_urlEncode(redTeamWins.c_str())) + 
				std::string("&matchTime=") + std::string(bz_urlEncode(matchTime.c_str())) + 
				std::string("&matchDate=") + std::string(bz_urlEncode(match_date));
				
				if(rotLeague)
					matchToSend += std::string("&mapPlayed=") + std::string(bz_urlEncode(map.c_str()));
				
				matchToSend += std::string("&redPlayers=");
				
				for (unsigned int i = 0; i < matchRedParticipants.size(); i++) //Go through the player list and check for red team players
				{
					matchToSend += std::string(bz_urlEncode(matchRedParticipants.at(i).callsign.c_str()));
					if(i+1 < matchRedParticipants.size()) //Only add a comma if the next player on the list is red
						matchToSend += "\"";
				}
				
				matchToSend += std::string("&greenPlayers=");
				
				for (unsigned int i = 0; i < matchGreenParticipants.size(); i++) //Now check for green team players...
				{
					matchToSend += std::string(bz_urlEncode(matchGreenParticipants.at(i).callsign.c_str()));
					if(i+1 < matchGreenParticipants.size()) //Only add a comma if the next player on the list is green
						matchToSend += "\"";
				}
			
				bz_debugMessage(DEBUG,"Match Over Seer: Reporting match...");
				bz_sendTextMessage(BZ_SERVER,BZ_ALLUSERS, "Reporting match...");
				
				bz_addURLJob(URL.c_str(), this, matchToSend.c_str()); //Send the match data to the league website

				matchRedParticipants.clear();
				matchGreenParticipants.clear();
			}
			else
			{
				bz_debugMessage(DEBUG,"Match Over Seer: Fun match was not reported.");
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

		case bz_ePlayerJoinEvent:
		{
			bz_PlayerJoinPartEventData_V1 *joinData = (bz_PlayerJoinPartEventData_V1*)eventData;
			
			if((bz_isCountDownActive() || bz_isCountDownInProgress()) && officialMatch) //If there is an official match in progress, notify others who join
				bz_sendTextMessage(BZ_SERVER,joinData->playerID, "There is currently an official match in progress, please be respectful.");
			else if((bz_isCountDownActive() || bz_isCountDownInProgress()) && (funMatch || !countDownStarted)) //If there is a fun match in progress, notify others who join
				bz_sendTextMessage(BZ_SERVER,joinData->playerID, "There is currently a fun match in progress, please be respectful.");
		}
		break;
		
		case bz_eTickEvent:
		{
			int totaltanks = bz_getTeamCount(eRogueTeam) + bz_getTeamCount(eRedTeam) + bz_getTeamCount(eGreenTeam) + bz_getTeamCount(eBlueTeam) + bz_getTeamCount(ePurpleTeam);
			
			if (totaltanks == 0 && (!officialMatch && !matchCanceled && !countDownStarted && !funMatch))
			{
				//Incase a boolean gets messed up in the plugin, reset all the plugin variables when there are no players (Observers excluded)
				officialMatch=false;
				matchCanceled=false;
				countDownStarted=false;
				funMatch=false;
				
				//This should never happen but just incase the countdown is going when there are no tanks 
				if(bz_isCountDownActive)
					bz_gameOver(253,eObservers);
			}
		}
		
		default:break; //I never really understand the point of this... -.-"
	}
}

bool matchOverSeer::SlashCommand(int playerID, bz_ApiString command, bz_ApiString /*message*/, bz_APIStringList *params)
{
	bz_BasePlayerRecord *playerData = bz_getPlayerByIndex(playerID);
	
	if(command == "official") //Someone used the /official command
	{
		if(playerData->verified && playerData->team != eObservers && bz_hasPerm(playerID,"spawn") && !bz_isCountDownActive() && !countDownStarted) //Check the user is not an obs and is a league member
		{
			officialMatch = true; //Notify the plugin that the match is official
			bz_debugMessagef(DEBUG,"Match Over Seer: Official match started by %s (%s).",playerData->callsign.c_str(),playerData->ipAddress.c_str());
			bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS, "Official match started by %s.",playerData->callsign.c_str());
			bz_startCountdown (10, bz_getTimeLimit(), "Server"); //Start the countdown for the official match
			countDownStarted = true;
		}	
		else if(!(bz_getCurrentTime()>matchStartTime+60) && playerData->team != eObservers && bz_hasPerm(playerID,"spawn") && !funMatch && !officialMatch)
		{
			officialMatch = true;
			bz_debugMessagef(DEBUG,"Match Over Seer: /countdown Match has been turned into an official match by %s (%s).",playerData->callsign.c_str(),playerData->ipAddress.c_str());
			bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS, "This is now an official match requested by %s.",playerData->callsign.c_str());
		}
		else if((bz_getCurrentTime()>matchStartTime+60) && playerData->team != eObservers && bz_hasPerm(playerID,"spawn") && !funMatch)
			bz_sendTextMessage(BZ_SERVER,playerID,"You may no longer request an official match.");
		else if(funMatch)
			bz_sendTextMessage(BZ_SERVER,playerID,"Fun matches cannot be turned into official matches.");
		else if(playerData->team == eObservers) //Observers can't start matches... Duh
			bz_sendTextMessage(BZ_SERVER,playerID,"Observers are not allowed to start matches.");
		else if(!playerData->verified || !bz_hasPerm(playerID,"spawn")) //People who can't spawn can't start matches either... Derp!
			bz_sendTextMessage(BZ_SERVER,playerID,"Only registered league players may start an official match.");
		else if((bz_isCountDownActive() || bz_isCountDownInProgress()) && countDownStarted)
			bz_sendTextMessage(BZ_SERVER,playerID,"There is currently a countdown active, you may not start another.");
		else
			bz_sendTextMessage(BZ_SERVER,playerID,"You do not have permission to run the /official command");
	}
	else if(command == "fm")
	{
		if(!bz_isCountDownActive() && !countDownStarted && playerData->team != eObservers && bz_hasPerm(playerID,"spawn") && playerData->verified)
		{
			bz_debugMessagef(DEBUG,"Match Over Seer: Fun match started by %s (%s).",playerData->callsign.c_str(),playerData->ipAddress.c_str());
			bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS, "Fun match started by %s.",playerData->callsign.c_str());
			bz_startCountdown (10, bz_getTimeLimit(), "Server"); //Start the countdown for the official match
			countDownStarted = true;
			funMatch = true;
		}
		else if(bz_isCountDownActive() || countDownStarted)
			bz_sendTextMessage(BZ_SERVER,playerID,"There is currently a countdown active, you may not start another.");
		else if(playerData->team == eObservers) //Observers can't start matches
			bz_sendTextMessage(BZ_SERVER,playerID,"Observers are not allowed to start matches.");
		else if(!playerData->verified || !bz_hasPerm(playerID,"spawn")) //If they can't spawn, they aren't a league player so they can't start a match
			bz_sendTextMessage(BZ_SERVER,playerID,"Only registered league players may start an official match.");
		else
			bz_sendTextMessage(BZ_SERVER,playerID,"You do not have permission to run the /fm command");
	}
	
	bz_freePlayerRecord(playerData);	
}
