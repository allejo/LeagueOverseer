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
#include <string>

#include "bz_JsonConfig/JsonObject/JsonObject.h"
#include "LeagueOverseer.h"
#include "LeagueOverseer-Helpers.h"

// We got a response from one of our URL jobs
void LeagueOverseer::URLDone (const char* /*URL*/, const void* data, unsigned int /*size*/, bool /*complete*/)
{
    // This variable will only be set to true for the duration of one URL job, so just set it back to false regardless
    MATCH_INFO_SENT = false;

    // Convert the data we get from the URL job to a std::string
    const char* siteData = (const char*)(data);
    logMessage(pluginSettings.getVerboseLevel(), "debug", "URL Job returned: %s", siteData);

    // Start handling the JSON information returned from BZiON
    json_object *obj = json_tokener_parse(siteData);

    JsonObject returnedData;
    JsonObject::buildObject(returnedData, obj);

    std::string datatype = returnedData.getChild("datatype").getString();

    // Check the data type returned in the URL request
    if (datatype == "league_roster")
    {
        for (auto team : returnedData.getChild("roster").getObjectArray())
        {
            std::string motto = team.getChild("name").getString();

            for (auto bzid : team.getChild("players").getStringArray())
            {
                teamMottos[bzid] = motto;
            }
        }
    }
    else if (datatype == "player")
    {
        std::string bzid = returnedData.getChild("bzid").getString();
        std::string motto = returnedData.getChild("motto").getString();

        teamMottos[bzid] = motto;
    }
}

// The league website is down or is not responding, the request timed out
void LeagueOverseer::URLTimeout (const char* /*URL*/, int /*errorCode*/)
{
    logMessage(0, "warning", "The request to the league site has timed out.");

    if (MATCH_INFO_SENT)
    {
        MATCH_INFO_SENT = false;
        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "The match could not be reported due to the connection to the league site timing out.");
        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "If the league site is not down, please notify the server owner to reconfigure this plugin.");
    }
}

// The server owner must have set up the URLs wrong because this shouldn't happen
void LeagueOverseer::URLError (const char* /*URL*/, int errorCode, const char *errorString)
{
    logMessage(0, "error", "Match report failed with the following error:");
    logMessage(0, "error", "Error code: %i - %s", errorCode, errorString);

    if (MATCH_INFO_SENT)
    {
        MATCH_INFO_SENT = false;
        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "An unknown error has occurred, please notify the server owner to reconfigure this plugin.");
    }
}