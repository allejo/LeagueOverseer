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
0.0.1 [Codename: Baby Monkey]
*/

#include <stdio.h>
#include "bzfsAPI.h"
#include "plugin_utils.h"

class matchOverSeer : public bz_Plugin, public bz_CustomSlashCommandHandler
{
	virtual const char* Name (){return "Match Over Seer";}
	virtual void Init ( const char* config);	
	virtual void Event( bz_EventData *eventData );
	virtual bool SlashCommand( int playerID, bz_ApiString, bz_ApiString, bz_APIStringList*);
	virtual void Cleanup ();
	
	int greenScore, redScore;
	bool officialMatch, matchCanceled;
	
	std::string redTeam[20];
	std::string greenTeam[20];
};

BZ_PLUGIN(matchOverSeer)

void matchOverSeer::Init ( const char* /*commandLine*/ )
{
	bz_debugMessage(2,"Match Over Seer: Plugin successfully loaded.");
	
	Register(bz_eCaptureEvent);
	Register(bz_eGameEndEvent);
	Register(bz_eGameStartEvent);
	Register(bz_ePlayerJoinEvent);

	bz_registerCustomSlashCommand ("match", this);
}

void matchOverSeer::Cleanup (void)
{
	bz_debugMessage(2,"Match Over Seer: Plugin successfully unloaded.");
	
	Flush();
	
	bz_removeCustomSlashCommand ("match");
}

void matchOverSeer::Event(bz_EventData *eventData)
{
	switch (eventData->eventType)
	{
		case bz_eCaptureEvent:
    	{
			bz_CTFCaptureEventData_V1 *captureData = (bz_CTFCaptureEventData_V1*)eventData;
			
			if(officialMatch && bz_isCountDownActive())
			{
				if(captureData->teamCapped == ePurpleTeam)
					greenScore++;
				if(captureData->teamCapped == eRedTeam)
					redScore++;
			}
		}
		break;
		
		case bz_eGameEndEvent:
		{
			officialMatch = false;
			greenScore=0;
			redScore=0;
			memset(redTeam, 0, 20);
			memset(greenTeam, 0, 20);
			time_t t = time(NULL);
			tm * now = gmtime(&t);
			char match_date[20];
			
			sprintf(match_date, "%02d-%02d-%02d %02d:%02d:%02d", now->tm_year+1900, now->tm_mon+1, now->tm_mday, now->tm_hour, now->tm_min, now->tm_sec);
			
			if(matchCanceled)
			{
				matchCanceled = false;
				bz_debugMessage(2,"Match Over Seer: Offical match was not reported.");
				bz_sendTextMessage(BZ_SERVER,BZ_ALLUSERS, "Offical match was not reported.");
				
				bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS, "Red Team Score: %i",redScore);
				bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS, "Green Team Score: %i",greenScore);
				bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS, "Match Time Limit: %i", bz_getTimeLimit());
				bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS, "Red Team: %s",redTeam);
				bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS, "Green Team: %s",greenTeam);
				bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS, "Date: %s", match_date);
			}
			else
			{
				bz_debugMessage(2,"Match Over Seer: Offical match was reported.");
				bz_sendTextMessage(BZ_SERVER,BZ_ALLUSERS, "Offical match was reported.");
				//TODO: report the match
			}
		}
		break;
		
		case bz_eGameStartEvent:
		{
			bz_GameStartEndEventData_V1 *startData = (bz_GameStartEndEventData_V1*)eventData;
			
			bz_APIIntList *playerList = bz_newIntList();
			bz_getPlayerIndexList(playerList);
			
			int rt,gt=0;
			redScore, greenScore = 0;

			for ( unsigned int i = 0; i < playerList->size(); i++ ){
				bz_BasePlayerRecord *teamMember = bz_getPlayerByIndex(playerList->get(i));
				if (teamMember->team == eObservers){
					if(teamMember->team == eRedTeam)
					{
						redTeam[rt]=teamMember->callsign.c_str();
						rt++;
					}
					else if(teamMember->team == eGreenTeam)
					{
						greenTeam[gt]=teamMember->callsign.c_str();
						gt++;
					}
				}
			}
			
			bz_deleteIntList(playerList);
		}
		break;
		
		case bz_eSlashCommandEvent:
		{
			bz_SlashCommandEventData_V1 *commandData = (bz_SlashCommandEventData_V1*)eventData;
			bz_BasePlayerRecord *playerData = bz_getPlayerByIndex(commandData->from);
			
			if(commandData->message.c_str() == "gameover" || commandData->message.c_str() == "superkill")
				bz_debugMessagef(2,"Match Over Seer: Offical match canceled by %s (%s)",playerData->callsign.c_str(),playerData->ipAddress.c_str());
				bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS, "Offical match canceled by %s",playerData->callsign.c_str());
				matchCanceled = true;
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
			bz_startCountdown (10, bz_getTimeLimit(), "SERVER");
		}
		else if(playerData->team == eObservers)
			bz_sendTextMessage(BZ_SERVER,playerID,"Observers are not allowed to start matches.");
		else if(!playerData->verified || !bz_hasPerm(playerID,"spawn"))
			bz_sendTextMessage(BZ_SERVER,playerID,"Only registered OL players may start an official match.");
	}
	
	bz_freePlayerRecord(playerData);	
}
