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
    if (root.hasChild("spawn_command_perm"))
    {
        return root.getChild("spawn_command_perm").getString();
    }

    return "ban";
}

std::string PluginSettings::getMatchReportURL (void)
{
    if (root.hasChild("match_report_url"))
    {
        return root.getChild("match_report_url").getString();
    }

    return "";
}

std::string PluginSettings::getShowHiddenPerm (void)
{
    if (root.hasChild("show_hidden_perm"))
    {
        return root.getChild("show_hidden_perm").getString();
    }

    return "ban";
}

std::string PluginSettings::getMapChangePath (void)
{
    if (root.hasChild("mapchange_path"))
    {
        return root.getChild("mapchange_path").getString();
    }

    return "";
}

std::string PluginSettings::getTeamNameURL (void)
{
    if (root.hasChild("team_name_url"))
    {
        return root.getChild("team_name_url").getString();
    }

    return "";
}

std::string PluginSettings::getLeagueGroup (void)
{
    if (root.hasChild("league_group"))
    {
        return root.getChild("league_group").getString();
    }

    return "VERIFIED";
}

std::string PluginSettings::getMottoFormat (void)
{
    if (root.hasChild("motto_format"))
    {
        return root.getChild("motto_format").getString();
    }

    return "{team}";
}

bool PluginSettings::areOfficialMatchesDisabled (void)
{
    if (root.hasChild("disable_official_matches"))
    {
        return root.getChild("disable_official_matches").getBool();
    }

    return false;
}

bool PluginSettings::isInterPluginCheckEnabled (void)
{
    if (root.hasChild("interplugin_api_check"))
    {
        return root.getChild("interplugin_api_check").getBool();
    }

    return false;
}

bool PluginSettings::areFunMatchesDisabled (void)
{
    if (root.hasChild("disable_fun_matches"))
    {
        return root.getChild("disable_fun_matches").getBool();
    }

    return false;
}

bool PluginSettings::ignoreTimeSanityCheck (void)
{
    if (root.hasChild("ignore_time_checks"))
    {
        return root.getChild("ignore_time_checks").getBool();
    }

    return false;
}

bool PluginSettings::isGuestSpawningEnable(PluginSettings::GameMode mode)
{
    if (!root.hasChild("guest_spawning"))
    {
        return false;
    }

    JsonObject guest_spawning = root.getChild("guest_spawning");
    std::string gamemode = gameModeAsString(mode);

    if (!guest_spawning.hasChild(gamemode) || !guest_spawning.getChild(gamemode).hasChild("allowed"))
    {
        return false;
    }

    return guest_spawning.getChild(gamemode).getChild("allowed").getBool();
}

bool PluginSettings::isInGameDebugEnabled (void)
{
    if (root.hasChild("in_game_debug_enabled"))
    {
        return root.getChild("in_game_debug_enabled").getBool();
    }

    return false;
}

bool PluginSettings::isMatchReportEnabled (void)
{
    if (root.hasChild("match_report_enabled"))
    {
        return root.getChild("match_report_enabled").getBool();
    }

    return true;
}

bool PluginSettings::isMottoFetchEnabled (void)
{
    if (root.hasChild("motto_fetch_enabled"))
    {
        return root.getChild("motto_fetch_enabled").getBool();
    }

    return true;
}

bool PluginSettings::isRotationalLeague (void)
{
    if (root.hasChild("rotational_league"))
    {
        return root.getChild("rotational_league").getBool();
    }

    return false;
}

int PluginSettings::getVerboseLevel (void)
{
    if (root.hasChild("verbose_level"))
    {
        return root.getChild("verbose_level").getInt();
    }

    return 4;
}

int PluginSettings::getDebugLevel (void)
{
    if (root.hasChild("debug_level"))
    {
        return root.getChild("debug_level").getInt();
    }

    return 1;
}

std::string PluginSettings::gameModeAsString(PluginSettings::GameMode mode)
{
    switch (mode)
    {
        case OFFICIAL:
        {
            return "during_official";
        }

        case FM:
        {
            return "during_fm";
        }

        case IDLE:
        {
            return "idle";
        }

        default:
        {
            return "Unknown";
        }
    }
}
