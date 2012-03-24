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

void leagueOverSeer::Init ( const char* commandLine )
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
  bz_registerCustomSlashCommand("cancel",this);
  bz_registerCustomSlashCommand("spawn",this);
  
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