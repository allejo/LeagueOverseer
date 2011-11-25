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
0.9 [Codename: Baby Monkey]
*/

#include <stdio.h>
#include "bzfsAPI.h"
#include "plugin_utils.h"

class matchOverSeer : public bz_Plugin, public bz_CustomSlashCommandHandler
{
	virtual const char* Name (){return "Match Over Seer 0.9.0 (20)";}
	virtual void Init ( const char* config);	
	virtual void Event( bz_EventData *eventData );
	virtual bool SlashCommand( int playerID, bz_ApiString, bz_ApiString, bz_APIStringList*);
	virtual void Cleanup ();
	
	bool officialMatch, matchCanceled;
	
	struct matchPlayers {
			bz_ApiString callsign;
			bz_eTeamType team;
	};
	std::vector<matchPlayers> matchParticipants;
};

//Define plugin version numbering
const int MAJOR = 0;
const int MINOR = 9;
const int REV = 0;
const int BUILD = 20;

BZ_PLUGIN(matchOverSeer)

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
		case bz_eSlashCommandEvent:
		{
			bz_SlashCommandEventData_V1 *commandData = (bz_SlashCommandEventData_V1*)eventData;
			bz_BasePlayerRecord *playerData = bz_getPlayerByIndex(commandData->from);
			std::string command = commandData->message.c_str();
			
			if(command.compare("/gameover") == 0 || command.compare("/superkill") == 0)
			{
				bz_debugMessagef(2,"Match Over Seer: Offical match canceled by %s (%s)",playerData->callsign.c_str(),playerData->ipAddress.c_str());
				bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS, "Offical match canceled by %s",playerData->callsign.c_str());
				matchCanceled = true;
			}
		}
		break;
		
		case bz_eGameEndEvent:
		{
			officialMatch = false;
			time_t t = time(NULL);
			tm * now = gmtime(&t);
			char match_date[20];
			
			sprintf(match_date, "%02d-%02d-%02d %02d:%02d:%02d", now->tm_year+1900, now->tm_mon+1, now->tm_mday, now->tm_hour, now->tm_min, now->tm_sec);
			
			if(matchCanceled)
			{
				matchCanceled = false;
				bz_debugMessage(2,"Match Over Seer: Offical match was not reported.");
				bz_sendTextMessage(BZ_SERVER,BZ_ALLUSERS, "Offical match was not reported.");
			}
			else
			{
				bz_debugMessage(2,"Match Over Seer: Offical match was reported.");
				bz_sendTextMessage(BZ_SERVER,BZ_ALLUSERS, "Offical match was reported.");
				
				bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS, "Red Team Score: %i",bz_getTeamWins(eRedTeam));
				bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS, "Green Team Score: %i",bz_getTeamWins(eGreenTeam));
				bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS, "Match Time Limit: %i", bz_getTimeLimit());
				bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS, "Date: %s", match_date);
				
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
				
				//TODO: report the match
			}
		}
		break;
		
		case bz_eGameStartEvent:
		{
			bz_GameStartEndEventData_V1 *startData = (bz_GameStartEndEventData_V1*)eventData;
			
			bz_APIIntList *playerList = bz_newIntList();
			bz_getPlayerIndexList(playerList);
			
			for ( unsigned int i = 0; i < playerList->size(); i++ ){
				bz_BasePlayerRecord *playerTeam = bz_getPlayerByIndex(playerList->get(i));
				if (bz_getPlayerTeam(playerList->get(i)) == eRedTeam || bz_getPlayerTeam(playerList->get(i)) == eGreenTeam){
					matchPlayers matchData;
					matchData.callsign = playerTeam->callsign.c_str();
					matchData.team = playerTeam->team;
					
					matchParticipants.push_back(matchData);
				}
			}
			
			bz_deleteIntList(playerList);
		}
		break;

		case bz_ePlayerJoinEvent:
		{
			bz_PlayerJoinPartEventData_V1 *joinData = (bz_PlayerJoinPartEventData_V1*)eventData;
			
			if(bz_isCountDownActive())
				bz_sendTextMessage(BZ_SERVER,joinData->playerID, "There is currently an official match in progress, please be respectful.");
		}
		break;
		
		default:break;
	}
}

bool matchOverSeer::SlashCommand(int playerID, bz_ApiString command, bz_ApiString /*message*/, bz_APIStringList *params)
{
	bz_BasePlayerRecord *playerData = bz_getPlayerByIndex(playerID);
	
	if(command == "match")
	{
		if(playerData->verified && playerData->team != eObservers && bz_hasPerm(playerID,"spawn"))
		{
			officialMatch = true;
			bz_debugMessagef(2,"Match Over Seer: Offical match started by %s (%s).",playerData->callsign.c_str(),playerData->ipAddress.c_str());
			bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS, "Offical match started by %s.",playerData->callsign.c_str());
			bz_startCountdown (10, bz_getTimeLimit(), "Open League Overlord");
		}
		else if(playerData->team == eObservers)
			bz_sendTextMessage(BZ_SERVER,playerID,"Observers are not allowed to start matches.");
		else if(!playerData->verified || !bz_hasPerm(playerID,"spawn"))
			bz_sendTextMessage(BZ_SERVER,playerID,"Only registered OL players may start an official match.");
	}
	
	bz_freePlayerRecord(playerData);	
}
