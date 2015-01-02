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

#ifndef __MATCH_EVENT_H__
#define __MATCH_EVENT_H__

#include <json/json.h>

#include "LeagueOverseer-Helpers.h"

template<class Derived>
class MatchEvent
{
    Derived* This() { return static_cast<Derived*>(this); }

    public:
        enum LosEventType
        {
            CAPTURE,
            PLAYER_SUB,
            PLAYER_KILL,
            PLAYER_JOIN,
            PLAYER_PART,
            LAST_LOS_EVENT_TYPE
        };

        const char* toString (void)
        {
            return json_object_to_json_string(jsonObj);
        }

        json_object *getJsonObject (void)
        {
            return jsonData;
        }

        virtual Derived& save (void) = 0;

    protected:
        LosEventType eventType;

        json_object  *jsonObj = json_object_new_object();
        json_object  *jsonData = json_object_new_object();

        Derived& setEventType (LosEventType _eventType)
        {
            eventType = _eventType;

            json_object *jEventType = json_object_new_string(losEventTypeToString(_eventType));

            json_object_object_add(jsonObj, "type", jEventType);

            return *This();
        }

        const char* losEventTypeToString (LosEventType _eventType)
        {
            switch (_eventType)
            {
                case CAPTURE:
                    return "capture";

                case PLAYER_SUB:
                    return "substitute";

                case PLAYER_KILL:
                    return "kill";

                case PLAYER_JOIN:
                    return "join";

                case PLAYER_PART:
                    return "part";

                default:
                    return "noEvent";
            }
        }
};

#endif