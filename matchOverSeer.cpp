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
Automatic match reports sent to the league website.

Slash Commands:
/match
/fm

License:
BSD

Version:
0.9.3 [Codename: Chip and Dale]
*/

#include <stdio.h>
#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>

#include "bzfsAPI.h"
#include "plugin_utils.h"

class MyURLHandler: public bz_BaseURLHandler {
public:
	virtual void URLDone( const char* /*URL*/, void * data, unsigned int size, bool complete ) {
		bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS,"%s",data);
	}
};

MyURLHandler myUrl;

class matchOverSeer : public bz_Plugin, public bz_CustomSlashCommandHandler
{
	virtual const char* Name (){return "Match Over Seer 0.9.3 (48)";}
	virtual void Init ( const char* config);	
	virtual void Event( bz_EventData *eventData );
	virtual bool SlashCommand( int playerID, bz_ApiString, bz_ApiString, bz_APIStringList*);
	virtual void Cleanup ();
	
	bool officialMatch, matchCanceled, countDownStarted, funMatch;
	double matchStartTime;
	
	struct matchPlayers { //Maintains the players that started the match and their team color
			bz_ApiString callsign;
			bz_eTeamType team;
	};
	std::vector<matchPlayers> matchParticipants;
};

//Define plugin version numbering
const int MAJOR = 0;
const int MINOR = 9;
const int REV = 3;
const int BUILD = 48;

const int DEBUG = 1; //The debug level that is going to be used for messages that the plugin sends
const int gracePeriod = 60; //Amount of seconds that a player has to turn a /countdown match to an official match

std::string URL = "http://localhost/auto_report.php";

BZ_PLUGIN(matchOverSeer);

void matchOverSeer::Init ( const char* /*commandLine*/ )
{
	bz_debugMessagef(0, "Match Over Seer %i.%i.%i (%i) loaded.", MAJOR, MINOR, REV, BUILD);
	
	Register(bz_eGameEndEvent);
	Register(bz_eGameStartEvent);
	Register(bz_eSlashCommandEvent);
	Register(bz_ePlayerJoinEvent);

	bz_registerCustomSlashCommand("official", this);
	bz_registerCustomSlashCommand("fm",this);
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
			
			if(command.compare("/gameover") == 0 || command.compare("/superkill") == 0) //Check if they did a /gameover or /superkill
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
				RTW << (bz_getTeamWins(eRedTeam));
				std::ostringstream GTW;
				GTW << (bz_getTeamWins(eGreenTeam));
				std::ostringstream MT;
				MT << (bz_getTimeLimit());
				
				//Keep references to values for quick reference
				std::string matchToSend = "";
				std::string redTeamWins = RTW.str();
				std::string greenTeamWins = GTW.str();
				std::string matchTime = MT.str();
				std::string mapPlayed = bz_getPublicDescription().c_str();
				
				//Create the syntax of the parameters that is going to be sent via a URL
				matchToSend = std::string("redTeamWins=") + std::string(bz_urlEncode(redTeamWins.c_str())) + 
				std::string("&greenTeamWins=") + std::string(bz_urlEncode(greenTeamWins.c_str())) + 
				std::string("&matchTime=") + std::string(bz_urlEncode(matchTime.c_str())) + 
				std::string("&matchDate=") + std::string(bz_urlEncode(match_date)) +
				std::string("&mapPlayed=") + std::string(bz_urlEncode(bz_getPublicDescription().c_str())) + 
				std::string("&redPlayers=");
				
				for (unsigned int i = 0; i < matchParticipants.size(); i++) //Go through the player list and check for red team players
				{
					if(matchParticipants.at(i).team == eRedTeam) //Check to see if the player is part of the red team
					{
						matchToSend += std::string(bz_urlEncode(matchParticipants.at(i).callsign.c_str()));
						if(i+1 < matchParticipants.size() && matchParticipants.at(i+1).team == eRedTeam) //Only add a comma if the next player on the list is red
							matchToSend += ",";
					}
				}
				
				matchToSend += std::string("&greenPlayers=");
				
				for (unsigned int i = 0; i < matchParticipants.size(); i++) //Now check for green team players...
				{
					if(matchParticipants.at(i).team == eGreenTeam) //Check to see if the player is part of the green team
					{
						matchToSend += std::string(bz_urlEncode(matchParticipants.at(i).callsign.c_str()));
						if(i+1 < matchParticipants.size() && matchParticipants.at(i+1).team == eGreenTeam) //Only add a comma if the next player on the list is green
							matchToSend += ",";
					}
				}
				
				bz_addURLJob(URL.c_str(), &myUrl, matchToSend.c_str()); //Send the match data to the league website
				
				bz_debugMessage(DEBUG,"Match Over Seer: Official match was reported.");
				bz_sendTextMessage(BZ_SERVER,BZ_ALLUSERS, "Official match was reported.");

				matchParticipants.clear();
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
					if (bz_getPlayerTeam(playerList->get(i)) == eRedTeam || bz_getPlayerTeam(playerList->get(i)) == eGreenTeam) //Check if the player is actually playing
					{
						matchPlayers matchData;
						matchData.callsign = playerTeam->callsign.c_str(); //Add callsign to struct
						matchData.team = playerTeam->team; //Add team color to struct
					
						matchParticipants.push_back(matchData);
					}
				}
			
				bz_deleteIntList(playerList);
			}
			else if(!countDownStarted) //Match was started with /countdown
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
