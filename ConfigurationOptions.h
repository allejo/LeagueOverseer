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

#ifndef __CONFIG_OPTIONS_H__
#define __CONFIG_OPTIONS_H__

#include <map>
#include <string>
#include <vector>

#include "plugin_utils.h"

class ConfigurationOptions
{
    public:
        ConfigurationOptions ();

        void readConfigurationFile(const char* filePath);

        std::vector<std::string> getNoSpawnMessage (void);
        std::vector<std::string> getNoTalkMessage  (void);

        std::string getSpawnCommandPerm (void);
        std::string getMatchReportURL   (void);
        std::string getShowHiddenPerm   (void);
        std::string getMapChangePath    (void);
        std::string getTeamNameURL      (void);
        std::string getLeagueGroup      (void);

        bool areOfficialMatchesDisabled (void);
        bool isInterPluginCheckEnabled  (void);
        bool areFunMatchesDisabled      (void);
        bool isPcProtectionEnabled      (void);
        bool isSpawnMessageEnabled      (void);
        bool ignoreTimeSanityCheck      (void);
        bool isInGameDebugEnabled       (void);
        bool isMatchReportEnabled       (void);
        bool isTalkMessageEnabled       (void);
        bool isMottoFetchEnabled        (void);
        bool isAllowLimitedChat         (void);
        bool isRotationalLeague         (void);

        int  getDefaultTimeLimit        (void);
        int  getVerboseLevel            (void);
        int  getDebugLevel              (void);

    private:
        PluginConfig pluginConfigObj;
        int defaultTimeLimit = 1800;

        std::vector<std::string> vectorConfigOptions = {
                                                            "NO_SPAWN_MESSAGE",         // The message for users who can't spawn; will be sent when they try to spawn
                                                            "NO_TALK_MESSAGE"           // The message for users who can't talk; will be sent when they try to talk
                                                       };

        std::vector<std::string> stringConfigOptions = {
                                                            "SPAWN_COMMAND_PERM",       // The BZFS permission required to use the /spawn command
                                                            "MATCH_REPORT_URL",         // The URL the plugin will use to report matches
                                                            "SHOW_HIDDEN_PERM",         // The BZFS permission required to use the /showhidden command
                                                            "MAPCHANGE_PATH",           // The path to the file that contains the name of current map being played
                                                            "TEAM_NAME_URL",            // The URL the plugin will use to fetch team information
                                                            "LEAGUE_GROUP"              // The BZBB group that signifies membership of a league (typically in the format of <something>.LEAGUE)
                                                       };

        std::vector<std::string> boolConfigOptions   = {
                                                            "DISABLE_OFFICIAL_MATCHES", // Whether or not official matches have been disabled on this server
                                                            "IN_GAME_DEBUG_ENABLED",    // Whether or not the "lodgb" command is enabled
                                                            "INTERPLUGIN_API_CHECK",    // Whether or not to check the inter plug-in API to make sure it works when it's first loaded
                                                            "PC_PROTECTION_ENABLED",    // Whether or not the PC protection is enabled
                                                            "SPAWN_MESSAGE_ENABLED",    // Whether or not to send custom messages explaining why players can't spawn
                                                            "MATCH_REPORT_ENABLED",     // Whether or not to enable automatic match reports if a server is not used as an official match server
                                                            "TALK_MESSAGE_ENABLED",     // Whether or not to send custom messages explaining why players can't talk
                                                            "MOTTO_FETCH_ENABLED",      // Whether or not to set a player's motto to their team name
                                                            "DISABLE_FUN_MATCHES",      // Whether or not fun matches have been disabled on this server
                                                            "ALLOW_LIMITED_CHAT",       // Whether or not to allow limited chat functionality for non-league players
                                                            "IGNORE_TIME_CHECKS",       // Whether or not to check for the DEFAULT_TIME_LIMIT to be sane
                                                            "ROTATIONAL_LEAGUE"         // Whether or not we are watching a league that uses different maps
                                                       };

        std::vector<std::string> intConfigOptions    = {
                                                            "DEFAULT_TIME_LIMIT",       // The default time limit each match will have
                                                            "VERBOSE_LEVEL",            // This is the spamming/ridiculous level of debug that the plugin uses
                                                            "DEBUG_LEVEL"               // The DEBUG level the server owner wants the plugin to use for its messages
                                                       };

        std::map<std::string, std::vector<std::string>>  vectorConfigValues;
        std::map<std::string, std::string>               stringConfigValues;
        std::map<std::string, bool>                      boolConfigValues;
        std::map<std::string, int>                       intConfigValues;

        std::string getString    (const char* itemName);
        void        sanityChecks (void);
        bool        isOptionSet  (const char* itemName);
        bool        getBool      (const char* itemName);
        int         getInt       (const char* itemName);
};

#endif