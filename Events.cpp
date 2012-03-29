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

void leagueOverSeer::Event(bz_EventData *eventData)
{
  switch (eventData->eventType)
  {
    case bz_eSlashCommandEvent: //Someone uses a slash command
    {
      bz_SlashCommandEventData_V1 *commandData = (bz_SlashCommandEventData_V1*)eventData;
      bz_BasePlayerRecord *playerData = bz_getPlayerByIndex(commandData->from);
      std::string command = commandData->message.c_str(); //Use std::string for quick reference
      
      if(command.compare("/gameover") == 0 && bz_hasPerm(commandData->from,"ENDGAME") && gameoverReport) //Check if they did a /gameover
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
        std::ostringstream myBTW;
        myBTW << (BTW);
        std::ostringstream myPTW;
        myPTW << (PTW);
        std::ostringstream MT;
        MT << (bz_getTimeLimit());
        
        //Keep references to values for quick reference
        std::string matchToSend = "";
        std::string redTeamWins = myRTW.str();
        std::string greenTeamWins = myGTW.str();
        std::string blueTeamWins = myBTW.str();
        std::string purpleTeamWins = myPTW.str();
        std::string matchTime = MT.str();
        
        //Create the syntax of the parameters that is going to be sent via a URL
        if (bz_getTeamPlayerLimit(eRedTeam) > 0)
          matchToSend += std::string("redTeamWins=") + std::string(bz_urlEncode(redTeamWins.c_str()));
        if (bz_getTeamPlayerLimit(eGreenTeam) > 0)
          matchToSend += std::string("&greenTeamWins=") + std::string(bz_urlEncode(greenTeamWins.c_str()));
        if (bz_getTeamPlayerLimit(eBlueTeam) > 0)
          matchToSend += std::string("&blueTeamWins=") + std::string(bz_urlEncode(blueTeamWins.c_str()));
        if (bz_getTeamPlayerLimit(ePurpleTeam) > 0)
          matchToSend += std::string("&purpleTeamWins=") + std::string(bz_urlEncode(purpleTeamWins.c_str()));
        
        matchToSend += std::string("&matchTime=") + std::string(bz_urlEncode(matchTime.c_str())) + 
        std::string("&matchDate=") + std::string(bz_urlEncode(match_date));
        
        if(rotLeague) //Only add this parameter if it's a rotational league such as OpenLeague
          matchToSend += std::string("&mapPlayed=") + std::string(bz_urlEncode(map.c_str()));
        
        if (bz_getTeamPlayerLimit(eRedTeam) > 0)
        {
          matchToSend += std::string("&redPlayers=");
          
          for (unsigned int i = 0; i < matchRedParticipants.size(); i++) //Add all the red players to the match report
          {
            matchToSend += std::string(bz_urlEncode(matchRedParticipants.at(i).callsign.c_str()));
            if (i+1 < matchRedParticipants.size()) //Only add a quote if there is another player on the list
              matchToSend += "\"";
          }
        }
        
        if (bz_getTeamPlayerLimit(eGreenTeam) > 0)
        {
          matchToSend += std::string("&greenPlayers=");
        
          for (unsigned int i = 0; i < matchGreenParticipants.size(); i++) //Now add all the green players
          {
            matchToSend += std::string(bz_urlEncode(matchGreenParticipants.at(i).callsign.c_str()));
            if (i+1 < matchGreenParticipants.size()) //Only add a quote if there is another player on the list
              matchToSend += "\"";
          }
        }
        
        if (bz_getTeamPlayerLimit(eBlueTeam) > 0)
        {
          matchToSend += std::string("&bluePlayers=");
        
          for (unsigned int i = 0; i < matchBlueParticipants.size(); i++) //Now add all the green players
          {
            matchToSend += std::string(bz_urlEncode(matchBlueParticipants.at(i).callsign.c_str()));
            if (i+1 < matchBlueParticipants.size()) //Only add a quote if there is another player on the list
              matchToSend += "\"";
          }
        }
        
        if (bz_getTeamPlayerLimit(ePurpleTeam) > 0)
        {
          matchToSend += std::string("&purplePlayers=");
        
          for (unsigned int i = 0; i < matchPurpleParticipants.size(); i++) //Now add all the green players
          {
            matchToSend += std::string(bz_urlEncode(matchPurpleParticipants.at(i).callsign.c_str()));
            if (i+1 < matchPurpleParticipants.size()) //Only add a quote if there is another player on the list
              matchToSend += "\"";
          }
        }
      
        //Match data stored in server logs
        bz_debugMessage(DEBUG,"*** Final Match Data ***");
        bz_debugMessagef(DEBUG,"Match Time Limit: %s", matchTime.c_str());
        bz_debugMessagef(DEBUG,"Match Date: %s", match_date);
        if(rotLeague)
          bz_debugMessagef(DEBUG,"Map Played: %s", map.c_str());
        if (bz_getTeamPlayerLimit(eRedTeam) > 0)
        {
          bz_debugMessagef(DEBUG, "Red Team (%s)", redTeamWins.c_str());
          for (unsigned int i = 0; i < matchRedParticipants.size(); i++)
            bz_debugMessagef(DEBUG, "  %s", matchRedParticipants.at(i).callsign.c_str());
        }
        
        if (bz_getTeamPlayerLimit(eGreenTeam) > 0)
        {
          bz_debugMessagef(DEBUG, "Green Team (%s)", greenTeamWins.c_str());
          for (unsigned int i = 0; i < matchGreenParticipants.size(); i++)
            bz_debugMessagef(DEBUG, "  %s", matchGreenParticipants.at(i).callsign.c_str());
        }
        
        if (bz_getTeamPlayerLimit(eBlueTeam) > 0)
        {
          bz_debugMessagef(DEBUG, "Blue Team (%s)", blueTeamWins.c_str());
          for (unsigned int i = 0; i < matchBlueParticipants.size(); i++)
            bz_debugMessagef(DEBUG, "  %s",matchBlueParticipants.at(i).callsign.c_str());
        }
        
        if (bz_getTeamPlayerLimit(ePurpleTeam) > 0)
        {
          bz_debugMessagef(DEBUG, "Purple Team (%s)", purpleTeamWins.c_str());
          for (unsigned int i = 0; i < matchPurpleParticipants.size(); i++)
            bz_debugMessagef(DEBUG, "  %s", matchPurpleParticipants.at(i).callsign.c_str());
        }
        
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
        matchBlueParticipants.clear();
        matchPurpleParticipants.clear();
        RTW = 0;
        GTW = 0;
        BTW = 0;
        PTW = 0;
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
        BTW = 0;
        PTW = 0;
        
        for (unsigned int i = 0; i < playerList->size(); i++){
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
          else if (bz_getPlayerTeam(playerList->get(i)) == eBlueTeam) //Check if the player is on the blue team
          {
            matchBluePlayers matchBlueData;
            matchBlueData.callsign = playerTeam->callsign.c_str(); //Add callsign to structure

            matchBlueParticipants.push_back(matchBlueData);
          }
          else if (bz_getPlayerTeam(playerList->get(i)) == ePurpleTeam) //Check if the player is on the purple team
          {
            matchPurplePlayers matchPurpleData;
            matchPurpleData.callsign = playerTeam->callsign.c_str(); //Add callsign to structure

            matchPurpleParticipants.push_back(matchPurpleData);
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
        if (BTW > 0) BTW = 0;
        if (PTW > 0) PTW = 0;
        
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
        {
          GTW++;
          BTW++;
          PTW++;
        }
        else if(capData->teamCapped == eGreenTeam) //Red team caps
        {
          RTW++;
          BTW++;
          PTW++;
        }
        else if(capData->teamCapped == eBlueTeam) //Red team caps
        {
          RTW++;
          GTW++;
          PTW++;
        }
        else if(capData->teamCapped == ePurpleTeam) //Red team caps
        {
          RTW++;
          GTW++;
          BTW++;
        }
      }
    }
    break;

    case bz_eAllowPlayer:
    {
      bz_AllowPlayerEventData_V1 *allowData = (bz_AllowPlayerEventData_V1*)eventData; 
      GameKeeper::Player *playerData = GameKeeper::Player::getPlayerByIndex(allowData->playerID);
      bz_BasePlayerRecord *record = bz_getPlayerByIndex(allowData->playerID);

      std::string mottoBefore = playerData->player.getMotto();
      const char* newMotto = getGuTeamFromBzId(playerData->getBzIdentifier());
      bz_debugMessagef(4, "Player Joined: MottoFilter: BzId value == %s", playerData->getBzIdentifier().c_str());

      playerData->player.PlayerInfo::setMotto(newMotto);
      std::string mottoAfter = playerData->player.getMotto();
      bz_debugMessagef(4, "Player Joined: MottoFilter: Replaced Motto %s with %s", mottoBefore.c_str(), mottoAfter.c_str());

      bz_freePlayerRecord(record);
    }
    default:break; //I never really understand the point of this... -.-"
  }
}

