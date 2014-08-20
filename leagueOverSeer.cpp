/*
League Overseer
    Copyright (C) 2013-2014 Vladimir Jimenez & Ned Anderson

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

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <json/json.h>
#include <math.h>
#include <memory>
#include <sstream>
#include <stdarg.h>
#include <string>
#include <time.h>
#include <vector>

#include "bzfsAPI.h"
#include "plugin_utils.h"

// Define plugin name
const std::string PLUGIN_NAME = "League Overseer";

// Define plugin version numbering
const int MAJOR = 1;
const int MINOR = 2;
const int REV = 0;
const int BUILD = 335;

// The API number used to notify the PHP counterpart about how to handle the data
const int API_VERSION = 2;

// The default messages to send based on the case
enum DefaultMsgType
{
    CHAT,
    SPAWN,
    LAST_MSG_TYPE
};

// Return a literal value of a boolean
static std::string boolToString(bool value)
{
    return (value) ? "true" : "false";
}

// A function to simply format the beginning of the debug messages outputted by this plugin
static std::string formatDebug (std::string msgType)
{
    msgType = bz_toupper(msgType.c_str());

    return msgType + " :: " + PLUGIN_NAME + " :: ";
}

// Output a formatted debug message and make it look nice
static void logMessage (int debugLevel, std::string msgType, const char* fmt, ...)
{
    char buffer[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, 4096, fmt, args);
    va_end(args);

    std::string message = formatDebug(msgType) + std::string(buffer);

    bz_debugMessage(debugLevel, message.c_str());
}

// A function that will get the player record by their callsign
static bz_BasePlayerRecord* bz_getPlayerByCallsign (const char* callsign)
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
            // Have we found the callsign we're looking for?
            if (bz_getPlayerByIndex(playerList->get(i))->callsign == callsign)
            {
                // Return the record for that player
                return bz_getPlayerByIndex(playerList->get(i));
            }
        }
    }

    // Return NULL if the callsign was not found
    return NULL;
}

// Convert a bz_eTeamType value into a string literal with the option
// of adding whitespace to format the string to return
static std::string formatTeam (bz_eTeamType teamColor, bool addWhiteSpace = false)
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
        // Our max padding length will be 7 so add white space as needed
        while (color.length() < 7)
        {
            color += " ";
        }
    }

    // Return the team color with or without the padding
    return color;
}

// Get the bz_eTeamType of a flag abbreviation
static bz_eTeamType getTeamTypeFromFlag(std::string flagAbbr)
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
    else
    {
        return eNoTeam;
    }
}

// Modify the perms of all of the players on the server
static void modifyPerms(bool grant, std::string perm)
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

// Shortcut to call the modifyPerms() function without having to remember the boolean in order to grant perms
static void grantPermToAll(std::string perm)
{
    modifyPerms(true, perm);
}

// Shortcut to call the modifyPerms() function without having to remember the boolean in order to revoke perms
static void revokePermFromAll(std::string perm)
{
    modifyPerms(false, perm);
}

// Convert an int to a string
static std::string intToString (int number)
{
    std::stringstream string;
    string << number;

    return string.str();
}

// Return whether or not a specified player ID exists or not
static bool isValidPlayerID (int playerID)
{
    // Use another smart pointer so we don't forget about freeing up memory
    std::shared_ptr<bz_BasePlayerRecord> playerData(bz_getPlayerByIndex(playerID));

    // If the pointer doesn't exist, that means the playerID does not exist
    return (playerData) ? true : false;
}

// Get a player record of a player from either a slot ID or a callsign
static bz_BasePlayerRecord* getPlayerFromCallsignOrID(std::string callsignOrID)
{
    // We have a pound sign followed by a valid player index
    if (std::string::npos != callsignOrID.find("#") && isValidPlayerID(atoi(callsignOrID.erase(0, 1).c_str())))
    {
        // Let's make some easy reference variables
        int victimPlayerID = atoi(callsignOrID.erase(0, 1).c_str());

        return bz_getPlayerByIndex(victimPlayerID);
    }
    else if (bz_getPlayerByCallsign(callsignOrID.c_str()) != NULL) // It's a valid callsign
    {
        return bz_getPlayerByCallsign(callsignOrID.c_str());
    }

    return NULL;
}

void registerCustomIntBZDB(const char* bzdbVar, int value, int perms = 0, bool persistent = false)
{
    if (!bz_BZDBItemExists(bzdbVar))
    {
        bz_setBZDBInt(bzdbVar, value, perms, persistent);
    }
}

// Send a player a message that is stored in a vector
static void sendPluginMessage (int playerID, bool sendCustomMessage, std::vector<std::string> message, DefaultMsgType msgToSend)
{
    if (sendCustomMessage) // We want to send the players a custom message
    {
        for (std::vector<std::string>::const_iterator it = message.begin(); it != message.end(); ++it)
        {
            std::string currentLine = std::string(*it);
            bz_sendTextMessagef(BZ_SERVER, playerID, "%s", currentLine.c_str());
        }
    }
    else // Send them the default BZFS message
    {
        switch (msgToSend)
        {
            case CHAT:
                bz_sendTextMessage(BZ_SERVER, playerID, "We're sorry, you are not allowed to talk!");
                break;

            case SPAWN:
                bz_sendTextMessage(BZ_SERVER, playerID, "We're sorry, you are not allowed to spawn!");
                break;

            default:
                break;
        }
    }
}

// Output a message saying that a configuragion file option has been deprecated and won't be supported in the future
static void showDeprecatedConfigValueWarning (std::string deprecatedValue, std::string supportedValue)
{
    logMessage(0, "warning", "The '%s' configuration file option has been deprecated.", deprecatedValue.c_str());
    logMessage(0, "warning", "This warning will trigger an error in a future release of the plug-in.");
    logMessage(0, "warning", "Please use the '%s' option instead.", supportedValue.c_str());
}

// Split a string by a delimeter and return a vector of elements
static std::vector<std::string> split (std::string string, std::string delimeter)
{
    // I love you, JeffM <3
    // Thanks for tokenize()
    return tokenize(string, delimeter, 0, true);
}

// Convert a string representation of a boolean to a boolean
static bool toBool (std::string str)
{
    return !str.empty() && (strcasecmp(str.c_str (), "true") == 0 || atoi(str.c_str ()) != 0);
}

class LeagueOverseer : public bz_Plugin, public bz_CustomSlashCommandHandler, public bz_BaseURLHandler
{
public:
    virtual const char* Name (void);
    virtual void Init (const char* config);
    virtual void Event (bz_EventData *eventData);
    virtual void Cleanup (void);

    virtual int  GeneralCallback (const char* name, void* data);

    virtual bool SlashCommand (int playerID, bz_ApiString, bz_ApiString, bz_APIStringList*);

    virtual void URLDone (const char* URL, const void* data, unsigned int size, bool complete);
    virtual void URLTimeout (const char* URL, int errorCode);
    virtual void URLError (const char* URL, int errorCode, const char *errorString);

    // We will keep a record of all the players on the server to disallow first time players from automatically
    // joining a team during a match
    struct Player
    {
        std::string  bzID;

        bz_eTeamType lastActiveTeam;

        double       lastActive;

        Player (std::string _bzID, bz_eTeamType _lastActiveTeam, double _lastActive) :
            bzID(_bzID),
            lastActiveTeam(_lastActiveTeam),
            lastActive(_lastActive)
        {}
    };

    // We will be storing events that occur in the match in this struct
    struct MatchEvent
    {
        int         playerID;

        std::string json,
                    bzID,
                    message,
                    match_time;

        MatchEvent (int _playerID, std::string _bzID, std::string _message, std::string _json, std::string _match_time) :
            playerID(_playerID),
            bzID(_bzID),
            json(_json),
            message(_message),
            match_time(_match_time)
        {}
    };

    // We will be storing information about the players who participated in a match so we will
    // be storing that information inside a struct
    struct MatchParticipant
    {
        std::string  bzID,
                     callsign,
                     ipAddress,
                     teamName;

        bz_eTeamType teamColor;

        MatchParticipant (std::string _bzID, std::string _callsign, std::string _ipAddress, std::string _teamName, bz_eTeamType _teamColor) :
            bzID(_bzID),
            callsign(_callsign),
            ipAddress(_ipAddress),
            teamName(_teamName),
            teamColor(_teamColor)
        {}
    };

    // Simply out of preference, we will be storing all the information regarding a match inside
    // of a struct where the struct will be NULL if it is current a fun match
    struct OfficialMatch
    {
        bool        playersRecorded,    // Whether or not the players participating in the match have been recorded or not
                    canceled;           // Whether or not the official match was canceled

        std::string cancelationReason,  // If the match was canceled, store the reason as to why it was canceled
                    teamOneName,        // We'll be doing our best to store the team names of who's playing so store the names for
                    teamTwoName;        //     each team respectively

        double      duration,           // The length of the match in seconds. Used when reporting a match to the server
                    matchRollCall;      // The amount of seconds that need to pass in a match before the first roll call

        // We keep the number of points scored in the case where all the members of a team leave and their team
        // score get reset to 0
        int         teamOnePoints,
                    teamTwoPoints;

        // We will be storing all of the match participants in this vector
        std::vector<MatchParticipant> matchParticipants;

        // All of the events that occur in this match
        std::vector<MatchEvent> matchEvents;

        // Set the default values for this struct
        OfficialMatch () :
            playersRecorded(false),
            canceled(false),
            cancelationReason(""),
            teamOneName("Team-A"),
            teamTwoName("Team-B"),
            duration(-1.0f),
            matchRollCall(90.0),
            teamOnePoints(0),
            teamTwoPoints(0),
            matchParticipants()
        {}
    };

    virtual int         setPluginConfigInt (std::string value, int defaultValue, std::string deprecatedField = "", bool showMsg = false),
                        getMatchProgress (void);

    virtual bool        isOfficialMatchInProgress (void),
                        setPluginConfigBool (std::string value, bool defaultValue, std::string deprecatedField = "", bool showMsg = false),
                        playerAlreadyJoined (std::string bzID),
                        isMatchInProgress (void),
                        isOfficialMatch (void),
                        isLeagueMember (int playerID);

    virtual std::string getPlayerTeamNameByBZID (std::string bzID),
                        setPluginConfigString (std::string value, std::string defaultValue, std::string deprecatedField = "", bool showMsg = false),
                        getPlayerTeamNameByID (int playerID),
                        setPluginConfig (std::string value, std::string defaultValue, std::string deprecatedField = "", bool showMsg = false),
                        buildBZIDString (bz_eTeamType team),
                        getMatchTime (void);

    virtual void        validateTeamName (bool &invalidate, bool &teamError, MatchParticipant currentPlayer, std::string &teamName, bz_eTeamType team),
                        requestTeamName (std::string callsign, std::string bzID),
                        requestTeamName (bz_eTeamType team),
                        setLeagueMember (int playerID),
                        loadConfig (const char *cmdLine);


    // All the variables that will be used in the plugin
    PluginConfig PLUGIN_CONFIG;          // The configuration file used by the plugin

    bool         PC_PROTECTION_ENABLED,  // Whether or not the PC protection is enabled
                 IN_GAME_DEBUG_ENABLED,  // Whether or not the "lodgb" command is enabled
                 IS_LEAGUE_MEMBER[256],  // Whether or not the player is a registered player for the league
                 MATCH_REPORT_ENABLED,   // Whether or not to enable automatic match reports if a server is not used as an official match server
                 MOTTO_FETCH_ENABLED,    // Whether or not to set a player's motto to their team name
                 ALLOW_LIMITED_CHAT,     // Whether or not to allow limited chat functionality for non-league players
                 SPAWN_MSG_ENABLED,      // Whether or not to send custom messages explaining why players can't spawn
                 DISABLE_OFFICIALS,      // Whether or not official matches have been disabled on this server
                 TALK_MSG_ENABLED,       // Whether or not to send custom messages explaining why players can't talk
                 ROTATION_LEAGUE,        // Whether or not we are watching a league that uses different maps
                 MATCH_INFO_SENT,        // Whether or not the information returned by a URL job pertains to a match report
                 DISABLE_FMS,            // Whether or not fun matches have been disabled on this server
                 RECORDING;              // Whether or not we are recording a match

    double       LAST_CAP;               // The event time of the last flag capture

    time_t       MATCH_START,            // The timestamp of when a match was started in order to calculate the timer
                 MATCH_PAUSED;           // If the match is paused, it will be stored here in order to update MATCH_START appropriately for the timer

    int          PC_PROTECTION_DELAY,    // The delay (in seconds) of how long the PC protection will be in effect
                 VERBOSE_LEVEL,          // This is the spamming/ridiculous level of debug that the plugin uses
                 DEBUG_LEVEL;            // The DEBUG level the server owner wants the plugin to use for its messages

    std::string  SPAWN_COMMAND_PERM,     // The BZFS permission required to use the /spawn command
                 SHOW_HIDDEN_PERM,       // The BZFS permission required to use the /showhidden command
                 MATCH_REPORT_URL,       // The URL the plugin will use to report matches
                 PLUGIN_SECTION,         // The section name the plugin will read from in the configuration file
                 MAPCHANGE_PATH,         // The path to the file that contains the name of current map being played
                 TEAM_NAME_URL,          // The URL the plugin will use to fetch team information
                 LEAGUE_GROUP,           // The BZBB group that signifies membership of a league (typically in the format of <something>.LEAGUE)
                 MAP_NAME;               // The name of the map that is currently be played if it's a rotation league (i.e. OpenLeague uses multiple maps)

    bz_eTeamType CAP_VICTIM_TEAM,        // The team who had their flag captured most recently
                 CAP_WINNER_TEAM,        // The team who captured the flag most recently
                 TEAM_ONE,               // Because we're serving more than just GU league, we need to support different colors therefore, call the teams
                 TEAM_TWO;               //     ONE and TWO

    // All of the messages that will be displayed to users on certain events
    std::vector<std::string> SLASH_COMMANDS,   // The slash commands that are supported and used by this plug-in
                             NO_SPAWN_MSG,     // The message for users who can't spawn; will be sent when they try to spawn
                             NO_TALK_MSG;      // The message for users who can't talk; will be sent when they try to talk

    // The vector that is storing all of the active players
    std::vector<Player> activePlayerList;

    // This is the only pointer of the struct for the official match that we will be using. If this
    // variable is set to NULL, that means that there is currently no official match occurring.
    std::shared_ptr<OfficialMatch> officialMatch;

    // We will be using a map to handle the team name mottos in the format of
    // <BZID, Team Name>
    typedef std::map<std::string, std::string> TeamNameMottoMap;
    TeamNameMottoMap teamMottos;
};

BZ_PLUGIN(LeagueOverseer)

const char* LeagueOverseer::Name (void)
{
    static std::string pluginBuild = "";

    if (!pluginBuild.size())
    {
        std::ostringstream pluginBuildStream;

        pluginBuildStream << PLUGIN_NAME << " " << MAJOR << "." << MINOR << "." << REV << " (" << BUILD << ")";
        pluginBuild = pluginBuildStream.str();
    }

    return pluginBuild.c_str();
}

void LeagueOverseer::Init (const char* commandLine)
{
    // Register our events with Register()
    Register(bz_eAllowFlagGrab);
    Register(bz_eAllowSpawn);
    Register(bz_eCaptureEvent);
    Register(bz_eGameEndEvent);
    Register(bz_eGamePauseEvent);
    Register(bz_eGameResumeEvent);
    Register(bz_eGameStartEvent);
    Register(bz_eGetAutoTeamEvent);
    Register(bz_eGetPlayerMotto);
    Register(bz_ePlayerJoinEvent);
    Register(bz_ePlayerPartEvent);
    Register(bz_eRawChatMessageEvent);
    Register(bz_eSlashCommandEvent);
    Register(bz_eTickEvent);

    // Add all of the support slash commands so we can easily remove them in the Cleanup() function
    SLASH_COMMANDS = {"cancel", "f", "finish", "fm", "lodbg", "o", "offi", "official", "p", "pause", "r", "resume", "showhidden", "spawn", "s", "stats"};

    // Register our custom slash commands
    for (auto command : SLASH_COMMANDS)
    {
        bz_registerCustomSlashCommand(command.c_str(), this);
    }

    // Set some default values
    officialMatch = NULL;

    // Load the configuration data when the plugin is loaded
    loadConfig(commandLine);

    // Check to see if the plugin is for a rotational league
    if (MAPCHANGE_PATH != "" && ROTATION_LEAGUE)
    {
        // Open the mapchange.out file to see what map is being used
        std::ifstream infile;
        infile.open(MAPCHANGE_PATH.c_str());
        getline(infile, MAP_NAME);
        infile.close();

        // Remove the '.conf' from the mapchange.out file
        MAP_NAME = MAP_NAME.substr(0, MAP_NAME.length() - 5);

        logMessage(DEBUG_LEVEL, "debug", "Current map being played: %s", MAP_NAME.c_str());
    }

    // Assign our two team colors to eNoTeam simply so we have something to check for
    // when we are trying to find the two colors the map is using
    TEAM_ONE = eNoTeam;
    TEAM_TWO = eNoTeam;

    // Loop through all the team colors
    for (bz_eTeamType t = eRedTeam; t <= ePurpleTeam; t = (bz_eTeamType) (t + 1))
    {
        // If the current team's player limit is more than 0, that means that we found a
        // team color that the map is using
        if (bz_getTeamPlayerLimit(t) > 0)
        {
            // If team one is eNoTeam, then that means this is just the first team with player limit
            // that we have found. If it's not eNoTeam, that means we've found the second team
            if (TEAM_ONE == eNoTeam)
            {
                TEAM_ONE = t;
            }
            else if (TEAM_TWO == eNoTeam)
            {
                TEAM_TWO = t;
                break; // After we've found the second team, there's no need to continue so break out of here
            }
        }
    }

    // Make sure both teams were found, if they weren't then notify in the logs
    if (TEAM_ONE == eNoTeam || TEAM_TWO == eNoTeam)
    {
        logMessage(0, "error", "Team colors could not be detected in LeagueOverseer::Init()");
    }

    // Build the POST data for the URL job
    std::string teamNameDump = "query=teamDump&apiVersion=" + intToString(API_VERSION);
    logMessage(VERBOSE_LEVEL, "debug", "Updating Team name database...");

    // Send the team update request to the league website
    bz_addURLJob(TEAM_NAME_URL.c_str(), this, teamNameDump.c_str());

    // Create a new BZDB variable to easily set the amount of seconds team flags are protected after captures
    registerCustomIntBZDB("_pcProtectionDelay", 5);

    // Set a clip field with the full name of the plug-in for other plug-ins to know the exact name of the plug-in
    // since this plug-in has a versioning system; i.e. League Overseer X.Y.Z (r)
    bz_setclipFieldString("LeagueOverseer", Name());



    ///
    /// Callback testing and examples
    ///

    // Check if the 'LeagueOverseer' clipfield exists which means the League Overseer plug-in is loaded
    if (bz_clipFieldExists("LeagueOverseer"))
    {
        // We have found the clip field so we can access the plug-in's callbacks
        bz_debugMessage(0, "'LeagueOverseer' clip field found meaning the League Overseer plug-in is loaded.");

        // Get the name from the clip field and save it for easy access
        std::string leagueOverseerPluginName = bz_getclipFieldString("LeagueOverseer");


        ///
        /// Get the current match time
        ///

        char currentMatchTime[256] = {0};
        int matchTimeReturnValue = bz_callPluginGenericCallback(leagueOverseerPluginName.c_str(), "GetMatchTime", currentMatchTime);

        // When calling the 'GetMatchTime' callback, 1 will be returned if getting the current match time was successful
        if (matchTimeReturnValue == 1)
        {
            std::string matchTime = currentMatchTime;

            // The 'GetMatchTime' callback will give a value of '-00:00' if there is no match in progress
            if (matchTime == "-00:00")
            {
                bz_debugMessagef(0, "There is currently no match in progress.");
            }
            else
            {
                bz_debugMessagef(0, "The current match time is: %s", currentMatchTime);
            }
        }
        else
        {
            bz_debugMessage(0, "The current match time could not be returned for an unknown reason.");
        }
    }
}

void LeagueOverseer::Cleanup (void)
{
    // Clean up all the events
    Flush();

    // Clean up our custom slash commands
    for (auto command : SLASH_COMMANDS)
    {
        bz_removeCustomSlashCommand(command.c_str());
    }
}

int LeagueOverseer::GeneralCallback (const char* name, void* data)
{
    logMessage(VERBOSE_LEVEL, "callback", "A plug-in has requested the '%s' callback.", name);

    // Store the callback that is being called for easy access
    std::string callbackOption = name;

    if (callbackOption == "GetMatchProgress")
    {
        int matchProgress = getMatchProgress();

        logMessage(VERBOSE_LEVEL, "callback", "Returning the current match progress (%d)...", matchProgress);
        return matchProgress;
    }
    else if (callbackOption == "GetMatchTime")
    {
        std::string matchTime = getMatchTime();

        strcpy((char*)data, matchTime.c_str());

        logMessage(VERBOSE_LEVEL, "callback", "Returning the current match time (%s)...", matchTime.c_str());
        return 1;
    }
    else if (callbackOption == "IsOfficialMatch")
    {
        bool isOfficial = isOfficialMatch();

        logMessage(VERBOSE_LEVEL, "callback", "Returning that an match is %sin progress.", (isOfficial) ? "" : "not ");
        return (int)isOfficial;
    }

    logMessage(VERBOSE_LEVEL, "callback", "The '%s' callback was not found.", name);
    return 0;
}

void LeagueOverseer::Event (bz_EventData *eventData)
{
    switch (eventData->eventType)
    {
        case bz_eAllowFlagGrab: // This event is called each time a player attempts to grab a flag
        {
            bz_AllowFlagGrabData_V1* allowFlagGrabData = (bz_AllowFlagGrabData_V1*)eventData;

            // Data
            // ---
            //    (int)          playerID  - The ID of the player who is grabbing the flag
            //    (int)          flagID    - The ID of the flag that is going to be grabbed
            //    (const char*)  flagType  - The type of the flag about to be grabbed
            //    (bool)         allow     - Whether or not to allow the flag grab
            //    (double)       eventTime - The server time at which the event occurred (in seconds).

            std::string              flagAbbr          = allowFlagGrabData->flagType;
            int                      playerID          = allowFlagGrabData->playerID;

            if (PC_PROTECTION_ENABLED) // Is the server configured to protect against Pass Camping
            {
                // Check if the last capture was within the 'PC_PROTECTION_DELAY' amount of seconds
                if (LAST_CAP + PC_PROTECTION_DELAY > bz_getCurrentTime())
                {
                    // Check to see if the flag being grabbed belongs to the team that just had their flag captured AND check
                    // to see if someone not from the victim team grabbed it
                    if ((getTeamTypeFromFlag(flagAbbr) == CAP_VICTIM_TEAM && bz_getPlayerTeam(playerID) != CAP_VICTIM_TEAM))
                    {
                        // Disallow the flag grab if it's being grabbed by an enemy right after a flag capture
                        allowFlagGrabData->allow = false;
                    }
                }
            }
        }
        break;

        case bz_eAllowSpawn: // This event is called before a player respawns
        {
            bz_AllowSpawnData_V1* allowSpawnData = (bz_AllowSpawnData_V1*)eventData;

            // Data
            // ---
            //    (int)           playerID  - This value is the player ID for the joining player.
            //    (bz_eTeamType)  team      - The team the player belongs to.
            //    (bool)          handled   - Whether or not the plugin will be handling the respawn or not.
            //    (bool)          allow     - Set to false if the player should not be allowed to spawn.
            //    (double)        eventTime - The server time the event occurred (in seconds.)

            int playerID = allowSpawnData->playerID;

            if (!isLeagueMember(playerID)) // Is the player not part of the league?
            {
                // Disable their spawning privileges
                allowSpawnData->handled = true;
                allowSpawnData->allow   = false;

                // Send the player a message, either default or custom based on 'SPAWN_MSG_ENABLED'
                sendPluginMessage(playerID, SPAWN_MSG_ENABLED, NO_SPAWN_MSG, SPAWN);
            }
        }
        break;

        case bz_eCaptureEvent: // This event is called each time a team's flag has been captured
        {
            // We only need to keep track of the store if it's an official match
            if (officialMatch != NULL)
            {
                bz_CTFCaptureEventData_V1* captureData = (bz_CTFCaptureEventData_V1*)eventData;

                // Data
                // ---
                //    (bz_eTeamType)  teamCapped    - The team whose flag was captured.
                //    (bz_eTeamType)  teamCapping   - The team who did the capturing.
                //    (int)           playerCapping - The player who captured the flag.
                //    (float[3])      pos           - The world position(X,Y,Z) where the flag has been captured
                //    (float)         rot           - The rotational orientation of the capturing player
                //    (double)        eventTime     - This value is the local server time of the event.

                (captureData->teamCapping == TEAM_ONE) ? officialMatch->teamOnePoints++ : officialMatch->teamTwoPoints++;

                // Log the information about the current score to the logs at the verbose level
                logMessage(VERBOSE_LEVEL, "debug", "%s team scored.", formatTeam(captureData->teamCapping).c_str());
                logMessage(VERBOSE_LEVEL, "debug", "Official Match Score %s [%i] vs %s [%i]",
                    formatTeam(TEAM_ONE).c_str(), officialMatch->teamOnePoints,
                    formatTeam(TEAM_TWO).c_str(), officialMatch->teamTwoPoints);

                CAP_VICTIM_TEAM = captureData->teamCapped;
                CAP_WINNER_TEAM = captureData->teamCapping;
                LAST_CAP        = captureData->eventTime;

                // Create a player record of the person who captured the flag
                std::shared_ptr<bz_BasePlayerRecord> playerData(bz_getPlayerByIndex(captureData->playerCapping));

                // Create a MatchEvent with the information relating to the capture
                MatchEvent capEvent(playerData->playerID, std::string(playerData->bzID.c_str()),
                                     std::string(playerData->callsign.c_str()) + " captured the " + formatTeam(captureData->teamCapped) + " flag",
                                     "{\"event\": {\"type\": \"capture\", \"color\": \"" + formatTeam(captureData->teamCapped) + "\"}}",
                                     getMatchTime());

                // Push the MatchEvent to the matchEvents vector stored in the officialMatch struct
                officialMatch->matchEvents.push_back(capEvent);
            }
        }
        break;

        case bz_eGameEndEvent: // This event is called each time a game ends
        {
            bz_debugMessage(VERBOSE_LEVEL, "A match has ended.");

            // Grant the "poll" perm when the match is over
            grantPermToAll("poll");

            // Get the current standard UTC time
            bz_Time standardTime;
            bz_getUTCtime(&standardTime);
            std::string recordingFileName;

            // Only save the recording buffer if we actually started recording when the match started
            if (RECORDING)
            {
                logMessage(VERBOSE_LEVEL, "debug", "Recording was in progress during the match.");

                // We'll be formatting the file name, so create a variable to store it
                char tempRecordingFileName[512];

                // Let's get started with formatting
                if (officialMatch != NULL)
                {
                    // If the official match was finished, then mark it as canceled
                    std::string matchCanceled = (officialMatch->canceled) ? "-Canceled" : "",
                                _teamOneName  = officialMatch->teamOneName.c_str(),
                                _teamTwoName  = officialMatch->teamTwoName.c_str(),
                                _matchTeams   = "";

                    // We want to standardize the names, so replace all spaces with underscores and
                    // any weird HTML symbols should have been stripped already by the PHP script
                    std::replace(_teamOneName.begin(), _teamOneName.end(), ' ', '_');
                    std::replace(_teamTwoName.begin(), _teamTwoName.end(), ' ', '_');

                    if (!officialMatch->matchParticipants.empty())
                    {
                        _matchTeams = _teamOneName + "-vs-" + _teamTwoName + "-";
                    }

                    sprintf(tempRecordingFileName, "offi-%d%02d%02d-%s%02d%02d%s.rec",
                        standardTime.year, standardTime.month, standardTime.day, _matchTeams.c_str(),
                        standardTime.hour, standardTime.minute, matchCanceled.c_str());
                }
                else
                {
                    sprintf(tempRecordingFileName, "fun-%d%02d%02d-%02d%02d.rec",
                        standardTime.year, standardTime.month, standardTime.day,
                        standardTime.hour, standardTime.minute);
                }

                // Move the char[] into a string to handle it better
                recordingFileName = tempRecordingFileName;
                logMessage(VERBOSE_LEVEL, "debug", "Replay file will be named: %s", recordingFileName.c_str());

                // Save the recording buffer and stop recording
                bz_saveRecBuf(recordingFileName.c_str(), 0);
                bz_stopRecBuf();
                logMessage(VERBOSE_LEVEL, "debug", "Replay file has been saved and recording has stopped.");

                // We're no longer recording, so set the boolean and announce to players that the file has been saved
                RECORDING = false;
                bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Match saved as: %s", recordingFileName.c_str());
            }

            if (MATCH_REPORT_ENABLED)
            {
                if (officialMatch == NULL)
                {
                    // It was a fun match, so there is no need to do anything

                    logMessage(DEBUG_LEVEL, "debug", "Fun match has completed.");
                }
                else if (officialMatch->canceled)
                {
                    // The match was canceled for some reason so output the reason to both the players and the server logs

                    logMessage(DEBUG_LEVEL, "debug", "%s", officialMatch->cancelationReason.c_str());
                    bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, officialMatch->cancelationReason.c_str());
                }
                else if (officialMatch->matchParticipants.empty())
                {
                    // Oops... I darn goofed. Somehow the players were not recorded properly

                    logMessage(DEBUG_LEVEL, "debug", "No recorded players for this official match.");
                    bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Official match could not be reported due to not having a list of valid match participants.");
                }
                else
                {
                    // This was an official match, so let's report it

                    // Format the date to -> year-month-day hour:minute:second
                    char matchDate[20];
                    sprintf(matchDate, "%02d-%02d-%02d %02d:%02d:%02d", standardTime.year, standardTime.month, standardTime.day, standardTime.hour, standardTime.minute, standardTime.second);

                    // Keep references to values for quick reference
                    std::string teamOnePointsFinal = intToString(officialMatch->teamOnePoints);
                    std::string teamTwoPointsFinal = intToString(officialMatch->teamTwoPoints);
                    std::string matchDuration      = intToString(officialMatch->duration/60);

                    // Store match data in the logs
                    bz_debugMessagef(0, "Match Data :: League Overseer Match Report");
                    bz_debugMessagef(0, "Match Data :: -----------------------------");
                    bz_debugMessagef(0, "Match Data :: Match Time      : %s", matchDate);
                    bz_debugMessagef(0, "Match Data :: Duration        : %s", matchDuration.c_str());
                    bz_debugMessagef(0, "Match Data :: %s  Score  : %s", formatTeam(TEAM_ONE, true).c_str(), teamOnePointsFinal.c_str());
                    bz_debugMessagef(0, "Match Data :: %s  Score  : %s", formatTeam(TEAM_TWO, true).c_str(), teamTwoPointsFinal.c_str());

                    // Start building POST data to be sent to the league website
                    std::string matchToSend = "query=reportMatch";
                                matchToSend += "&apiVersion="  + std::string(bz_urlEncode(intToString(API_VERSION).c_str()));
                                matchToSend += "&teamOneWins=" + std::string(bz_urlEncode(teamOnePointsFinal.c_str()));
                                matchToSend += "&teamTwoWins=" + std::string(bz_urlEncode(teamTwoPointsFinal.c_str()));
                                matchToSend += "&duration="    + std::string(bz_urlEncode(matchDuration.c_str()));
                                matchToSend += "&matchTime="   + std::string(bz_urlEncode(matchDate));
                                matchToSend += "&server="      + std::string(bz_urlEncode(bz_getPublicAddr().c_str()));
                                matchToSend += "&port="        + std::string(bz_urlEncode(intToString(bz_getPublicPort()).c_str()));
                                matchToSend += "&replayFile="  + std::string(bz_urlEncode(recordingFileName.c_str()));

                    // Only add this parameter if it's a rotational league such as OpenLeague
                    if (ROTATION_LEAGUE)
                    {
                        matchToSend += "&mapPlayed=" + std::string(bz_urlEncode(MAP_NAME.c_str()));
                    }

                    // Build a string of BZIDs and also output the BZIDs to the server logs while we're at it
                    matchToSend += "&teamOnePlayers=" + buildBZIDString(TEAM_ONE);
                    matchToSend += "&teamTwoPlayers=" + buildBZIDString(TEAM_TWO);

                    // Finish prettifying the server logs
                    bz_debugMessagef(0, "Match Data :: -----------------------------");
                    bz_debugMessagef(0, "Match Data :: End of Match Report");
                    logMessage(DEBUG_LEVEL, "debug", "Reporting match data...");
                    bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Reporting match...");

                    // Send the match data to the league website
                    bz_addURLJob(MATCH_REPORT_URL.c_str(), this, matchToSend.c_str());
                    MATCH_INFO_SENT = true;
                }
            }

            // We're done with the struct, so make it NULL until the next official match
            officialMatch = NULL;

            // Empty our list of players since we don't need a history
            activePlayerList.clear();
        }
        break;

        case bz_eGamePauseEvent:
        {
            bz_GamePauseResumeEventData_V1* gamePauseData = (bz_GamePauseResumeEventData_V1*)eventData;

            // Data
            // ---
            //    (bz_ApiString) actionBy  - The callsign of whoever triggered the event. By default, it's "SERVER"
            //    (double)       eventTime - The server time the event occurred (in seconds).

            // Get the current UTC time
            MATCH_PAUSED = time(NULL);

            // We've paused an official match, so we need to delay the approxTimeProgress in order to calculate the roll call time properly
            if (officialMatch != NULL)
            {
                // Grant the "poll" perm while a match is paused
                grantPermToAll("poll");

                // Send the messages
                bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "    with %s remaining.", getMatchTime().c_str());
                logMessage(VERBOSE_LEVEL, "debug", "Match paused at %s by %s.", getMatchTime().c_str(), gamePauseData->actionBy.c_str());

                // Create a player record of the person who captured the flag
                std::shared_ptr<bz_BasePlayerRecord> playerData(bz_getPlayerByCallsign(gamePauseData->actionBy.c_str()));

                // Create a MatchEvent with the information relating to the capture
                MatchEvent pauseEvent(playerData->playerID, std::string(playerData->bzID.c_str()),
                                     std::string(playerData->callsign.c_str()) + " paused the match at " + getMatchTime(),
                                     "{\"event\": {\"type\": \"pause\"}}",
                                     getMatchTime());

                // Push the MatchEvent to the matchEvents vector stored in the officialMatch struct
                officialMatch->matchEvents.push_back(pauseEvent);
            }
        }
        break;

        case bz_eGameResumeEvent:
        {
            bz_GamePauseResumeEventData_V1* gameResumeData = (bz_GamePauseResumeEventData_V1*)eventData;

            // Data
            // ---
            //    (bz_ApiString) actionBy  - The callsign of whoever triggered the event. By default, it's "SERVER"
            //    (double)       eventTime - The server time the event occurred (in seconds).


            // Get the current UTC time
            time_t now = time(NULL);

            // Do the math to determine how long the match was paused
            double timePaused = difftime(now, MATCH_PAUSED);

            // Create a temporary variable to store and manipulate the match start time
            struct tm modMatchStart = *localtime(&MATCH_START);

            // Manipulate the time by adding the amount of seconds the match was paused in order to be able to accurately
            // calculate the amount of time remaining with LeagueOverseer::getMatchTime()
            modMatchStart.tm_sec += timePaused;

            // Save the manipulated match start time
            MATCH_START = mktime(&modMatchStart);
            logMessage(VERBOSE_LEVEL, "debug", "Match paused for %.f seconds. Match continuing at %s.", timePaused, getMatchTime().c_str());

            // We've resumed an official match, so we need to properly edit the start time so we can calculate the roll call
            if (officialMatch != NULL)
            {
                // Revoke the "poll" perm while a match is active
                revokePermFromAll("poll");

                // Create a player record of the person who captured the flag
                std::shared_ptr<bz_BasePlayerRecord> playerData(bz_getPlayerByCallsign(gameResumeData->actionBy.c_str()));

                // Create a MatchEvent with the information relating to the capture
                MatchEvent resumeEvent(playerData->playerID, std::string(playerData->bzID.c_str()),
                                     std::string(playerData->callsign.c_str()) + " resumed the match",
                                     "{\"event\": {\"type\": \"resume\"}}",
                                     getMatchTime());

                // Push the MatchEvent to the matchEvents vector stored in the officialMatch struct
                officialMatch->matchEvents.push_back(resumeEvent);
            }
        }
        break;

        case bz_eGameStartEvent: // This event is triggered when a timed game begins
        {
            logMessage(VERBOSE_LEVEL, "debug", "A match has started");

            // Empty our list of players since we don't need a history
            activePlayerList.clear();

            // We started recording a match, so save the status
            RECORDING = bz_startRecBuf();

            // We want to notify the logs if we couldn't start recording just in case an issue were to occur and the server
            // owner needs to check to see if players were lying about there no replay
            if (RECORDING)
            {
                logMessage(VERBOSE_LEVEL, "debug", "Match recording has started successfully");
            }
            else
            {
                logMessage(0, "error", "This match could not be recorded");
            }

            // Check if this is an official match
            if (officialMatch != NULL)
            {
                // Revoke the "poll" perm while a match is active
                revokePermFromAll("poll");

                // Reset scores in case Caps happened during countdown delay.
                officialMatch->teamOnePoints = officialMatch->teamTwoPoints = 0;
                officialMatch->duration = bz_getTimeLimit();
            }

            MATCH_START = time(NULL);
        }
        break;

        case bz_eGetAutoTeamEvent: // This event is called for each new player is added to a team
        {
            bz_GetAutoTeamEventData_V1* autoTeamData = (bz_GetAutoTeamEventData_V1*)eventData;

            // Data
            // ---
            //    (int)           playerID  - ID of the player that is being added to the game.
            //    (bz_ApiString)  callsign  - Callsign of the player that is being added to the game.
            //    (bz_eTeamType)  team      - The team that the player will be added to. Initialized to the team chosen by the
            //                                current server team rules, or the effects of a plug-in that has previously processed
            //                                the event. Plug-ins wishing to override the team should set this value.
            //    (bool)          handled   - The current state representing if other plug-ins have modified the default team.
            //                                Plug-ins that modify the team should set this value to true to inform other plug-ins
            //                                that have not processed yet.
            //    (double)        eventTime - This value is the local server time of the event.

            int playerID = autoTeamData->playerID;
            std::shared_ptr<bz_BasePlayerRecord> playerData(bz_getPlayerByIndex(playerID));

            // Only force new players to observer if a match is in progress
            if (isMatchInProgress())
            {
                // Automatically move non-league members or players who just joined to the observer team
                if (!isLeagueMember(playerID) || !playerAlreadyJoined(playerData->bzID.c_str()))
                {
                    autoTeamData->handled = true;
                    autoTeamData->team    = eObservers;
                }
            }
        }
        break;

        case bz_eGetPlayerMotto: // This event is called when the player joins. It gives us the motto of the player
        {
            bz_GetPlayerMottoData_V2* mottoData = (bz_GetPlayerMottoData_V2*)eventData;

            // Data
            // ---
            //    (bz_ApiString)         motto     - The motto of the joining player. This value may ve overwritten to change the motto of a player.
            //    (bz_BasePlayerRecord)  record    - The player record for the player using the motto.
            //    (double)               eventTime - The server time the event occurred (in seconds).

            if (MOTTO_FETCH_ENABLED)
            {
                mottoData->motto = getPlayerTeamNameByBZID(mottoData->record->bzID.c_str());
            }
        }
        break;

        case bz_ePlayerJoinEvent: // This event is called each time a player joins the game
        {
            bz_PlayerJoinPartEventData_V1* joinData = (bz_PlayerJoinPartEventData_V1*)eventData;

            // Data
            // ---
            //    (int)                   playerID  - The player ID that is joining
            //    (bz_BasePlayerRecord*)  record    - The player record for the joining player
            //    (double)                eventTime - Time of event.

            int playerID = joinData->playerID;
            std::shared_ptr<bz_BasePlayerRecord> playerData(bz_getPlayerByIndex(playerID));

            setLeagueMember(playerID);

            // Only notify a player if they exist, have joined the observer team, and there is a match in progress
            if (isMatchInProgress() && isValidPlayerID(joinData->playerID) && playerData->team == eObservers)
            {
                bz_sendTextMessagef(BZ_SERVER, joinData->playerID, "*** There is currently %s match in progress, please be respectful. ***",
                                    ((officialMatch != NULL) ? "an official" : "a fun"));
            }

            if (MOTTO_FETCH_ENABLED)
            {
                // Only send a URL job if the user is verified
                if (playerData->verified)
                {
                    requestTeamName(playerData->callsign.c_str(), playerData->bzID.c_str());
                }
            }
        }
        break;

        case bz_ePlayerPartEvent: // This event is called each time a player leaves a game
        {
            bz_PlayerJoinPartEventData_V1* partData = (bz_PlayerJoinPartEventData_V1*)eventData;

            // Data
            // ---
            //    (int)                   playerID  - The player ID that is leaving
            //    (bz_BasePlayerRecord*)  record    - The player record for the leaving player
            //    (bz_ApiString)          reason    - The reason for leaving, such as a kick or a ban
            //    (double)                eventTime - Time of event.

            int playerID = partData->playerID;
            std::shared_ptr<bz_BasePlayerRecord> playerData(bz_getPlayerByIndex(playerID));

            // Only keep track of the parting player if they are a league member and there is a match in progress
            if (isLeagueMember(playerID) && isMatchInProgress())
            {
                if (!playerAlreadyJoined(playerData->bzID.c_str()))
                {
                    // Create a record for the player who just left
                    Player partingPlayer(playerData->bzID.c_str(), playerData->team, bz_getCurrentTime());

                    // Push the record to our vector
                    activePlayerList.push_back(partingPlayer);
                }
            }
        }
        break;

        case bz_eRawChatMessageEvent: // This event is called for each chat message the server receives. It is called before any filtering is done.
        {
            bz_ChatEventData_V1* chatData  = (bz_ChatEventData_V1*)eventData;

            // Data
            // ---
            //    (int)           from      - The player ID sending the message.
            //    (int)           to        - The player ID that the message is to if the message is to an individual, or a
            //                                broadcast. If the message is a broadcast the id will be BZ_ALLUSERS.
            //    (bz_eTeamType)  team      - The team the message is for if it not for an individual or a broadcast. If it
            //                                is not a team message the team will be eNoTeam.
            //    (bz_ApiString)  message   - The filtered final text of the message.
            //    (double)        eventTime - The time of the event.

            bz_eTeamType target    = chatData->team;
            int          playerID  = chatData->from,
                         recipient = chatData->to;

            // A non-league player is attempting to talk
            if (!isLeagueMember(playerID))
            {
                if (ALLOW_LIMITED_CHAT) // Are non-league members allowed limited talking functionality
                {
                    if (isMatchInProgress()) // A match is progress
                    {
                        if (bz_getPlayerTeam(playerID) == eObservers) // Only consider allowing non-league members to talk if they are in the observer team
                        {
                            // Only allow non-league members to talk if they're talking to the observer team chat or private messaging a player in the observer team
                            // this precaution is so non-league players do not private message players participating in a match, do not message an admin who may
                            // playing a match, and do not send messages to public chat to avoid match disturbances
                            if (target != eObservers || bz_getPlayerTeam(recipient) != eObservers)
                            {
                                chatData->message = ""; // We set the message to nothing so they won't send thing anything
                                sendPluginMessage(playerID, TALK_MSG_ENABLED, NO_TALK_MSG, CHAT);
                            }
                        }
                        else // If they aren't in the observer team during a match, don't let them talk
                        {
                            chatData->message = "";
                            sendPluginMessage(playerID, TALK_MSG_ENABLED, NO_TALK_MSG, CHAT);
                        }
                    }
                    else // A match is not in progress
                    {
                        if (target != eAdministrators)
                        {
                            chatData->message = "";
                            sendPluginMessage(playerID, TALK_MSG_ENABLED, NO_TALK_MSG, CHAT);
                        }
                    }
                }
                else // Non-league members are not allowed limited talk functionality
                {
                    chatData->message = "";
                    sendPluginMessage(playerID, TALK_MSG_ENABLED, NO_TALK_MSG, CHAT); // Send them a message
                }
            }
        }
        break;

        case bz_eSlashCommandEvent: // This event is called each time a player sends a slash command
        {
            bz_SlashCommandEventData_V1* slashCommandData = (bz_SlashCommandEventData_V1*)eventData;

            // Data
            // ---
            //    (int)           from      - The player who sent the slash command
            //    (bz_ApiString)  message   - The full text of the chat message for the slash command, containing the command and all associated parameters
            //    (double)        eventTime - The local server time of the event

            // Store the information in variables for quick reference
            int         playerID = slashCommandData->from;
            std::string command  = slashCommandData->message.c_str();

            // Because players have quick keys and players of habit, send them a notification in the case they
            // use a deprecated slash command
            if (strncmp("/gameover", command.c_str(), 9) == 0)
            {
                bz_sendTextMessagef(BZ_SERVER, playerID, "** '/gameover' is disabled, please use /finish or /cancel instead **");
            }
            else if (strncmp("/countdown pause", command.c_str(), 16) == 0)
            {
                bz_sendTextMessagef(BZ_SERVER, playerID, "** '/countdown pause' is disabled, please use /pause instead **");
            }
            else if (strncmp("/countdown resume", command.c_str(), 17) == 0)
            {
                bz_sendTextMessagef(BZ_SERVER, playerID, "** '/countdown resume' is disabled, please use /resume instead **");
            }
            else if (strncmp("/countdown", command.c_str(), 10) == 0)
            {
                bz_sendTextMessage(BZ_SERVER, playerID, "** '/countdown TIME' is disabled, please use /official or /fm instead **");
            }
            else if (strncmp("/poll", command.c_str(), 5) == 0 && isOfficialMatchInProgress())
            {
                bz_sendTextMessage(BZ_SERVER, playerID, "** '/poll' is disabled during official matches. Please /pause the match in order to start a poll. **");
            }
        }
        break;

        case bz_eTickEvent: // This event is called once for each BZFS main loop
        {
            // Get the total number of tanks playing
            int totaltanks = bz_getTeamCount(eRedTeam) + bz_getTeamCount(eGreenTeam) + bz_getTeamCount(eBlueTeam) + bz_getTeamCount(ePurpleTeam);

            // If there are no tanks playing, then we need to do some clean up
            if (totaltanks == 0)
            {
                // If there is an official match and no tanks playing, we need to cancel it
                if (officialMatch != NULL)
                {
                    officialMatch->canceled = true;
                    officialMatch->cancelationReason = "Official match automatically canceled due to all players leaving the match.";
                }

                // If we have players recorded and there's no one around, empty the list
                if (!activePlayerList.empty())
                {
                    activePlayerList.clear();
                }

                // If there is a countdown active an no tanks are playing, then cancel it
                if (bz_isCountDownActive())
                {
                    bz_gameOver(253, eObservers);
                    logMessage(VERBOSE_LEVEL, "debug", "Game ended because no players were found playing with an active countdown.");
                }
            }

            // Let's get the roll call only if there is an official match
            if (officialMatch != NULL)
            {
                // Check if the start time is not negative since our default value for the approxTimeProgress is -1. Also check
                // if it's time to do a roll call, which is defined as 90 seconds after the start of the match by default,
                // and make sure we don't have any match participants recorded and the match isn't paused
                if (getMatchProgress() > officialMatch->matchRollCall && officialMatch->matchParticipants.empty() &&
                    !bz_isCountDownPaused() && !bz_isCountDownInProgress())
                {
                    logMessage(VERBOSE_LEVEL, "debug", "Processing roll call...");

                    std::shared_ptr<bz_APIIntList> playerList(bz_getPlayerIndexList());
                    bool invalidateRollcall, teamOneError, teamTwoError;
                    std::string teamOneMotto, teamTwoMotto;

                    invalidateRollcall = teamOneError = teamTwoError = false;
                    teamOneMotto = teamTwoMotto = "";

                    // We can't do a roll call if the player list wasn't created
                    if (!playerList)
                    {
                        logMessage(VERBOSE_LEVEL, "error", "Failure to create player list for roll call.");
                        return;
                    }

                    for (unsigned int i = 0; i < playerList->size(); i++)
                    {
                        std::shared_ptr<bz_BasePlayerRecord> playerRecord(bz_getPlayerByIndex(playerList->get(i)));

                        if (playerRecord && isLeagueMember(playerRecord->playerID) && bz_getPlayerTeam(playerList->get(i)) != eObservers) // If player is not an observer
                        {
                            MatchParticipant currentPlayer(playerRecord->bzID.c_str(), playerRecord->callsign.c_str(),
                                                           playerRecord->ipAddress.c_str(), teamMottos[playerRecord->bzID.c_str()],
                                                           playerRecord->team);

                            // In order to see what is going wrong with the roll call if anything, display all of the player's information
                            logMessage(VERBOSE_LEVEL, "debug", "Adding player '%s' to roll call...", currentPlayer.callsign.c_str());
                            logMessage(VERBOSE_LEVEL, "debug", "  >  BZID       : %s", currentPlayer.bzID.c_str());
                            logMessage(VERBOSE_LEVEL, "debug", "  >  IP Address : %s", currentPlayer.ipAddress.c_str());
                            logMessage(VERBOSE_LEVEL, "debug", "  >  Team Name  : %s", currentPlayer.teamName.c_str());
                            logMessage(VERBOSE_LEVEL, "debug", "  >  Team Color : %s", formatTeam(currentPlayer.teamColor).c_str());

                            // Check if there is any need to invalidate a roll call from a team
                            validateTeamName(invalidateRollcall, teamOneError, currentPlayer, teamOneMotto, TEAM_ONE);
                            validateTeamName(invalidateRollcall, teamTwoError, currentPlayer, teamTwoMotto, TEAM_TWO);

                            if (currentPlayer.bzID.empty()) // Someone is playing without a BZID, how did this happen?
                            {
                                invalidateRollcall = true;
                                logMessage(VERBOSE_LEVEL, "error", "Roll call has been marked as invalid due to '%s' not having a valid BZID.", currentPlayer.callsign.c_str());
                            }

                            // Add the player to the struct of participants
                            officialMatch->matchParticipants.push_back(currentPlayer);
                            logMessage(VERBOSE_LEVEL, "debug", "Player '%s' successfully added to the roll call.", currentPlayer.callsign.c_str());
                        }
                    }

                    // We were asked to invalidate the roll call because of some issue so let's check if there is still time for
                    // another roll call
                    if (invalidateRollcall && officialMatch->matchRollCall + 60 < officialMatch->duration)
                    {
                        logMessage(DEBUG_LEVEL, "debug", "Invalid player found on field at %s.", getMatchTime().c_str());

                        // There was an error with one of the members of either team, so request a team name update for all of
                        // the team members to try to fix any inconsistencies of different team names
                        if (teamOneError) { requestTeamName(TEAM_ONE); }
                        if (teamTwoError) { requestTeamName(TEAM_TWO); }

                        // Delay the next roll call by 60 seconds
                        officialMatch->matchRollCall += 60;
                        logMessage(VERBOSE_LEVEL, "debug", "Match roll call time has been delayed by 60 seconds.");

                        // Clear the struct because it's useless data
                        officialMatch->matchParticipants.clear();
                        logMessage(VERBOSE_LEVEL, "debug", "Match participants have been cleared.");
                    }

                    // There is no need to invalidate the roll call so the team names must be right so save them in the struct
                    if (!invalidateRollcall)
                    {
                        officialMatch->teamOneName = teamOneMotto;
                        officialMatch->teamTwoName = teamTwoMotto;

                        logMessage(VERBOSE_LEVEL, "debug", "Team One set to: %s", officialMatch->teamOneName.c_str());
                        logMessage(VERBOSE_LEVEL, "debug", "Team Two set to: %s", officialMatch->teamTwoName.c_str());
                    }
                }
            }
        }
        break;

        default: break;
    }
}

bool LeagueOverseer::SlashCommand (int playerID, bz_ApiString command, bz_ApiString /*message*/, bz_APIStringList *params)
{
    std::shared_ptr<bz_BasePlayerRecord> playerData(bz_getPlayerByIndex(playerID));

    // For some reason, the player record could not be created
    if (!playerData)
    {
        return true;
    }

    // If the player is not verified and does not have the spawn permission, they can't use any of the commands
    if (!playerData->verified || !isLeagueMember(playerID))
    {
        bz_sendTextMessagef(BZ_SERVER, playerID, "You do not have permission to run the /%s command.", command.c_str());
        return true;
    }

    if (command == "cancel")
    {
        if (playerData->team == eObservers) // Observers can't cancel matches
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "Observers are not allowed to cancel matches.");
        }
        else if (bz_isCountDownInProgress()) // There's no way to stop a countdown so let's not cancel during a countdown
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "You may only cancel a match after it has started.");
        }
        else if (bz_isCountDownActive()) // We can only cancel a match if the countdown is active
        {
            // We're canceling an official match
            if (officialMatch != NULL)
            {
                officialMatch->canceled = true;
                officialMatch->cancelationReason = "Official match cancellation requested by " + std::string(playerData->callsign.c_str());
            }
            else // Cancel the fun match like normal
            {
                bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Fun match ended by %s", playerData->callsign.c_str());
            }

            logMessage(DEBUG_LEVEL, "debug", "Match ended by %s (%s).", playerData->callsign.c_str(), playerData->ipAddress.c_str());
            bz_gameOver(253, eObservers);
        }
        else
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "There is no match in progress to cancel.");
        }

        return true;
    }
    else if (command == "finish")
    {
        if (playerData->team == eObservers) // Observers can't cancel matches
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "Observers are not allowed to cancel matches.");
        }
        else if (bz_isCountDownInProgress()) // There's no way to stop a countdown so let's not finish during a countdown
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "You may only finish a match after it has started.");
        }
        else if (bz_isCountDownActive()) // Only finish if the countdown is active
        {
            // We can only '/finish' official matches because I wanted to have a command only dedicated to
            // reporting partially completed matches
            if (officialMatch != NULL)
            {
                // Let's check if we can report the match, in other words, at least half of the match has been reported
                if (getMatchProgress() >= officialMatch->duration / 2)
                {
                    bz_debugMessagef(DEBUG_LEVEL, "DEBUG :: Match Over Seer :: Official match ended early by %s (%s)", playerData->callsign.c_str(), playerData->ipAddress.c_str());
                    bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Official match ended early by %s", playerData->callsign.c_str());

                    bz_gameOver(253, eObservers);
                }
                else
                {
                    bz_sendTextMessage(BZ_SERVER, playerID, "Sorry, I cannot automatically report a match less than half way through.");
                    bz_sendTextMessage(BZ_SERVER, playerID, "Please use the /cancel command and message a referee for review of this match.");
                }
            }
            else
            {
                bz_sendTextMessage(BZ_SERVER, playerID, "You cannot /finish a fun match. Use /cancel instead.");
            }
        }
        else
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "There is no match in progress to end.");
        }

        return true;
    }
    else if (command == "f" || command == "fm")
    {
        if (DISABLE_FMS)
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "Sorry, this server has not be configured for fun matches.");
        }
        else if (playerData->team == eObservers) // Observers can't start matches
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "Observers are not allowed to start matches.");
        }
        else if (officialMatch != NULL || bz_isCountDownActive() || bz_isCountDownInProgress()) // There is already a countdown
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "There is already a game in progress; you cannot start another.");
        }
        else // They are verified, not an observer, there is no match. So start one
        {
            // We signify an FM whenever the 'officialMatch' variable is set to NULL so set it to null
            officialMatch = NULL;

            // Log the actions
            logMessage(DEBUG_LEVEL, "debug", "Fun match started by %s (%s).", playerData->callsign.c_str(), playerData->ipAddress.c_str());
            bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Fun match started by %s.", playerData->callsign.c_str());

            // The amount of seconds the countdown should take
            int timeToStart = (params->size() == 1) ? atoi(params->get(0).c_str()) : 10;

            // Sanity check...
            if (timeToStart <= 60 && timeToStart >= 10)
            {
                bz_startCountdown(timeToStart, bz_getTimeLimit(), "Server"); // Start the countdown with a custom countdown time limit under 1 minute
            }
            else
            {
                bz_sendTextMessage(BZ_SERVER, playerID, "Holy sanity check, Batman! Let's not have a countdown last longer than 60 seconds or less than 10.");
                bz_startCountdown(10, bz_getTimeLimit(), "Server"); // Start the countdown for the official match
            }
        }

        return true;
    }
    else if (command == "lodbg")
    {
        if (IN_GAME_DEBUG_ENABLED)
        {
            if (bz_hasPerm(playerID, "shutdownserver"))
            {
                if (params->size() > 0)
                {
                    std::string commandOption = params->get(0).c_str();

                    if (commandOption == "grant_perm" || commandOption == "revoke_perm")
                    {
                        if (params->size() == 3)
                        {
                            int victimID = getPlayerFromCallsignOrID(params->get(1).c_str())->playerID;

                            if (commandOption == "grant_perm")
                            {
                                bz_grantPerm(victimID, params->get(2).c_str());
                            }
                            else if (commandOption == "revoke_perm")
                            {
                                bz_revokePerm(victimID, params->get(2).c_str());
                            }
                        }
                        else
                        {
                            bz_sendTextMessagef(BZ_SERVER, playerID, "Syntax: /lodbg %s <player id or callsign> <permission name>", commandOption.c_str());
                        }
                    }
                }
                else
                {
                    bz_sendTextMessagef(BZ_SERVER, playerID, "League Overseer In-Game Debug Commands");
                    bz_sendTextMessagef(BZ_SERVER, playerID, "--------------------------------------");
                    bz_sendTextMessagef(BZ_SERVER, playerID, "   /lodbg <option> <parameters>");
                    bz_sendTextMessagef(BZ_SERVER, playerID, "");
                    bz_sendTextMessagef(BZ_SERVER, playerID, "   Options");
                    bz_sendTextMessagef(BZ_SERVER, playerID, "   -------");
                    bz_sendTextMessagef(BZ_SERVER, playerID, "     - grant_perm <player id or callsign> <permission name>");
                    bz_sendTextMessagef(BZ_SERVER, playerID, "     - revoke_perm <player id or callsign> <permission name>");
                    bz_sendTextMessagef(BZ_SERVER, playerID, "     - set");
                    bz_sendTextMessagef(BZ_SERVER, playerID, "         - config_option <option> <value>");
                    bz_sendTextMessagef(BZ_SERVER, playerID, "     - show");
                    bz_sendTextMessagef(BZ_SERVER, playerID, "         - match_stats");
                    bz_sendTextMessagef(BZ_SERVER, playerID, "         - player_stats <player id or callsign>");
                    bz_sendTextMessagef(BZ_SERVER, playerID, "         - config_options");
                }
            }
            else
            {
                bz_sendTextMessage(BZ_SERVER, playerID, "You do not have permission to use the /lodbg command.");
            }
        }
        else
        {
            bz_sendTextMessagef(BZ_SERVER, playerID, "The League Overseer Debug command is disabled on production servers.");
        }
    }
    else if (command == "o" || command == "offi" || command == "official")
    {
        if (DISABLE_OFFICIALS)
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "Sorry, this server has not be configured for official matches.");
        }
        else if (bz_pollActive())
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "You are not allowed to start a match while a poll is active.");
        }
        else if (playerData->team == eObservers) // Observers can't start matches
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "Observers are not allowed to start matches.");
        }
        else if (bz_getTeamCount(TEAM_ONE) < 2 || bz_getTeamCount(TEAM_TWO) < 2) // An official match cannot be 1v1 or 2v1
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "You may not have an official match with less than 2 players per team.");
        }
        else if (officialMatch != NULL || bz_isCountDownActive() || bz_isCountDownInProgress()) // A countdown is in progress already
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "There is already a game in progress; you cannot start another.");
        }
        else // They are verified non-observer with valid team sizes and no existing match. Start one!
        {
            officialMatch.reset(new OfficialMatch()); // It's an official match

            // Log the actions so admins can bug brad to look at detailed information
            logMessage(DEBUG_LEVEL, "debug", "Official match started by %s (%s).", playerData->callsign.c_str(), playerData->ipAddress.c_str());
            bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Official match started by %s.", playerData->callsign.c_str());

            // The amount of seconds the countdown should take
            int timeToStart = (params->size() == 1) ? atoi(params->get(0).c_str()) : 10;

            // Sanity check...
            if (timeToStart <= 60 && timeToStart >= 10)
            {
                bz_startCountdown(timeToStart, bz_getTimeLimit(), "Server"); // Start the countdown with a custom countdown time limit under 1 minute
            }
            else
            {
                bz_sendTextMessage(BZ_SERVER, playerID, "Holy sanity check, Batman! Let's not have a countdown last longer than 60 seconds or less than 10.");
                bz_startCountdown(10, bz_getTimeLimit(), "Server"); // Start the countdown for the official match
            }
        }

        return true;
    }
    else if (command == "p" || command == "pause")
    {
        if (bz_isCountDownPaused())
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "The match is already paused!");
        }
        else if (bz_isCountDownActive())
        {
            bz_pauseCountdown(playerData->callsign.c_str());
        }
        else
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "There is no active match to pause right now.");
        }

        return true;
    }
    else if (command == "r" || command == "resume")
    {
        if (bz_pollActive())
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "You are not allowed to resume a match while a poll is active.");
        }
        else if (!bz_isCountDownPaused())
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "The match is not paused!");
        }
        else if (bz_isCountDownActive())
        {
            bz_resumeCountdown(playerData->callsign.c_str());

            if (officialMatch != NULL)
            {
                logMessage(VERBOSE_LEVEL, "debug", "Match resumed by %s.", playerData->callsign.c_str());
            }
        }
        else
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "There is no active match to resume right now.");
        }

        return true;
    }
    else if (command == "showhidden")
    {
        if (bz_hasPerm(playerID, "ban"))
        {
            std::shared_ptr<bz_APIIntList> playerList(bz_getPlayerIndexList());

            // Our player list couldn't be created so exit out of here
            if (!playerList)
            {
                logMessage(VERBOSE_LEVEL, "debug", "Oops. I couldn't create a playerlist for some odd reason.");
                bz_sendTextMessage(BZ_SERVER, playerID, "Seems like I darn goofed, please execute your command again.");
                return true;
            }

            bz_sendTextMessage(BZ_SERVER, playerID, "Hidden Admins Present");
            bz_sendTextMessage(BZ_SERVER, playerID, "---------------------");

            for (unsigned int i = 0; i < playerList->size(); i++)
            {
                // If the player is hidden, then show them in the list
                if (bz_hasPerm(playerList->get(i), "hideadmin"))
                {
                    bz_sendTextMessagef(BZ_SERVER, playerID, " - %s", bz_getPlayerByIndex(playerList->get(i))->callsign.c_str());
                }
            }
        }
        else
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "You do not have permission to use the /showhidden command.");
        }

        return true;
    }
    else if (command == "spawn")
    {
        if (bz_hasPerm(playerID, "ban"))
        {
            if (params->size() > 0)
            {
                logMessage(VERBOSE_LEVEL, "debug", "%s has executed the /spawn command.", playerData->callsign.c_str());

                std::string callsignOrID = params->get(0).c_str(); // Store the callsign we're going to search for

                logMessage(VERBOSE_LEVEL, "debug", "Callsign or Player slot to look for: %s", callsignOrID.c_str());

                std::shared_ptr<bz_BasePlayerRecord> victim(getPlayerFromCallsignOrID(callsignOrID.c_str()));

                if (victim)
                {
                    bz_grantPerm(victim->playerID, "spawn");
                    bz_sendTextMessagef(BZ_SERVER, eAdministrators, "%s granted %s the ability to spawn.", playerData->callsign.c_str(), victim->callsign.c_str());
                }
                else
                {
                    bz_sendTextMessagef(BZ_SERVER, playerID, "player %s not found", callsignOrID.c_str());
                    logMessage(VERBOSE_LEVEL, "debug", "Player %s was not found.", callsignOrID.c_str());
                }
            }
            else
            {
                bz_sendTextMessage(BZ_SERVER, playerID, "/spawn <player id or callsign>");
            }
        }
        else
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "You do not have permission to use the /spawn command.");
        }

        return true;
    }
    else if (command == "s" || command == "stats")
    {
        if (isLeagueMember(playerID))
        {
            if (officialMatch != NULL)
            {
                bz_sendTextMessage(BZ_SERVER, playerID, "Match Data");
                bz_sendTextMessage(BZ_SERVER, playerID, "----------");

                for (unsigned int i = 0; i < officialMatch->matchEvents.size(); i++)
                {
                    std::string timeStamp = officialMatch->matchEvents.at(i).match_time;
                    std::string message   = officialMatch->matchEvents.at(i).message;

                    bz_sendTextMessagef(BZ_SERVER, playerID, "  [%s] %s", timeStamp.c_str(), message.c_str());
                }
            }
            else
            {
                bz_sendTextMessage(BZ_SERVER, playerID, "Match data is not recorded for fun matches.");
            }
        }
        else
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "You do not have permission to use the /stats command.");
        }

        return true;
    }
}

// We got a response from one of our URL jobs
void LeagueOverseer::URLDone (const char* /*URL*/, const void* data, unsigned int /*size*/, bool /*complete*/)
{
    // This variable will only be set to true for the duration of one URL job, so just set it back to false regardless
    MATCH_INFO_SENT = false;

    // Convert the data we get from the URL job to a std::string
    std::string siteData = (const char*)(data);
    logMessage(VERBOSE_LEVEL, "debug", "URL Job returned: %s", siteData.c_str());

    /*
        For whenever <regex> gets pushed out in the new version of GCC...

        // Let's store this regex since that's how we'll get our team name data
        std::regex teamNameRegex ("\\{\"bzid\":\"\\d+\",\"team\":\"[\\w\\s]+\"\\}");

        // The data returned matches an expected regex syntax meaning we got team name information
        if (std::regex_match(siteData, teamNameRegex))
    */

    // The returned data starts with a '{' and ends with a '}' so chances are it's JSON data
    if (siteData.at(0) == '{' && siteData.at(siteData.length() - 1) == '}')
    {
        json_object* jobj = json_tokener_parse(siteData.c_str());
        enum json_type type;
        std::string urlJobBZID = "", urlJobTeamName = "";

        // Because our JSON information has a BZID and a team name, we need to loop through them to get the information
        json_object_object_foreach(jobj, key, val)
        {
            // Get the type of object, we need to make sure we only handle strings because that's all we should be expecting
            type = json_object_get_type(val);

            // There are multiple JSON types so let's switch through them
            switch (type)
            {
                // We're getting an array meaning it's an entire team dump, so handle it accordingly
                case json_type_array:
                {
                    logMessage(VERBOSE_LEVEL, "debug", "Team dump JSON data received.");

                    // Our array will have multiple indexes with each index containing two elements
                    // so we need to create an array_list that will give us access to all of the indexes
                    array_list* teamMembers = json_object_get_array(val);

                    // Loop through of the indexes in our array
                    for (int i = 0; i < array_list_length(teamMembers); i++)
                    {
                        // Now we need to create a JSON object so we can access the two values that the index
                        // holds, i.e. the team name and BZIDs of the members
                        json_object* individualTeam = (json_object*)array_list_get_idx(teamMembers, i);

                        // We will be storing the team name out here so we can access it as we're looping through
                        // all of the team members
                        std::string teamName;

                        // Now we need to loop through both those elements in the current index
                        json_object_object_foreach(individualTeam, _key, _value)
                        {
                            // Just in case there's something funky, only handle strings at this point
                            if (json_object_get_type(_value) == json_type_string)
                            {
                                // Our first key is the team name so save it
                                if (strcmp(_key, "team") == 0)
                                {
                                    teamName = json_object_get_string(_value);

                                    logMessage(VERBOSE_LEVEL, "debug", "Team '%s' recorded. Getting team members...", teamName.c_str());
                                }
                                // Our second key is going to be the team members' BZIDs seperated by commas
                                else if (strcmp(_key, "members") == 0)
                                {
                                    // Now we need to handle each BZID separately so we will split the elements
                                    // by each comma and stuff it into a vector
                                    std::vector<std::string> bzIDs = split(json_object_get_string(_value), ",");

                                    // Now iterate through the vector of team members so we can save them to our
                                    // map
                                    for (std::vector<std::string>::const_iterator it = bzIDs.begin(); it != bzIDs.end(); ++it)
                                    {
                                        std::string bzID = std::string(*it);
                                        teamMottos[bzID.c_str()] = teamName.c_str();

                                        logMessage(VERBOSE_LEVEL, "debug", "BZID %s set to team %s.", bzID.c_str(), teamName.c_str());
                                    }
                                }
                            }
                        }
                    }
                }
                break;

                // We've found a JSON string, which means it's only a single team name and bzid so handle it accordingly
                case json_type_string:
                {
                    logMessage(VERBOSE_LEVEL, "debug", "Team name JSON data received.");

                    // Store the respective information in other variables because we aren't done looping
                    if (strcmp(key, "bzid") == 0)
                    {
                        urlJobBZID = json_object_get_string(val);
                    }
                    else if (strcmp(key, "team") == 0)
                    {
                        urlJobTeamName = json_object_get_string(val);
                    }
                }
                break;

                default: break;
            }
        }

        // We have both a BZID and a team name so let's update our team motto map
        if (urlJobBZID != "")
        {
            teamMottos[urlJobBZID] = urlJobTeamName;

            logMessage(VERBOSE_LEVEL, "debug", "Motto saved for BZID %s.", urlJobBZID.c_str());

            // If the team name is equal to an empty string that means a player is teamless and if they are in our motto
            // map, that means they recently left a team so remove their entry in the map
            if (urlJobTeamName == "" && teamMottos.count(urlJobBZID))
            {
                teamMottos.erase(urlJobBZID);
            }
        }
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

// We are building a string of BZIDs from the people who matched in the match that just occurred
// and we're also writing the player information to the server logs while we're at it. Efficiency!
std::string LeagueOverseer::buildBZIDString (bz_eTeamType team)
{
    // The string of BZIDs separated by commas
    std::string teamString;

    // Send a debug message of the players on the specified team
    bz_debugMessagef(0, "Match Data :: %s Team Players", formatTeam(team).c_str());

    // Add all the players from the specified team to the match report
    for (unsigned int i = 0; i < officialMatch->matchParticipants.size(); i++)
    {
        // If the player current player is part of the team we're formatting
        if (officialMatch->matchParticipants.at(i).teamColor == team)
        {
            // Add the BZID of the player to string with a comma at the end
            teamString += std::string(bz_urlEncode(officialMatch->matchParticipants.at(i).bzID.c_str())) + ",";

            // Output their information to the server logs
            bz_debugMessagef(0, "Match Data ::  %s [%s] (%s)", officialMatch->matchParticipants.at(i).callsign.c_str(),
                                                               officialMatch->matchParticipants.at(i).bzID.c_str(),
                                                               officialMatch->matchParticipants.at(i).ipAddress.c_str());
        }
    }

    // Return the comma separated string minus the last character because the loop will always
    // add an extra comma at the end. If we leave it, it will cause issues with the PHP counterpart
    // which tokenizes the BZIDs by commas and we don't want an empty BZID
    return teamString.erase(teamString.size() - 1);
}

// Return the progress of a match in seconds. For example, 20:00 minutes remaining would return 600
int LeagueOverseer::getMatchProgress (void)
{
    if (isMatchInProgress())
    {
        time_t now = time(NULL);

        return difftime(now, MATCH_START);
    }

    return -1;
}

// Get the literal time remaining in a match in the format of MM:SS
std::string LeagueOverseer::getMatchTime (void)
{
    int time = getMatchProgress();

    // We could not get the current match progress
    if (getMatchProgress() < 0)
    {
        return "-00:00";
    }

    // Let's covert the seconds of a match's progress into minutes and seconds
    int minutes = (officialMatch->duration/60) - ceil(time / 60.0);
    int seconds = 60 - (time % 60);

    // We need to store the literal values
    std::string minutesLiteral,
                secondsLiteral;

    // If the minutes remaining are less than 10 (only has one digit), then prepend a 0 to keep the format properly
    minutesLiteral = (minutes < 10) ? "0" : "";
    minutesLiteral += intToString(minutes);

    // Do some formatting for seconds similarly to minutes
    if (seconds == 60)
    {
        secondsLiteral = "00";
    }
    else
    {
        secondsLiteral = (seconds < 10) ? "0" : "";
        secondsLiteral += intToString(seconds);
    }

    return minutesLiteral + ":" + secondsLiteral;
}

std::string LeagueOverseer::getPlayerTeamNameByID (int playerID)
{
    std::shared_ptr<bz_BasePlayerRecord> playerData(bz_getPlayerByIndex(playerID));

    return getPlayerTeamNameByBZID(playerData->bzID.c_str());
}

std::string LeagueOverseer::getPlayerTeamNameByBZID (std::string bzID)
{
    return teamMottos[bzID];
}

// Check if a player is part of the league
bool LeagueOverseer::isLeagueMember (int playerID)
{
    return IS_LEAGUE_MEMBER[playerID];
}

// Check if there is a match in progress; even if it's paused
bool LeagueOverseer::isMatchInProgress (void)
{
    return (bz_isCountDownActive() || bz_isCountDownPaused() || bz_isCountDownInProgress());
}

// Check if there is currently an active official match
bool LeagueOverseer::isOfficialMatch(void)
{
    return (officialMatch != NULL);
}

// Check if there is currently an active official match
bool LeagueOverseer::isOfficialMatchInProgress (void)
{
    return (isOfficialMatch() && isMatchInProgress());
}

// Load the plugin configuration file
void LeagueOverseer::loadConfig (const char* cmdLine)
{
    // Setup to read the configuration file
    PLUGIN_CONFIG = PluginConfig(cmdLine);
    PLUGIN_SECTION = "leagueOverSeer";

    // Shutdown the server if the configuration file has errors because we can't do anything
    // with a broken config
    if (PLUGIN_CONFIG.errors)
    {
        logMessage(0, "error", "Your configuration file contains errors. Shutting down...");
        bz_shutdown();
    }

    // Extract all the data in the configuration file and assign it to plugin variables
    SPAWN_COMMAND_PERM     = setPluginConfigString("SPAWN_COMMAND_PERM", "ban");
    SHOW_HIDDEN_PERM       = setPluginConfigString("SHOW_HIDDEN_PERM", "ban");
    MAPCHANGE_PATH         = setPluginConfigString("MAPCHANGE_PATH", "");
    LEAGUE_GROUP           = setPluginConfigString("LEAGUE_GROUP", "VERIFIED");
    PC_PROTECTION_ENABLED  = setPluginConfigBool("PC_PROTECTION_ENABLED", false);
    IN_GAME_DEBUG_ENABLED  = setPluginConfigBool("ENABLE_IN_GAME_DEBUG", false);
    MATCH_REPORT_ENABLED   = setPluginConfigBool("MATCH_REPORT_ENABLED", true, "DISABLE_MATCH_REPORT", true);
    MOTTO_FETCH_ENABLED    = setPluginConfigBool("MOTTO_FETCH_ENABLED", true, "DISABLE_TEAM_MOTTO", true);
    ALLOW_LIMITED_CHAT     = setPluginConfigBool("ALLOW_LIMITED_CHAT", false);
    SPAWN_MSG_ENABLED      = setPluginConfigBool("ENABLE_SPAWN_MESSAGE", true);
    DISABLE_OFFICIALS      = setPluginConfigBool("DISABLE_OFFICIAL_MATCHES", false);
    TALK_MSG_ENABLED       = setPluginConfigBool("ENABLE_TALK_MESSAGE", true);
    ROTATION_LEAGUE        = setPluginConfigBool("ROTATIONAL_LEAGUE", false);
    DISABLE_FMS            = setPluginConfigBool("DISABLE_FM_MATCHES", false);
    PC_PROTECTION_DELAY    = setPluginConfigInt("PC_PROTECTION_DELAY", 15);
    VERBOSE_LEVEL          = setPluginConfigInt("VERBOSE_LEVEL", 4, "DEBUG_ALL");
    DEBUG_LEVEL            = setPluginConfigInt("DEBUG_LEVEL", 1);

    NO_SPAWN_MSG           = split(PLUGIN_CONFIG.item(PLUGIN_SECTION, "NO_SPAWN_MESSAGE"), "\n");
    NO_TALK_MSG            = split(PLUGIN_CONFIG.item(PLUGIN_SECTION, "NO_TALK_MESSAGE"), "\n");

    if (!PLUGIN_CONFIG.item(PLUGIN_SECTION, "LEAGUE_OVERSEER_URL").empty())
    {
        MATCH_REPORT_URL = setPluginConfigString("LEAGUE_OVERSEER_URL", "", "LEAGUE_OVER_SEER_URL", true);
        TEAM_NAME_URL    = setPluginConfigString("LEAGUE_OVERSEER_URL", "", "LEAGUE_OVER_SEER_URL");
    }
    else
    {
        if (MATCH_REPORT_ENABLED)
        {
            if (!PLUGIN_CONFIG.item(PLUGIN_SECTION, "MATCH_REPORT_URL").empty())
            {
                MATCH_REPORT_URL = PLUGIN_CONFIG.item(PLUGIN_SECTION, "MATCH_REPORT_URL");
            }
            else
            {
                logMessage(0, "error", "You are requesting to report matches but you have not specified a URL to report to.");
                logMessage(0, "error", "Please set the 'MATCH_REPORT_URL' or 'LEAGUE_OVERSEER_URL' option respectively.");
                logMessage(0, "error", "If you do not wish to report matches, set 'DISABLE_MATCH_REPORT' to true.");
                bz_shutdown();
            }
        }

        if (MOTTO_FETCH_ENABLED)
        {
            if (!PLUGIN_CONFIG.item(PLUGIN_SECTION, "MOTTO_FETCH_URL").empty())
            {
                TEAM_NAME_URL = PLUGIN_CONFIG.item(PLUGIN_SECTION, "MOTTO_FETCH_URL");
            }
            else
            {
                logMessage(0, "error", "You have requested to fetch team names but have not specified a URL to fetch them from.");
                logMessage(0, "error", "Please set the 'MOTTO_FETCH_URL' or 'LEAGUE_OVERSEER_URL' option respectively.");
                logMessage(0, "error", "If you do not wish to team names for mottos, set 'DISABLE_TEAM_MOTTO' to true.");
                bz_shutdown();
            }
        }
    }

    // Sanity check for our debug level, if it doesn't pass the check then set the debug level to 1
    if (DEBUG_LEVEL > 4 || DEBUG_LEVEL < 0)
    {
        logMessage(0, "warning", "Invalid debug level in the configuration file.");
        logMessage(0, "warning", "Debug level set to the default: 1.");
        DEBUG_LEVEL = 1;
    }

    // We don't need to advertise that VERBOSE_LEVEL failed so let's set it to 4, which is the default
    if (VERBOSE_LEVEL > 4 || VERBOSE_LEVEL < 0)
    {
        VERBOSE_LEVEL = 4;
    }

    // Output the configuration settings
    logMessage(VERBOSE_LEVEL, "debug", "Configuration File Settings");
    logMessage(VERBOSE_LEVEL, "debug", "---------------------------");
    logMessage(VERBOSE_LEVEL, "debug", "Rotational league set to  : %s", (ROTATION_LEAGUE) ? "true" : "false");

    if (ROTATION_LEAGUE)
    {
        logMessage(VERBOSE_LEVEL, "debug", "Map change path set to    : %s", MAPCHANGE_PATH.c_str());
    }

    if (MATCH_REPORT_ENABLED)
    {
        logMessage(VERBOSE_LEVEL, "debug", "Reporting matches to URL  : %s", MATCH_REPORT_URL.c_str());
    }

    if (MOTTO_FETCH_ENABLED)
    {
        logMessage(VERBOSE_LEVEL, "debug", "Fetching Team Names from  : %s", TEAM_NAME_URL.c_str());
    }

    logMessage(VERBOSE_LEVEL, "debug", "Debug level set to        : %d", DEBUG_LEVEL);
    logMessage(VERBOSE_LEVEL, "debug", "Verbose level set to      : %d", VERBOSE_LEVEL);
}

// Check if a player was already on the server within 5 minutes of their last part
bool LeagueOverseer::playerAlreadyJoined (std::string bzID)
{
    // Loop through all of the players on the list
    for (unsigned int i = 0; i < activePlayerList.size(); i++)
    {
        // The player has their BZID saved as being active
        if (activePlayerList.at(i).bzID == bzID)
        {
            // If they left at least 1 minute ago, then update their last active time and consider them active
            if (activePlayerList.at(i).lastActive + 60 > bz_getCurrentTime())
            {
                activePlayerList.at(i).lastActive = bz_getCurrentTime();
                return true;
            }
            else // Their BZID was saved but it's been more than 5 minutes, remove them and mark them as they were not active
            {
                activePlayerList.erase(activePlayerList.begin() + i, activePlayerList.begin() + i + 1);
                return false;
            }
        }
    }

    // They weren't saved on the list
    return false;
}

// Request a team name update for all the members of a team
void LeagueOverseer::requestTeamName (bz_eTeamType team)
{
    logMessage(VERBOSE_LEVEL, "debug", "A team name update for the '%s' team has been requested.", formatTeam(team).c_str());
    std::shared_ptr<bz_APIIntList> playerList(bz_getPlayerIndexList());

    // Our player list couldn't be created so exit out of here
    if (!playerList)
    {
        logMessage(VERBOSE_LEVEL, "debug", "The player list to process team names failed to be created.");
        return;
    }

    for (unsigned int i = 0; i < playerList->size(); i++)
    {
        std::shared_ptr<bz_BasePlayerRecord> playerRecord(bz_getPlayerByIndex(playerList->get(i)));

        if (playerRecord && playerRecord->team == team) // Only request a new team name for the players of a certain team
        {
            logMessage(VERBOSE_LEVEL, "debug", "Player '%s' is a part of the '%s' team.", playerRecord->callsign.c_str(), formatTeam(team).c_str());
            requestTeamName(playerRecord->callsign.c_str(), playerRecord->bzID.c_str());
        }
    }
}

// Because there will be different times where we request a team name motto, let's make into a function
void LeagueOverseer::requestTeamName (std::string callsign, std::string bzID)
{
    // Build the POST data for the URL job
    std::string teamMotto = "query=teamName&apiVersion=" + intToString(API_VERSION);
    teamMotto += "&bzid=" + std::string(bzID.c_str());

    logMessage(DEBUG_LEVEL, "debug", "Sending motto request for '%s'", callsign.c_str());

    // Send the team update request to the league website
    bz_addURLJob(TEAM_NAME_URL.c_str(), this, teamMotto.c_str());
}

// Check the player's user groups to see if they belong to the league and save that value
void LeagueOverseer::setLeagueMember (int playerID)
{
    std::shared_ptr<bz_BasePlayerRecord> playerData(bz_getPlayerByIndex(playerID));

    IS_LEAGUE_MEMBER[playerID] = false;

    // If a player isn't verified, then they are for sure not a registered player
    if (playerData->verified)
    {
        for (int i = 0; i < playerData->groups.size(); i++) // Go through all the groups a player belongs to
        {
            std::string group = playerData->groups.get(i).c_str(); // Convert the group into a string

            if (group == LEAGUE_GROUP) // Player is a part of the *.LEAGUE group
            {
                IS_LEAGUE_MEMBER[playerID] = true;
                break;
            }
        }
    }
}

std::string LeagueOverseer::setPluginConfig (std::string value, std::string defaultValue, std::string deprecatedField, bool showMsg)
{
    std::string pluginConfig = PLUGIN_CONFIG.item(PLUGIN_SECTION, value); // Get the value of the config field

    if (pluginConfig.empty()) // If the field is empty...
    {
        if (!deprecatedField.empty()) // Check if we're looking at a deprecated field
        {
            std::string deprecatedValue = PLUGIN_CONFIG.item(PLUGIN_SECTION, deprecatedField); // Get the value from the deprecated field

            if (showMsg)
            {
                showDeprecatedConfigValueWarning(deprecatedField, value);
            }

            if (!deprecatedValue.empty()) // If the value isn't empty, return the boolean value of it
            {
                return deprecatedValue;
            }
        }

        // If we've had no luck finding a value to use, let's use the default value
        return defaultValue;
    }

    // We found a normal value, let's return the boolean equivalent of it
    return pluginConfig;
}

// Get the value of a configuration boolean setting or use the default
bool LeagueOverseer::setPluginConfigBool (std::string value, bool defaultValue, std::string deprecatedField, bool showMsg)
{
    std::string configValue = setPluginConfig(value, boolToString(defaultValue), deprecatedField);

    return toBool(configValue);
}

// Get the value of a configuration int setting or use the default
int LeagueOverseer::setPluginConfigInt (std::string value, int defaultValue, std::string deprecatedField, bool showMsg)
{
    std::string configValue = setPluginConfig(value, intToString(defaultValue), deprecatedField);

    return atoi(configValue.c_str());
}

// Get the value of a configuration string setting or use the default
std::string LeagueOverseer::setPluginConfigString (std::string value, std::string defaultValue, std::string deprecatedField, bool showMsg)
{
    return setPluginConfig(value, defaultValue, deprecatedField);
}

// Check if there is any need to invalidate a roll call team
void LeagueOverseer::validateTeamName (bool &invalidate, bool &teamError, MatchParticipant currentPlayer, std::string &teamName, bz_eTeamType team)
{
    logMessage(VERBOSE_LEVEL, "debug", "Starting validation of the %s team.", formatTeam(team).c_str());

    // Check if the player is a part of the team we're validating
    if (currentPlayer.teamColor == team)
    {
        // Check if the team name of team one has been set yet, if it hasn't then set it
        // and we'll be able to set it so we can conclude that we have the same team for
        // all of the players
        if (teamName == "")
        {
            teamName = currentPlayer.teamName;
            logMessage(VERBOSE_LEVEL, "debug", "The team name for the %s team has been set to: %s", formatTeam(team).c_str(), teamName.c_str());
        }
        // We found someone with a different team name, therefore we need invalidate the
        // roll call and check all of the member's team names for sanity
        else if (teamName != currentPlayer.teamName)
        {
            logMessage(VERBOSE_LEVEL, "error", "Player '%s' is not part of the '%s' team.", currentPlayer.callsign.c_str(), teamName.c_str());
            invalidate = true; // Invalidate the roll call
            teamError = true;  // We need to check team one's members for their teams
        }
    }

    if (!teamError)
    {
        logMessage(VERBOSE_LEVEL, "debug", "Player '%s' belongs to the '%s' team.", currentPlayer.callsign.c_str(), currentPlayer.teamName.c_str());
    }
}