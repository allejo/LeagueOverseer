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

#include <cstdarg>
#include <memory>
#include <string>
#include <vector>

#include "bzfsAPI.h"
#include "plugin_utils.h"

#include "LeagueOverseer-Helpers.h"
#include "LeagueOverseer-Version.h"

std::vector<std::string> split (std::string string, std::string delimeter)
{
    // I love you, JeffM <3
    // Thanks for tokenize()
    return tokenize(string, delimeter, 0, true);
}

std::string boolToString(bool value)
{
    return (value) ? "true" : "false";
}

std::string eTeamToString(bz_eTeamType teamColor)
{
    switch (teamColor)
    {
        case eBlueTeam:
            return "Blue";

        case eGreenTeam:
            return "Green";

        case ePurpleTeam:
            return "Purple";

        case eRedTeam:
            return "Red";

        default:
            return "NoTeam";
    }
}

std::string formatTeam (bz_eTeamType teamColor, bool addWhiteSpace, unsigned int totalCharacters)
{
    // Because we may have to format the string with white space padding, we need to store
    // the value somewhere
    std::string color;

    // Switch through the supported team colors of this plugin
    switch (teamColor)
    {
        case eBlueTeam:
            color = "Blue";
            break;

        case eGreenTeam:
            color = "Green";
            break;

        case ePurpleTeam:
            color = "Purple";
            break;

        case eRedTeam:
            color = "Red";
            break;

        default:
            break;
    }

    // We may want to format the team color name with white space for the debug messages
    if (addWhiteSpace)
    {
        // Our max padding length will be 'totalWhiteSpace' so add white space as needed
        while (color.length() < totalCharacters)
        {
            color += " ";
        }
    }

    // Return the team color with or without the padding
    return color;
}

const char* getCurrentTimeStamp(std::string format)
{
    // Get the current standard UTC time
    bz_Time standardTime;
    bz_getUTCtime(&standardTime);

    // Format the date to -> year-month-day hour:minute:second
    char matchDate[20];
    sprintf(matchDate, format.c_str(), standardTime.year, standardTime.month, standardTime.day, standardTime.hour, standardTime.minute, standardTime.second);

    return std::string(matchDate).c_str();
}

bz_eTeamType getTeamTypeFromFlag(std::string flagAbbr)
{
    if (flagAbbr == "R*")
    {
        return eRedTeam;
    }
    else if (flagAbbr == "G*")
    {
        return eGreenTeam;
    }
    else if (flagAbbr == "B*")
    {
        return eBlueTeam;
    }
    else if (flagAbbr == "P*")
    {
        return ePurpleTeam;
    }

    return eNoTeam;
}

void logMessage (int debugLevel, const char* msgType, const char* fmt, ...)
{
    char buffer[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, 4096, fmt, args);
    va_end(args);

    std::string message = std::string(bz_toupper(msgType)) + " :: " + PLUGIN_NAME + " :: " + std::string(buffer);

    bz_debugMessage(debugLevel, message.c_str());
}

void modifyPerms(bool grant, std::string perm)
{
    // Use a smart pointer so we don't have to worry about freeing up the memory
    // when we're done. In other words, laziness.
    std::shared_ptr<bz_APIIntList> playerList(bz_getPlayerIndexList());

    // Be sure the playerlist exists
    if (playerList)
    {
        // Loop through all of the players' callsigns
        for (unsigned int i = 0; i < playerList->size(); i++)
        {
            if (grant)
            {
                bz_grantPerm(playerList->get(i), perm.c_str());
            }
            else
            {
                bz_revokePerm(playerList->get(i), perm.c_str());
            }
        }
    }
}

void grantPermToAll(std::string perm)
{
    modifyPerms(true, perm);
}

void revokePermFromAll(std::string perm)
{
    modifyPerms(false, perm);
}

void sendPluginMessage (int playerID, std::vector<std::string> message)
{
    for (std::vector<std::string>::const_iterator it = message.begin(); it != message.end(); ++it)
    {
        std::string currentLine = std::string(*it);
        bz_sendTextMessagef(BZ_SERVER, playerID, "%s", currentLine.c_str());
    }
}

bool isInteger(std::string str)
{
    try
    {
        std::stoi(str);
    }
    catch (std::exception const &e)
    {
        return false;
    }

    return true;
}

bool isValidPlayerID (int playerID)
{
    // Use another smart pointer so we don't forget about freeing up memory
    std::shared_ptr<bz_BasePlayerRecord> playerData(bz_getPlayerByIndex(playerID));

    // If the pointer doesn't exist, that means the playerID does not exist
    return (playerData) ? true : false;
}

bool toBool (std::string str)
{
    return !str.empty() && (strcasecmp(str.c_str (), "true") == 0 || atoi(str.c_str ()) != 0);
}

int registerCustomIntBZDB(const char* bzdbVar, int value, int perms, bool persistent)
{
    if (!bz_BZDBItemExists(bzdbVar))
    {
        bz_setBZDBInt(bzdbVar, value, perms, persistent);
    }

    return bz_getBZDBInt(bzdbVar);
}

bool registerCustomBoolBZDB(const char* bzdbVar, bool value, int perms, bool persistent)
{
    if (!bz_BZDBItemExists(bzdbVar))
    {
        bz_setBZDBBool(bzdbVar, value, perms, persistent);
    }

    return bz_getBZDBBool(bzdbVar);
}

bool isPcProtectionEnabled (void)
{
    return bz_getBZDBBool("_pcProtectionEnabled");
}

int getPcProtectionDelay (void)
{
    return bz_getBZDBInt("_pcProtectionDelay");
}

int getDefaultTimeLimit (void)
{
    return bz_getBZDBInt("_defaultTimeLimit");
}