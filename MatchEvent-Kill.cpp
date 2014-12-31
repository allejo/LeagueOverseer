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

#include <json/json.h>

#include "MatchEvent-Kill.h"

KillMatchEvent::KillMatchEvent ()
{
    this->setEventType(PLAYER_KILL);
}

KillMatchEvent& KillMatchEvent::setKiller (std::string _bzID)
{
    killerBZID = _bzID;

    return *this;
}

KillMatchEvent& KillMatchEvent::setVictim (std::string _bzID)
{
    victimBZID = _bzID;

    return *this;
}

KillMatchEvent& KillMatchEvent::setTime (std::string _matchTime)
{
    matchTime = _matchTime;

    return *this;
}

KillMatchEvent& KillMatchEvent::save (void)
{
    json_object *jKillerBZID = json_object_new_string(killerBZID.c_str());
    json_object *jVictimBZID = json_object_new_string(victimBZID.c_str());
    json_object *jMatchTime  = json_object_new_string(matchTime.c_str());

    json_object_object_add(jsonData, "killer", jKillerBZID);
    json_object_object_add(jsonData, "victim", jVictimBZID);
    json_object_object_add(jsonData, "time", jMatchTime);

    json_object_object_add(jsonObj, "data", jsonData);

    return *this;
}