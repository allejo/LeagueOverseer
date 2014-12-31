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

#include "LeagueOverseer.h"
#include "LeagueOverseer-Helpers.h"

int LeagueOverseer::GeneralCallback (const char* name, void* data)
{
	logMessage(pluginSettings.getVerboseLevel(), "callback", "A plug-in has requested the '%s' callback.", name);

    // Store the callback that is being called for easy access
    std::string callbackOption = name;

    if (callbackOption == "GetMatchProgress")
    {
        int matchProgress = getMatchProgress();

        logMessage(pluginSettings.getVerboseLevel(), "callback", "Returning the current match progress (%d)...", matchProgress);
        return matchProgress;
    }
    else if (callbackOption == "GetMatchTime")
    {
        std::string matchTime = getMatchTime();

        strcpy((char*)data, matchTime.c_str());

        logMessage(pluginSettings.getVerboseLevel(), "callback", "Returning the current match time (%s)...", matchTime.c_str());
        return 1;
    }
    else if (callbackOption == "IsOfficialMatch")
    {
        bool isOfficial = isOfficialMatch();

        logMessage(pluginSettings.getVerboseLevel(), "callback", "Returning that an match is %sin progress.", (isOfficial) ? "" : "not ");
        return (int)isOfficial;
    }

    logMessage(pluginSettings.getVerboseLevel(), "callback", "The '%s' callback was not found.", name);
    return 0;
}