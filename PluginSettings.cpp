//
// Created by Vladimir Jimenez on 8/4/15.
// Copyright (c) 2015 BZFlag. All rights reserved.
//

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
