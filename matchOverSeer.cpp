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

License:
BSD

Version:
0.9.1 [Codename: Baby Monkey]
*/

#include <stdio.h>
#include "bzfsAPI.h"
#include "plugin_utils.h"

class matchOverSeer : public bz_Plugin, public bz_CustomSlashCommandHandler
{
	virtual const char* Name (){return "Match Over Seer 0.9.1 (25)";}
	virtual void Init ( const char* config);	
	virtual void Event( bz_EventData *eventData );
	virtual bool SlashCommand( int playerID, bz_ApiString, bz_ApiString, bz_APIStringList*);
	virtual void Cleanup ();
	
	bool officialMatch, matchCanceled;
	
	struct matchPlayers { //Maintains the players that started the match and their team color
			bz_ApiString callsign;
			bz_eTeamType team;
	};
	std::vector<matchPlayers> matchParticipants;
};

//Define plugin version numbering
const int MAJOR = 0;
const int MINOR = 9;
const int REV = 1;
const int BUILD = 25;

BZ_PLUGIN(matchOverSeer);

void matchOverSeer::Init ( const char* /*commandLine*/ )
{
	bz_debugMessagef(0, "Match Over Seer %i.%i.%i (%i) loaded.", MAJOR, MINOR, REV, BUILD);
	
	Register(bz_eGameEndEvent);
	Register(bz_eGameStartEvent);
	Register(bz_eSlashCommandEvent);
	Register(bz_ePlayerJoinEvent);

	bz_registerCustomSlashCommand ("match", this);
}

void matchOverSeer::Cleanup (void)
{
	bz_debugMessagef(0, "Match Over Seer %i.%i.%i (%i) unloaded.", MAJOR, MINOR, REV, BUILD);
	
	Flush();
	
	bz_removeCustomSlashCommand ("match");
}

void matchOverSeer::Event(bz_EventData *eventData)
{
	switch (eventData->eventType)
	{
		case bz_eSlashCommandEvent: //Some users a slash command
		{
			bz_SlashCommandEventData_V1 *commandData = (bz_SlashCommandEventData_V1*)eventData;
			bz_BasePlayerRecord *playerData = bz_getPlayerByIndex(commandData->from);
			std::string command = commandData->message.c_str();
			
			if(command.compare("/gameover") == 0 || command.compare("/superkill") == 0) //Check if they did a /gameover or /superkill
			{
				bz_debugMessagef(2,"Match Over Seer: Offical match canceled by %s (%s)",playerData->callsign.c_str(),playerData->ipAddress.c_str());
				bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS, "Offical match canceled by %s",playerData->callsign.c_str());
				matchCanceled = true; //To prevent reporting a canceled match, let plugin know the match was canceled
			}
		}
		break;
		
		case bz_eGameEndEvent:
		{
			officialMatch = false; //Match is over
			time_t t = time(NULL); //Get the current time
			tm * now = gmtime(&t);
			char match_date[20];
			
			sprintf(match_date, "%02d-%02d-%02d %02d:%02d:%02d", now->tm_year+1900, now->tm_mon+1, now->tm_mday, now->tm_hour, now->tm_min, now->tm_sec);
			
			if(matchCanceled) //The match was canceled via /gameover or /superkill
			{
				matchCanceled = false; //Reset the variable for next usage
				bz_debugMessage(2,"Match Over Seer: Offical match was not reported.");
				bz_sendTextMessage(BZ_SERVER,BZ_ALLUSERS, "Offical match was not reported.");
			}
			else
			{
				bz_debugMessage(2,"Match Over Seer: Offical match was reported.");
				bz_sendTextMessage(BZ_SERVER,BZ_ALLUSERS, "Offical match was reported.");
				
				std::string URL = "http://localhost/auto_report.php";
				
				/*Start Debug information...
				//To be removed once the URL job is added
				bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS, "Red Team Score: %i",bz_getTeamWins(eRedTeam));
				bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS, "Green Team Score: %i",bz_getTeamWins(eGreenTeam));
				bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS, "Match Time Limit: %f", bz_getTimeLimit());
				bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS, "Date: %s", match_date);
				bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS, "Map Played: %s", bz_getPublicDescription().c_str());
				
				bz_sendTextMessage(BZ_SERVER,BZ_ALLUSERS, "Red Team:");
				for (unsigned int i = 0; i < matchParticipants.size(); i++)
				{
					if(matchParticipants.at(i).team == eRedTeam)
						bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS,"%s",matchParticipants.at(i).callsign.c_str());
				}
				
				bz_sendTextMessage(BZ_SERVER,BZ_ALLUSERS, "Green Team:");
				for (unsigned int i = 0; i < matchParticipants.size(); i++)
				{
					if(matchParticipants.at(i).team == eGreenTeam)
						bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS,"%s",matchParticipants.at(i).callsign.c_str());
				}
				//End Debug information
				*/
				
				std::string matchToSend = "";
				
				matchToSend = std::string("redTeam=") + std::string(bz_urlEncode((const char*)bz_getTeamWins(eRedTeam))) + 
				std::string("&greenTeam=") + std::string(bz_urlEncode((const char*)bz_getTeamWins(eGreenTeam))) + 
				std::string("&matchTime=") + std::string(bz_urlEncode((const char*)int(bz_getTimeLimit()))) + 
				std::string("&matchDate=") + std::string(bz_urlEncode(match_date)) +
				std::string("&mapPlayed=") + std::string(bz_urlEncode(bz_getPublicDescription().c_str())) + 
				std::string("&redPlayers=");
				
				for (unsigned int i = 0; i < matchParticipants.size(); i++)
				{
					if(matchParticipants.at(i).team == eRedTeam)
					{
						matchToSend += std::string(bz_urlEncode(matchParticipants.at(i).callsign.c_str()));
						if(i+1 < matchParticipants.size())
							matchToSend += ",";
					}
				}
				
				matchToSend += std::string("&greenPlayers=");
				
				for (unsigned int i = 0; i < matchParticipants.size(); i++)
				{
					if(matchParticipants.at(i).team == eGreenTeam)
					{
						matchToSend += std::string(bz_urlEncode(matchParticipants.at(i).callsign.c_str()));
						if(i+1 < matchParticipants.size())
							matchToSend += ",";
					}
				}
				
				bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS,"%s",matchToSend.c_str());
				//bz_addURLJob(URL.c_str(), NULL, matchToSend.c_str());
			}
		}
		break;
		
		case bz_eGameStartEvent: //The countdown is started
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
						matchData.callsign = playerTeam->callsign.c_str();
						matchData.team = playerTeam->team;
					
						matchParticipants.push_back(matchData);
					}
				}
			
				bz_deleteIntList(playerList);
			}
		}
		break;

		case bz_ePlayerJoinEvent:
		{
			bz_PlayerJoinPartEventData_V1 *joinData = (bz_PlayerJoinPartEventData_V1*)eventData;
			
			if(bz_isCountDownActive()) //If there is a match in progress, notify others who join
				bz_sendTextMessage(BZ_SERVER,joinData->playerID, "There is currently an official match in progress, please be respectful.");
		}
		break;
		
		default:break;
	}
}

bool matchOverSeer::SlashCommand(int playerID, bz_ApiString command, bz_ApiString /*message*/, bz_APIStringList *params)
{
	bz_BasePlayerRecord *playerData = bz_getPlayerByIndex(playerID);
	
	if(command == "match") //Someone used the /match command
	{
		if(playerData->verified && playerData->team != eObservers && bz_hasPerm(playerID,"spawn")) //Check the user is not an obs and is a league member
		{
			officialMatch = true; //Notify the plugin that the match is official
			bz_debugMessagef(2,"Match Over Seer: Offical match started by %s (%s).",playerData->callsign.c_str(),playerData->ipAddress.c_str());
			bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS, "Offical match started by %s.",playerData->callsign.c_str());
			bz_startCountdown (10, bz_getTimeLimit(), "Open League Overlord"); //Start the countdown for the official match
		}
		else if(playerData->team == eObservers) //Observers can't start matches... Duh
			bz_sendTextMessage(BZ_SERVER,playerID,"Observers are not allowed to start matches.");
		else if(!playerData->verified || !bz_hasPerm(playerID,"spawn")) //People who can't spawn can't start matches either... Derp!
			bz_sendTextMessage(BZ_SERVER,playerID,"Only registered OL players may start an official match.");
	}
	
	bz_freePlayerRecord(playerData);	
}
