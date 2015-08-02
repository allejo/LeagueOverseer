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

/**
 * A MatchEvent is a base class for events that occur on a server. This is not to be confused with BZFS API events but
 * instead these are events defined by League Overseer that contain meaningful data to be submitted to the league
 * website. All MatchEvents are essentially stored and submitted as JSON objects.
 *
 * For example, a player join is both a BZFS API and a MatchEvent. In this match event, we are saving information such
 * as the IP, a timestamp, and callsign. All of this information will be submitted to the league website.
 */
class MatchEvent
{
public:
    /// Supported MatchEvent types
    enum LosEventType
    {
        CAPTURE,
        PLAYER_SUB,
        PLAYER_KILL,
        PLAYER_JOIN,
        PLAYER_PART,
        LAST_LOS_EVENT_TYPE
    };

    /**
     * Output a string representation of the JSON object with all of the MatchEvent information
     *
     * @return A string representation of the main JSON object
     */
    const char* toString (void)
    {
        return json_object_to_json_string(jsonObj);
    }

    /**
     * Get the main JSON object with all the data respective to a specific event and all of the data that defines the
     * actual MatchType
     *
     * @return The main JSON object with all of the MatchEvent data
     */
    json_object *getJsonObject (void)
    {
        return jsonObj;
    }

    virtual void save (void) = 0;

protected:
    /// The type of event this object is
    LosEventType eventType;

    /// The JSON object that contains the structure of an actual MatchEvent; e.g. event type and event data
    json_object  *jsonObj = json_object_new_object();

    /// A JSON object containing all of the data respective to a MatchEvent
    json_object  *jsonData = json_object_new_object();

    /**
     * Define the type of MatchEvent and save that value in the main JSON object
     *
     * @param _eventType The type of MatchEvent
     */
    void setEventType (LosEventType _eventType)
    {
        eventType = _eventType;

        json_object *jEventType = json_object_new_string(losEventTypeToString(_eventType));

        json_object_object_add(jsonObj, "type", jEventType);
    }

private:
    /**
     * Get a string representation of a MatchEvent
     *
     * @return A string representation of the MatchEvent
     */
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