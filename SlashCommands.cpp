/*
League Over Seer Plug-in
    Copyright (C) 2012 Ned Anderson & Vladimir Jimenez

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

#include "bzfsAPI.h"
//#include "plugin_utils.h"
#include "../../src/bzfs/GameKeeper.h"
#include "leagueOverSeer.h"

bool leagueOverSeer::SlashCommand(int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList *params)
{
  int timeToStart = atoi(params->get(0).c_str());
  bz_BasePlayerRecord *playerData = bz_getPlayerByIndex(playerID);
  
  if(command == "official") //Someone used the /official command
  {  
    if(playerData->team == eObservers) //Observers can't start matches
      bz_sendTextMessage(BZ_SERVER,playerID,"Observers are not allowed to start matches.");
    else if(bz_getTeamCount(eRedTeam) < 2 || bz_getTeamCount(eGreenTeam) < 2 || bz_getTeamCount(eBlueTeam) < 2 || bz_getTeamCount(ePurpleTeam) < 2) //An official match cannot be 1v1 or 2v1
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
  else if(command == "cancel")
  {
    if(bz_hasPerm(playerID,"spawn") && (bz_isCountDownActive() || countDownStarted))
    {
      bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS,"Match ended by %s",playerData->callsign.c_str());
      bz_debugMessagef(DEBUG,"DEBUG::Match Over Seer::Match ended by %s (%s).",playerData->callsign.c_str(),playerData->ipAddress.c_str());
      
      //Reset the server. Cleanly ends a match
      if (officialMatch) officialMatch = false;
      if (matchCanceled) matchCanceled = false;
      if (countDownStarted) countDownStarted = false;
      if (funMatch) funMatch = false;
      if (RTW > 0) RTW = 0;
      if (GTW > 0) GTW = 0;
      if (BTW > 0) BTW = 0;
      if (PTW > 0) PTW = 0;
    
      //End the countdown
      if(bz_isCountDownActive())
        bz_gameOver(253,eObservers);
    }
    else if(!bz_isCountDownActive() || !countDownStarted)
      bz_sendTextMessage(BZ_SERVER,playerID,"There is no match in progress to cancel.");
    else //Not a league player
      bz_sendTextMessage(BZ_SERVER,playerID,"You do not have permission to run the /cancel command.");
  }
  else if(command == "spawn") 
  {
    if(playerData->admin) {
      if(params->size() == 1) {
        const char* msg = message.c_str() + 6;
        while ((*msg != '\0') && isspace(*msg)) msg++;

        if(isdigit(msg[0])) {
          int grantee = (int) atoi(params->get(0).c_str());
	  const char* granterCallsign = bz_getPlayerCallsign(playerID);
	  const char* granteeCallsign = bz_getPlayerCallsign(grantee);
                 
          bz_grantPerm(grantee, "spawn");
          bz_sendTextMessagef(BZ_SERVER, eAdministrators, "%s Gave spawn perms to %s", granterCallsign, granteeCallsign); 
        } else if(!isdigit(msg[0])) {
          int grantee = GameKeeper::Player::getPlayerIDByName(params->get(0).c_str());
	  const char* granterCallsign = bz_getPlayerCallsign(playerID);
	  const char* granteeCallsign = bz_getPlayerCallsign(grantee);

          bz_grantPerm(grantee, "spawn");
	  bz_sendTextMessagef(BZ_SERVER, eAdministrators, "%s Gave spawn perms to %s", granterCallsign, granteeCallsign); 
        }
      }
    }
    else if(!playerData->admin) {
      bz_sendTextMessage(BZ_SERVER,playerID,"You do not have permission to use that command.");
    }
  }
  bz_freePlayerRecord(playerData);  
}
