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

#ifndef __PLUGIN_HELPERS_H__
#define __PLUGIN_HELPERS_H__

#include <json/json.h>
#include <string>
#include <vector>

#include "bzfsAPI.h"
#include "plugin_utils.h"

// The default messages to send based on the case
enum DefaultMsgType
{
    CHAT,
    SPAWN,
    LAST_MSG_TYPE
};

/**
 * Split a string by a delimeter and return a vector of elements
 *
 * @param  string    The string that will be split
 * @param  delimeter The delimiter that will be used to split a string
 *
 * @return           A vector of strings gotten after the original string was split
 */
std::vector<std::string> split (std::string string, std::string delimeter);

/**
 * Return a string representation of a boolean
 *
 * @param  value The boolean you want to be converted in a string
 *
 * @return       The string represenation of a boolean
 */
std::string boolToString (bool value);

/**
 * Convert a bz_eTeamType value into a string literal with the option of adding whitespace to format the string to return
 *
 * @param  teamColor       The team color that you want a string representation of
 *
 * @return A string representation of a team color
 */
std::string eTeamToString(bz_eTeamType teamColor);

/**
 * Convert a bz_eTeamType value into a string literal with the option of adding whitespace to format the string to return
 *
 * @param  teamColor       The team color that you want a string representation of
 * @param  addWhiteSpace   Whether or not to add space after the string representation to have a set amount of characters (used for lining up text)
 * @param  totalCharacters The total amount of characters both the string respentation and white space should take up together
 *
 * @return                 The string representation of a team color with white padding added if specified
 */
std::string formatTeam (bz_eTeamType teamColor, bool addWhiteSpace = false, unsigned int totalCharacters = 7);

/**
 * Get the current time stamp in the specified format
 *
 * @param  format The format you'd like the timestamp to be formatted into
 *
 * @return        A formatted timestamp
 */
const char* getCurrentTimeStamp(std::string format = "%02d-%02d-%02d %02d:%02d:%02d");

/**
 * Get the bz_eTeamType from a flag abbreviation
 *
 * @param  flagAbbr The flag abbreviation we're searching for
 *
 * @return          The team the flag belongs to. Return eNoTeam if the flag is not a team flag
 */
bz_eTeamType getTeamTypeFromFlag(std::string flagAbbr);

/**
 * Output a formatted debug message that will follow a similar syntax and can be easily found in the log file.
 *    (example) MSG TYPE :: League Overseer :: A sample message that has been written to the log file
 *
 * @param debugLevel The debug level that at which this message will be outputted to the log file
 * @param msgType    The type of the message. For example: debug, verbose, error, warning
 * @param fmt        The format of the message that will accept placeholders
 */
void logMessage (int debugLevel, const char* msgType, const char* fmt, ...);

/**
 * Modify the perms of all of the players on the server
 *
 * @param grant True if you'd like to grant a specific permission. False to remove it.
 * @param perm  The server permission you'd like to revoke or grant
 */
void modifyPerms(bool grant, std::string perm);

/**
 * A convenience method for modifyPerms() that will grant permissions
 *
 * @param perm The permission that you would like to grant to everyone
 */
void grantPermToAll(std::string perm);

/**
 * A convenience method for modifyPerms() that will revoke permissions
 *
 * @param perm The permission that you would like to revoke from everyone
 */
void revokePermFromAll(std::string perm);

/**
 * Send a player a message that is stored in a vector
 *
 * @param playerID The ID of the player who the message is being sent to
 * @param message  A vector containing the message sent to player
 */
void sendPluginMessage (int playerID, std::vector<std::string> message);

/**
 * Check if a string is an integer
 *
 * @param  str The string in question
 *
 * @return     Returns true if the string is an integer
 */
bool isInteger(std::string str);

/**
 * Return whether or not a specified player ID exists or not
 *
 * @param  playerID The player ID we are checking
 *
 * @return          True if a player was found with the specified player ID
 */
bool isValidPlayerID (int playerID);

/**
 * Convert a string representation of a boolean to a boolean
 *
 * @param  str The string representation of a boolean
 *
 * @return     True if the string is not empty, is equal to "true" or does not equal 0
 */
bool toBool (std::string str);

/**
 * Create a BZDB variable if it doesn't exist. This is used because if the variable already exists via -setforced in
 * the configuration file, then this value would be overloaded and we don't want that
 *
 * @param  bzdbVar    The name of the BZDB variable
 * @param  value      The value of the variable
 * @param  perms      What to set the permission value to. (Range: 1-3. Using 0 sets it to the default [2], and using a higher number sets it to 3.)
 * @param  persistent Whether or not the variable will be persistent.
 *
 * @return            The value of the BZDB variable
 */
int registerCustomIntBZDB (const char* bzdbVar, int value, int perms = 0, bool persistent = false);

/**
 * Create a BZDB variable if it doesn't exist. This is used because if the variable already exists via -setforced in
 * the configuration file, then this value would be overloaded and we don't want that
 *
 * @param  bzdbVar    The name of the BZDB variable
 * @param  value      The value of the variable
 * @param  perms      What to set the permission value to. (Range: 1-3. Using 0 sets it to the default [2], and using a higher number sets it to 3.)
 * @param  persistent Whether or not the variable will be persistent.
 *
 * @return            The value of the BZDB variable
 */
bool registerCustomBoolBZDB (const char* bzdbVar, bool value, int perms = 0, bool persistent = false);

/**
 * A convenience method for retrieving the _pcProtectionEnabled BZDB variable
 *
 * @return True if Pass Camping Protection is enabled on the server
 */
bool isPcProtectionEnabled (void);

/**
 * A convenience method for retrieving the _pcProtectionDelay BZDB variable
 *
 * @return The number of seconds a team flag is protected after a flag capture
 */
int getPcProtectionDelay (void);

/**
 * A convenience method for retrieving the _defaultTimeLimit BZDB variable
 *
 * @return The time (in seconds) the default match length is set to
 */
int getDefaultTimeLimit (void);

#endif