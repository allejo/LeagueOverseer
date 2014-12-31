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

#include <map>
#include <string>
#include <vector>

#include "plugin_utils.h"

#include "ConfigurationOptions.h"
#include "LeagueOverseer-Helpers.h"

ConfigurationOptions::ConfigurationOptions ()
{
    stringConfigValues["SPAWN_COMMAND_PERM"]       = "ban";
    stringConfigValues["MATCH_REPORT_URL"]         = "";
    stringConfigValues["SHOW_HIDDEN_PERM"]         = "ban";
    stringConfigValues["MAPCHANGE_PATH"]           = "";
    stringConfigValues["TEAM_NAME_URL"]            = "";
    stringConfigValues["LEAGUE_GROUP"]             = "VERIFIED";

    boolConfigValues["INTER_PLUGIN_COM_API_CHECK"] = false;
    boolConfigValues["DISABLE_OFFICIAL_MATCHES"]   = false;
    boolConfigValues["PC_PROTECTION_ENABLED"]      = false;
    boolConfigValues["IN_GAME_DEBUG_ENABLED"]      = false;
    boolConfigValues["SPAWN_MESSAGE_ENABLED"]      = true;
    boolConfigValues["MATCH_REPORT_ENABLED"]       = true;
    boolConfigValues["TALK_MESSAGE_ENABLED"]       = true;
    boolConfigValues["MOTTO_FETCH_ENABLED"]        = true;
    boolConfigValues["DISABLE_FUN_MATCHES"]        = false;
    boolConfigValues["ALLOW_LIMITED_CHAT"]         = true;
    boolConfigValues["ROTATIONAL_LEAGUE"]          = false;

    intConfigValues["DEFAULT_TIME_LIMIT"]          = defaultTimeLimit;
    intConfigValues["VERBOSE_LEVEL"]               = 4;
    intConfigValues["DEBUG_LEVEL"]                 = 1;
}

void ConfigurationOptions::readConfigurationFile(const char* filePath)
{
    pluginConfigObj = PluginConfig(filePath);

    if (pluginConfigObj.errors)
    {
        logMessage(0, "error", "Your configuration file has one or more errors. Using default configuration values.");
        return;
    }

    // A short cut configuration value that will take the value of two separate configuration values
    if (isOptionSet("LEAGUE_OVERSEER_URL"))
    {
        stringConfigValues["MATCH_REPORT_URL"] = getString("LEAGUE_OVERSEER_URL");
        stringConfigValues["TEAM_NAME_URL"]    = getString("LEAGUE_OVERSEER_URL");
    }

    for (auto option : stringConfigOptions)
    {
        if (isOptionSet(option.c_str()))
        {
            vectorConfigValues[option] = split(getString(option.c_str()), "\\n");
        }
    }

    for (auto option : stringConfigOptions)
    {
        if (isOptionSet(option.c_str()))
        {
            stringConfigValues[option] = getString(option.c_str());
        }
    }

    for (auto option : boolConfigOptions)
    {
        if (isOptionSet(option.c_str()))
        {
            boolConfigValues[option] = getBool(option.c_str());
        }
    }

    for (auto option : intConfigOptions)
    {
        if (isOptionSet(option.c_str()))
        {
            intConfigValues[option] = getInt(option.c_str());
        }
    }

    sanityChecks();
}

std::vector<std::string> ConfigurationOptions::getNoSpawnMessage (void) { return vectorConfigValues["NO_SPAWN_MESSAGE"]; }
std::vector<std::string> ConfigurationOptions::getNoTalkMessage  (void) { return vectorConfigValues["NO_TALK_MESSAGE"]; }

std::string ConfigurationOptions::getSpawnCommandPerm (void) { return stringConfigValues["SPAWN_COMMAND_PERM"]; }
std::string ConfigurationOptions::getMatchReportURL   (void) { return stringConfigValues["MATCH_REPORT_URL"]; }
std::string ConfigurationOptions::getShowHiddenPerm   (void) { return stringConfigValues["SHOW_HIDDEN_PERM"]; }
std::string ConfigurationOptions::getMapChangePath    (void) { return stringConfigValues["MAPCHANGE_PATH"]; }
std::string ConfigurationOptions::getTeamNameURL      (void) { return stringConfigValues["TEAM_NAME_URL"]; }
std::string ConfigurationOptions::getLeagueGroup      (void) { return stringConfigValues["LEAGUE_GROUP"]; }

bool ConfigurationOptions::areOfficialMatchesDisabled (void) { return boolConfigValues["DISABLE_OFFICIAL_MATCHES"]; }
bool ConfigurationOptions::isInterPluginCheckEnabled  (void) { return boolConfigValues["INTER_PLUGIN_COM_API_CHECK"]; }
bool ConfigurationOptions::areFunMatchesDisabled      (void) { return boolConfigValues["DISABLE_FUN_MATCHES"]; }
bool ConfigurationOptions::isPcProtectionEnabled      (void) { return boolConfigValues["PC_PROTECTION_ENABLED"]; }
bool ConfigurationOptions::isSpawnMessageEnabled      (void) { return boolConfigValues["SPAWN_MESSAGE_ENABLED"]; }
bool ConfigurationOptions::ignoreTimeSanityCheck      (void) { return boolConfigValues["IGNORE_TIME_CHECKS"]; }
bool ConfigurationOptions::isInGameDebugEnabled       (void) { return boolConfigValues["IN_GAME_DEBUG_ENABLED"]; }
bool ConfigurationOptions::isMatchReportEnabled       (void) { return boolConfigValues["MATCH_REPORT_ENABLED"]; }
bool ConfigurationOptions::isTalkMessageEnabled       (void) { return boolConfigValues["TALK_MESSAGE_ENABLED"]; }
bool ConfigurationOptions::isMottoFetchEnabled        (void) { return boolConfigValues["MOTTO_FETCH_ENABLED"]; }
bool ConfigurationOptions::isAllowLimitedChat         (void) { return boolConfigValues["ALLOW_LIMITED_CHAT"]; }
bool ConfigurationOptions::isRotationalLeague         (void) { return boolConfigValues["ROTATIONAL_LEAGUE"]; }

int  ConfigurationOptions::getDefaultTimeLimit        (void) { return intConfigValues["DEFAULT_TIME_LIMIT"]; }
int  ConfigurationOptions::getVerboseLevel            (void) { return intConfigValues["VERBOSE_LEVEL"]; }
int  ConfigurationOptions::getDebugLevel              (void) { return intConfigValues["DEBUG_LEVEL"]; }

bool ConfigurationOptions::isOptionSet(const char* itemName)
{
    return (!pluginConfigObj.item("LeagueOverseer", itemName).empty());
}

bool ConfigurationOptions::getBool(const char* itemName)
{
    return toBool(getString(itemName));
}

int ConfigurationOptions::getInt(const char* itemName)
{
    return atoi(getString(itemName).c_str());
}

std::string ConfigurationOptions::getString(const char* itemName)
{
    return pluginConfigObj.item("LeagueOverseer", itemName);
}

void ConfigurationOptions::sanityChecks()
{
    if (isOptionSet("LEAGUE_OVERSEER_URL") && (isOptionSet("MATCH_REPORT_URL") || isOptionSet("TEAM_NAME_URL")))
    {
        logMessage(0, "warning", "You have set the 'LEAGUE_OVERSEER_URL' configuration value but you have also specified");
        logMessage(0, "warning", "a 'MATCH_REPORT_URL' or 'TEAM_NAME_URL' value. This configuration may lead to unintended");
        logMessage(0, "warning", "side effects. Please resolve this issue but only setting 'LEAGUE_OVERSEER_URL' OR by");
        logMessage(0, "warning", "setting 'MATCH_REPORT_URL' and 'TEAM_NAME_URL' manually.");
    }

    if (isMatchReportEnabled() && getMatchReportURL().empty())
    {
        logMessage(0, "error", "You are requesting to report matches but you have not specified a URL to report to.");
        logMessage(0, "error", "Please set the 'MATCH_REPORT_URL' or 'LEAGUE_OVERSEER_URL' option respectively.");
        logMessage(0, "error", "If you do not wish to report matches, set 'DISABLE_MATCH_REPORT' to true.");
    }

    if (isMottoFetchEnabled() && getTeamNameURL().empty())
    {
        logMessage(0, "error", "You have requested to fetch team names but have not specified a URL to fetch them from.");
        logMessage(0, "error", "Please set the 'TEAM_NAME_URL' or 'LEAGUE_OVERSEER_URL' option respectively.");
        logMessage(0, "error", "If you do not wish to team names for mottos, set 'DISABLE_TEAM_MOTTO' to true.");
    }

    if (getDebugLevel() > 4 || getDebugLevel() < 0)
    {
        intConfigValues["DEBUG_LEVEL"] = 1;
        logMessage(0, "warning", "Invalid debug level in the configuration file. Default value used: %d", getDebugLevel());
    }

    if (getVerboseLevel() > 4 || getVerboseLevel() < 0)
    {
        intConfigValues["VERBOSE_LEVEL"] = 4;
        logMessage(0, "warning", "Invalid verbose level in the configuration file. Default value used: %d", getVerboseLevel());
    }

    if (getDefaultTimeLimit() < 600 && !ignoreTimeSanityCheck())
    {
        intConfigValues["DEFAULT_TIME_LIMIT"] = defaultTimeLimit;
        logMessage(0, "warning", "The default time limit for matches is less than 10 minutes. The default time limit set");
        logMessage(0, "warning", "to %d minutes. If your league consists of extremely short matches, use the IGNORE_TIME_CHECKS", (getDefaultTimeLimit() / 60));
        logMessage(0, "warning", "option in your configuration file.");
        logMessage(0, "warning", "* Please note, DEFAULT_TIME_LIMIT is specified in seconds so calculate accordingly.");
    }
}