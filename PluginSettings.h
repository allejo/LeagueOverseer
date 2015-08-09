//
// Created by Vladimir Jimenez on 8/4/15.
// Copyright (c) 2015 BZFlag. All rights reserved.
//


#ifndef __PluginSettings_H_
#define __PluginSettings_H_

#include "bz_JsonConfig.h"

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
    bool isGuestMessagingEnable     (GameMode mode);
    bool areFunMatchesDisabled      (void);
    bool isGuestSpawningEnable      (GameMode mode);
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
};

#endif