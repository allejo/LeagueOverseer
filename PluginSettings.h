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

#ifndef __PluginSettings_H_
#define __PluginSettings_H_

#include "bz_JsonConfig/bz_JsonConfig.h"

class PluginSettings : public bz_JsonConfig
{
public:
    enum GameMode {
        OFFICIAL,
        FM,
        IDLE,
        LAST_GAMEMODE
    };

    void loadConfig (const char* filePath);

    std::vector<std::string> getGuestMessagingMessage (GameMode mode);
    std::vector<std::string> getGuestSpawningMessage  (GameMode mode);

    std::string getSpawnCommandPerm (void);
    std::string getMatchReportURL   (void);
    std::string getShowHiddenPerm   (void);
    std::string getMapChangePath    (void);
    std::string getTeamNameURL      (void);
    std::string getLeagueGroup      (void);
    std::string getMottoFormat      (void);

    bool areOfficialMatchesDisabled (void);
    bool isInterPluginCheckEnabled  (void);
    bool isGuestMessagingEnabled    (GameMode mode);
    bool hasGuestSpawningMessage    (GameMode mode);
    bool isGuestSpawningEnabled     (GameMode mode);
    bool areFunMatchesDisabled      (void);
    bool isSpawnMessageEnabled      (void);
    bool ignoreTimeSanityCheck      (void);
    bool isInGameDebugEnabled       (void);
    bool isMatchReportEnabled       (void);
    bool isTalkMessageEnabled       (void);
    bool isMottoFetchEnabled        (void);
    bool isAllowLimitedChat         (void);
    bool isRotationalLeague         (void);

    int  getVerboseLevel            (void);
    int  getDebugLevel              (void);

private:
    std::string gameModeAsString (GameMode mode);
};

#endif