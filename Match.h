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

#ifndef __MATCH_OBJ_H__
#define __MATCH_OBJ_H__

#include <json/json.h>
#include <map>
#include <memory>
#include <string>

#include "bzfsAPI.h"

class Match
{
    public:
        bool isRosterEmpty (void);
        bool matchCanceled (void);
        bool reportMatch (void);
        bool isOfficial (void);
        bool isFM (void);

        int incrementMatchRollCall (int _newRollCall);
        int getMatchRollCall (void);
        int getMatchDuration (void);
        int getTeamOneScore (void);
        int getTeamTwoScore (void);
        int getTeamOneID (void);
        int getTeamTwoID (void);

        std::string getTeamOneName (void);
        std::string getTeamTwoName (void);
        std::string getCancelation (void);
        std::string toString (void);

        Match& setMatchDuration (int _duration);
        Match& setVerboseLevel (int _verbose);
        Match& setTeamOneName (std::string _teamName);
        Match& setTeamTwoName (std::string _teamName);
        Match& setDebugLevel (int _debug);
        Match& setTeamOneID (int _teamID);
        Match& setTeamTwoID (int _teamID);
        Match& setOfficial (void);
        Match& cancelMatch (std::string reason);
        Match& reportMatch (bool _report);
        Match& savePlayer (bz_BasePlayerRecord *pr, int teamID);
        Match& saveEvent (json_object *eventObject);
        Match& setFM (void);

        void stats_createPlayer (int playerID);
        void stats_flagCapture (int playerID);
        void stats_playerKill (int killerID, int victimID);

        void incrementTeamOneScore (void);
        void incrementTeamTwoScore (void);
        void clearPlayerRoster (void);
        void save (std::string _timestamp, std::string _replayFile);


    private:
        struct Player {
            int          teamID;

            std::string  bzID,
                         callsign,
                         ipAddress;

            bz_eTeamType teamColor;
        };

        struct PlayerStats {
            std::map <std::string, int> killsAgainst,
                                        deathsAgainst;

            std::string bzID;

            int captureCount,
                deathCount,
                killCount,
                teamKills,
                selfKills;
        };

        std::vector<Player> matchParticipants;

        std::map <std::string, PlayerStats> matchPlayerStats;

        json_object *jMaster = json_object_new_object();
        json_object *jEvents = json_object_new_array();

        std::string cancelationReason,
                    teamOneName,
                    teamTwoName,
                    mapPlayed;

        bool        playersRecorded,
                    isMatchCanceled,
                    reportTheMatch = true,
                    official;

        int         matchDuration,
                    matchRollCall = 15,
                    verboseLevel,
                    teamOneScore = 0,
                    teamTwoScore = 0,
                    debugLevel,
                    teamOneID,
                    teamTwoID;
};

#endif