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

#include "MatchEvent-Part.h"

PartMatchEvent::PartMatchEvent ()
{
    setEventType(PLAYER_PART);
    setTimestamp();
}

PartMatchEvent& PartMatchEvent::setBZID (std::string _bzID)
{
    bzID = _bzID;

    return *this;
}

PartMatchEvent& PartMatchEvent::save (void)
{
    json_object *jTimestamp = json_object_new_string(timestamp.c_str());
    json_object *jBZID      = json_object_new_string(bzID.c_str());

    json_object_object_add(jsonData, "timestamp", jTimestamp);
    json_object_object_add(jsonData, "bzid", jBZID);

    json_object_object_add(jsonObj, "data", jsonData);

    return *this;
}

PartMatchEvent& PartMatchEvent::setTimestamp ()
{
    timestamp = getCurrentTimeStamp();

    return *this;
}