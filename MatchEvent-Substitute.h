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

#ifndef __MATCH_SUB_EVENT_H__
#define __MATCH_SUB_EVENT_H__

#include <string>

#include "MatchEvent.h"

class SubMatchEvent : public MatchEvent<SubMatchEvent>
{
    public:
        SubMatchEvent ();

        SubMatchEvent& setMatchTime (std::string _matchTime);
        SubMatchEvent& setGoingOut  (void);
        SubMatchEvent& setGoingIn   (void);
        SubMatchEvent& setTeamID    (int _teamID);
        SubMatchEvent& setBZID      (std::string _bzID);
        SubMatchEvent& save         (void);

    private:
        int         teamID;

        std::string matchTime,
                    action,
                    bzID;
};

#endif