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
#include <memory>
#include <string>

#include "bzfsAPI.h"

class Match
{
    public:
        bool isOfficial (void);
        bool isFM (void);

        int getTeamIdByPlayerIndex (int playerID);
        int getTeamOneID (void);
        int getTeamTwoID (void);

        std::string getTeamOneName (void);
        std::string getTeamTwoName (void);
        std::string toString (void);

        Match& incrementTeamOneScore (void);
        Match& incrementTeamTwoScore (void);
        Match& setTeamOneName (std::string _teamName);
        Match& setTeamTwoName (std::string _teamName);
        Match& setTeamOneID (int _teamID);
        Match& setTeamTwoID (int _teamID);
        Match& setOfficial (void);
        Match& savePlayer (bz_BasePlayerRecord *pr);
        Match& saveEvent (json_object eventObject);
        Match& setFM (void);
        Match& save (void);

    private:
        struct Player {
            std::string  bzID,
                         callsign,
                         ipAddress;

            bz_eTeamType teamColor;

            Player (std::string _bzID, std::string _callsign, std::string _ipAddress, bz_eTeamType _teamColor) :
                bzID(_bzID),
                callsign(_callsign),
                ipAddress(_ipAddress),
                teamColor(_teamColor)
            {}
        };

        std::vector<Player> matchParticipants;

        json_object *jMaster = json_object_new_object();
        json_object *jEvents = json_object_new_array();

        std::string cancelationReason,
                    teamOneName,
                    teamTwoName;

        double      matchRollCall,
                    matchDuration;

        bool        playersRecorded,
                    matchCanceled,
                    official;
        
        int         teamOneScore,
                    teamTwoScore;
};

#endif