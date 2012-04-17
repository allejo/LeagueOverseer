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
#include "plugin_utils.h"
#include "../../src/bzfs/GameKeeper.h"

#include "leagueOverSeer.h"

void leagueOverSeer::URLDone( const char* URL, void* data, unsigned int size, bool complete ) //Everything went fine with the report
{
  std::string siteData = (char*)(data); //Convert the data to a std::string

  if(_urlQuery.at(0)._URL.compare("query") == 0 && URL == LEAGUE_URL) //Someone queried for teams
  {  
    char* token; //Store tokens
    
    GameKeeper::Player *playerData = GameKeeper::Player::getPlayerByIndex(_playerIDs.at(0)._playerID);
    playerData->player.PlayerInfo::setMotto(token);

    _urlQuery.erase(_urlQuery.begin(),_urlQuery.begin()+1); //Tell the plugin that the the match query has been delt with, move to the next url job
	 _playerIDs.erase(_playerIDs.begin(),_playerIDs.begin()+1); //Tell the plugin that this player has received his/her information, move to the next player
  }
  else if(_urlQuery.at(0)._URL.compare("match") == 0 && URL == LEAGUE_URL) //The plugin reported the match successfully
  {
    bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS,"%s",(char*)data);
    bz_debugMessagef(DEBUG,"%s",(char*)data);

    _urlQuery.erase(_urlQuery.begin(),_urlQuery.begin()+1); //Tell the plugin that the the match query has been delt with, move to the next url job
  }
}

void leagueOverSeer::URLTimeout(const char* URL, int errorCode) //The league website is down or is not responding, the request timed out
{
  if(_urlQuery.at(0)._URL.compare("match") == 0 && URL == LEAGUE_URL) //Something went wrong while reporting the match, it timed out
  {
    bz_sendTextMessage(BZ_SERVER,BZ_ALLUSERS,">-- WARNING --<");
    bz_sendTextMessage(BZ_SERVER,BZ_ALLUSERS,">-- The request to report the match has timed out. --<");
    bz_sendTextMessage(BZ_SERVER,BZ_ALLUSERS,">-- Please contact a league admin or referee with the match result. --<");
    bz_debugMessage(DEBUG,"DEBUG::Match Over Seer::The request to report the match has timed out.");

    _urlQuery.erase(_urlQuery.begin(),_urlQuery.begin()+1); //Tell the plugin that the the match query has been delt with, move to the next url job
  }
  else if(_urlQuery.at(0)._URL.compare("query") == 0 && URL == LEAGUE_URL) //Something went wrong with the team query, it timed out
  {
    bz_sendTextMessage(BZ_SERVER,_playerIDs.at(0)._playerID, "The request to query the league website has timed out. Please try again later.");

    _urlQuery.erase(_urlQuery.begin(),_urlQuery.begin()+1); //Tell the plugin that the the match query has been delt with, move to the next url job
    _playerIDs.erase(_playerIDs.begin(),_playerIDs.begin()+1); //Tell the plugin that this player has received his/her information, move to the next player
  }
}

void leagueOverSeer::URLError(const char* URL, int errorCode, const char *errorString) //The server owner must have set up the URLs wrong because this shouldn't happen
{
  if(_urlQuery.at(0)._URL.compare("match") == 0 && URL == LEAGUE_URL) //Something went wrong while reporting the match, no website found
  {
    bz_sendTextMessage(BZ_SERVER,BZ_ALLUSERS,">-- WARNING --<");
    bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS,">-- Match report failed with error code %i - %s --<",errorCode,errorString);
    bz_sendTextMessage(BZ_SERVER,BZ_ALLUSERS,">-- Please contact a league admin or referee with this error and the match result. --<");
    bz_debugMessage(DEBUG,"DEBUG::Match Over Seer::Match report failed with the following error:");
    bz_debugMessagef(DEBUG,"DEBUG::Match Over Seer::Error code: %i - %s",errorCode,errorString);

    _urlQuery.erase(_urlQuery.begin(),_urlQuery.begin()+1); //Tell the plugin that the the match query has been delt with, move to the next url job
  }
  else if(_urlQuery.at(0)._URL.compare("query") == 0 && URL == LEAGUE_URL) //Something went wrong with the team query, no website found
  {
    bz_sendTextMessagef(BZ_SERVER,_playerIDs.at(0)._playerID, "Your team query failed with error code %i - %s",errorCode,errorString);
    bz_sendTextMessagef(BZ_SERVER,_playerIDs.at(0)._playerID, "Please contact a league admin with this error as this should not happen.");

    _urlQuery.erase(_urlQuery.begin(),_urlQuery.begin()+1); //Tell the plugin that the the match query has been delt with, move to the next url job
    _playerIDs.erase(_playerIDs.begin(),_playerIDs.begin()+1); //Tell the plugin that this player has received his/her information, move to the next player
  }
}
