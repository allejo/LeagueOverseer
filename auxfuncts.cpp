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

#include "leagueOverSeer.h"

int leagueOverSeer::loadConfig(const char* cmdLine) //Load the plugin configuration file
{
  PluginConfig config = PluginConfig(cmdLine);
  std::string section = "leagueOverSeer";
  
  if (config.errors) bz_shutdown(); //Shutdown the server
  
  //Extract all the data in the configuration file and assign it to plugin variables
  LEAGUE = config.item(section, "LEAGUE");
  rotLeague = toBool(config.item(section, "ROTATIONAL_LEAGUE"));
  mapchangePath = (config.item(section, "MAPCHANGE_PATH")).c_str();
  gameoverReport = toBool(config.item(section, "GAMEOVER_REPORT"));
  LEAGUE_URL = config.item(section, "MATCH_REPORT_URL");
  gracePeriod = atoi((config.item(section, "GRACE_PERIOD")).c_str());
  DEBUG = atoi((config.item(section, "DEBUG_LEVEL")).c_str());
  mottoReplacer = toBool(config.item(section, "MOTTOFILTER_REPLACER"));
  rejoinPrevention = toBool(config.item(section, "REJOIN_PREVENTION"));
  
  //Check for errors in the configuration data. If there is an error, shut down the server
  if (strcmp(LEAGUE, "") == 0)
  {
    bz_debugMessage(0, "*** DEBUG::Match Over Seer::No league was specified ***");
    bz_shutdown();
  }
  if (strcmp(LEAGUE_URL, "") == 0)
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

const char* leagueOverSeer::getGuTeamFromBzId(std::string bzid)
{
  //get team name from GU website.
  const char* playerTeam = "allejo needs to do this:p";
  return playerTeam;
}
bool leagueOverSeer::toBool(std::string var) //Turn std::string into a boolean value
{
   if(var == "true" || var == "TRUE" || var == "1") //If the value is true then the boolean is true
      return true;
   else //If it's not true, then it's false.
      return false;
}
