/*
League Overseer
    Copyright (C) 2013-2015 Vladimir Jimenez & Ned Anderson

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

#include <fstream>
#include <sstream>

#include "LeagueOverseer.h"
#include "LeagueOverseer-Helpers.h"
#include "LeagueOverseer-Version.h"
#include "UrlQuery.h"

BZ_PLUGIN(LeagueOverseer)

const char* LeagueOverseer::Name (void)
{
    static std::string pluginBuild = "";

    if (!pluginBuild.size())
    {
        std::ostringstream pluginBuildStream;

        pluginBuildStream << PLUGIN_NAME << " " << MAJOR << "." << MINOR << "." << REV << " (" << BUILD << ")";
        pluginBuild = pluginBuildStream.str();
    }

    return pluginBuild.c_str();
}

void LeagueOverseer::Init (const char* commandLine)
{
    // Register our events with Register()
    Register(bz_eAllowFlagGrab);
    Register(bz_eAllowSpawn);
    Register(bz_eBZDBChange);
    Register(bz_eCaptureEvent);
    Register(bz_eGameEndEvent);
    Register(bz_eGamePauseEvent);
    Register(bz_eGameResumeEvent);
    Register(bz_eGameStartEvent);
    Register(bz_eGetAutoTeamEvent);
    Register(bz_eGetPlayerMotto);
    Register(bz_ePlayerDieEvent);
    Register(bz_ePlayerJoinEvent);
    Register(bz_ePlayerPartEvent);
    Register(bz_eRawChatMessageEvent);
    Register(bz_eTickEvent);

    // Add all of the support slash commands so we can easily remove them in the Cleanup() function
    SLASH_COMMANDS = {"ban", "cancel", "countdown", "f", "finish", "fm", "gameover", "kick", "leagueoverseer", "los", "mute", "o", "offi", "official", "p", "pause", "poll", "r", "resume", "showhidden", "spawn", "s", "stats", "timelimit", "unmute"};
    BZDB_VARS = {"_pcProtectionEnabled", "_pcProtectionDelay", "_defaultTimeLimit"};

    // Register our custom slash commands
    for (auto command : SLASH_COMMANDS)
    {
        bz_registerCustomSlashCommand(command.c_str(), this);
    }

    // Set some default values
    currentMatch = Match();

    // Load the configuration data when the plugin is loaded
    CONFIG_PATH = commandLine;
    pluginSettings.loadConfig(CONFIG_PATH.c_str());

    // Check to see if the plugin is for a rotational league
    if (pluginSettings.isRotationalLeague())
    {
        if (pluginSettings.getMapChangePath() != "")
        {
            // Open the mapchange.out file to see what map is being used
            std::ifstream infile;
            infile.open(pluginSettings.getMapChangePath().c_str());
            getline(infile, MAP_NAME);
            infile.close();

            // Remove the '.conf' from the mapchange.out file
            MAP_NAME = MAP_NAME.substr(0, MAP_NAME.length() - 5);

            logMessage(pluginSettings.getDebugLevel(), "debug", "Current map being played: %s", MAP_NAME.c_str());
        }
        else
        {
            logMessage(0, "error", "Server marked as a rotational league but the location to the mapchange.out file was not specified.");
        }
    }

    // Assign our two team colors to eNoTeam simply so we have something to check for
    // when we are trying to find the two colors the map is using
    TEAM_ONE = eNoTeam;
    TEAM_TWO = eNoTeam;

    // Loop through all the team colors
    for (bz_eTeamType t = eRedTeam; t <= ePurpleTeam; t = (bz_eTeamType) (t + 1))
    {
        // If the current team's player limit is more than 0, that means that we found a
        // team color that the map is using
        if (bz_getTeamPlayerLimit(t) > 0)
        {
            // If team one is eNoTeam, then that means this is just the first team with player limit
            // that we have found. If it's not eNoTeam, that means we've found the second team
            if (TEAM_ONE == eNoTeam)
            {
                TEAM_ONE = t;
            }
            else if (TEAM_TWO == eNoTeam)
            {
                TEAM_TWO = t;
                break; // After we've found the second team, there's no need to continue so break out of here
            }
        }
    }

    // Make sure both teams were found, if they weren't then notify in the logs
    if (TEAM_ONE == eNoTeam || TEAM_TWO == eNoTeam)
    {
        logMessage(0, "error", "Team colors could not be detected in LeagueOverseer::Init()");
    }

    // Set up our UrlQuery objects
    TeamUrlRepo  = UrlQuery(this, pluginSettings.getTeamNameURL().c_str());
    MatchUrlRepo = UrlQuery(this, pluginSettings.getMatchReportURL().c_str());

    // Request the team name database
    if (pluginSettings.isMottoFetchEnabled())
    {
        logMessage(pluginSettings.getVerboseLevel(), "debug", "Requesting league roster...");
        TeamUrlRepo.set("query", "teamNameDump").submit();
    }

    // Create a new BZDB variable to easily set the amount of seconds team flags are protected after captures
    registerCustomBoolBZDB("_pcProtectionEnabled", false);
    registerCustomIntBZDB("_pcProtectionDelay", 5);
    registerCustomIntBZDB("_defaultTimeLimit", 1800);

    if (bz_getBZDBBool("_pcProtectionEnabled") && bz_getBZDBInt("_pcProtectionDelay") < 3 && bz_getBZDBInt("_pcProtectionDelay") > 15)
    {
        logMessage(0, "warning", "Invalid value for the PC protection delay. Default value used: %d", 5);
    }

    // Set a clip field with the full name of the plug-in for other plug-ins to know the exact name of the plug-in
    // since this plug-in has a versioning system; i.e. League Overseer X.Y.Z (r)
    bz_setclipFieldString("LeagueOverseer", Name());

    // We'll be ignoring the '-time' option as of 1.2.0 so we'll be relying entirely on the _defaultTimeLimit BZDB variable

    if (bz_getTimeLimit() > 0.0)
    {
        logMessage(0, "warning", "As of League Overseer 1.2.0, the '-time' option is no longer used and is ignored in favor of using");
        logMessage(0, "warning", "the _defaultTimeLimit BZDB variable. Default value used: %d minutes", (getDefaultTimeLimit() / 60));
    }

    bz_setTimeLimit(getDefaultTimeLimit());


    ///
    /// Check for plug-ins that we do not play nicely with
    ///
    bz_APIStringList pluginList;
    int pluginCount = bz_getLoadedPlugins(&pluginList);
    std::vector<std::string> pluginVictims = {"Record Match", "Time Limit"};

    for (int i = 0; i < pluginCount; i++)
    {
        std::string currentPlugin = pluginList.get(i).c_str();

        for (auto plugin : pluginVictims)
        {
            if (plugin == currentPlugin)
            {
                logMessage(0, "error", "The '%s' plug-in was detected to be loaded. League Overseer replaces the functionality of this plug-in.");
                logMessage(0, "error", "Please unload that plug-in and allow League Overseer to handle the functionality.");
            }
        }
    }


    ///
    /// Check Server Configuration
    ///

    if (!bz_isTimeManualStart())
    {
        logMessage(0, "error", "BZFS does not have -timemanual enabled. This plug-in cannot function without the server");
        logMessage(0, "error", "configured to have a manual countdowns.");
    }


    ///
    /// Callback testing and examples
    ///

    // Check if the 'LeagueOverseer' clipfield exists which means the League Overseer plug-in is loaded
    if (pluginSettings.isInterPluginCheckEnabled() && bz_clipFieldExists("LeagueOverseer"))
    {
        // We have found the clip field so we can access the plug-in's callbacks
        bz_debugMessage(0, "'LeagueOverseer' clip field found meaning the League Overseer plug-in is loaded.");

        // Get the name from the clip field and save it for easy access
        std::string leagueOverseerPluginName = bz_getclipFieldString("LeagueOverseer");


        ///
        /// Get the current match time
        ///

        char currentMatchTime[256] = {0};
        int matchTimeReturnValue = bz_callPluginGenericCallback(leagueOverseerPluginName.c_str(), "GetMatchTime", currentMatchTime);

        // When calling the 'GetMatchTime' callback, 1 will be returned if getting the current match time was successful
        if (matchTimeReturnValue == 1)
        {
            std::string matchTime = currentMatchTime;

            // The 'GetMatchTime' callback will give a value of '-00:00' if there is no match in progress
            if (matchTime == "-00:00")
            {
                bz_debugMessagef(0, "There is currently no match in progress.");
            }
            else
            {
                bz_debugMessagef(0, "The current match time is: %s", currentMatchTime);
            }
        }
        else
        {
            bz_debugMessage(0, "The current match time could not be returned for an unknown reason.");
        }
    }
}

void LeagueOverseer::Cleanup (void)
{
    // Clean up all the events
   Flush();

   // Clean up our custom slash commands
   for (auto command : SLASH_COMMANDS)
   {
       bz_removeCustomSlashCommand(command.c_str());
   }
}
