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

#include "PluginSettings.h"

void PluginSettings::loadConfig (char const *filePath)
{
    loadConfigurationFile(filePath);
}

std::string PluginSettings::getSpawnCommandPerm (void)
{
    return root.getChild("spawn_command_perm").getString();
}

std::string PluginSettings::getMatchReportURL (void)
{
    return root.getChild("match_report_url").getString();
}

std::string PluginSettings::getShowHiddenPerm (void)
{
    return root.getChild("show_hidden_perm").getString();
}

std::string PluginSettings::getMapChangePath (void)
{
    return root.getChild("mapchange_path").getString();
}

std::string PluginSettings::getTeamNameURL (void)
{
    return root.getChild("team_name_url").getString();
}

std::string PluginSettings::getLeagueGroup (void)
{
    return root.getChild("league_group").getString();
}

std::string PluginSettings::getMottoFormat (void)
{
    return root.getChild("motto_format").getString();
}

bool PluginSettings::areOfficialMatchesDisabled (void)
{
    return root.getChild("disable_official_matches").getBool();
}

bool PluginSettings::isInterPluginCheckEnabled (void)
{
    return root.getChild("interplugin_api_check").getBool();
}

bool PluginSettings::areFunMatchesDisabled (void)
{
    return root.getChild("disable_fun_matches").getBool();
}

bool PluginSettings::isPcProtectionEnabled (void)
{
    return root.getChild("pc_protection_enabled").getBool();
}

bool PluginSettings::ignoreTimeSanityCheck (void)
{
    return root.getChild("ignore_time_checks").getBool();
}

bool PluginSettings::isInGameDebugEnabled (void)
{
    return root.getChild("in_game_debug_enabled").getBool();
}

bool PluginSettings::isMatchReportEnabled (void)
{
    return root.getChild("match_report_enabled").getBool();
}

bool PluginSettings::isMottoFetchEnabled (void)
{
    return root.getChild("motto_fetch_enabled").getBool();
}

bool PluginSettings::isRotationalLeague (void)
{
    return root.getChild("rotational_league").getBool();
}

int PluginSettings::getDefaultTimeLimit (void)
{
    return root.getChild("default_time_limit").getInt();
}

int PluginSettings::getVerboseLevel (void)
{
    return root.getChild("verbose_level").getInt();
}

int PluginSettings::getDebugLevel (void)
{
    return root.getChild("debug_level").getInt();
}
