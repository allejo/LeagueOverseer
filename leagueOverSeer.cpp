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
#include <fstream>
#include <iostream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>

#include "bzfsAPI.h"
#include "plugin_utils.h"

// Define plugin version numbering
const int MAJOR = 1;
const int MINOR = 1;
const int REV = 0;
const int BUILD = 187;

// Log failed assertions at debug level 0 since this will work for non-member functions and it is important enough.
#define ASSERT(x)
{
    if (!(x)) // If the condition is not true, then send a debug message
    {
        bz_debugMessagef(0, "ERROR :: League Over Seer :: Failed assertion '%s' at %s:%d", #x, __FILE__, __LINE__);
    }
}

// A function that will get the player record by their callsign
static bz_BasePlayerRecord* bz_getPlayerByCallsign(const char* callsign)
{
    // Use a smart pointer so we don't have to worry about freeing up the memory
    // when we're done. In other words, laziness.
    std::unique_ptr<bz_APIIntList> playerList(bz_getPlayerIndexList());

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

    // Return NULL if the callsign was not found
    return NULL;
}

// Convert an int to a string
static std::string intToString(int number)
{
    std::stringstream string;
    string << number;

    return string.str();
}

// Convert a bz_eTeamType value into a string literal with the option
// of adding whitespace to format the string to return
static std::string formatTeam(bz_eTeamType teamColor, bool addWhiteSpace)
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

// Return whether or not a specified player ID exists or not
static bool isValidPlayerID(int playerID)
{
    // Use another smart pointer so we don't forget about freeing up memory
    std::unique_ptr<bz_BasePlayerRecord> playerData(bz_getPlayerByIndex(playerID));

    // If the pointer doesn't exist, that means the playerID does not exist
    return (playerData) ? true : false;
}

// Convert a string representation of a boolean to a boolean
static bool to_bool(std::string str)
{
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    std::istringstream is(str);
    bool b;
    is >> std::boolalpha >> b;
    return b;
}

class LeagueOverseer : public bz_Plugin, public bz_CustomSlashCommandHandler, public bz_BaseURLHandler
{
public:
    virtual const char* Name ()
    {
        char buffer[100];
        sprintf(buffer, "League Overseer %i.%i.%i (%i)", MAJOR, MINOR, REV, BUILD);
        return std::string(buffer).c_str();
    }
    virtual void Init (const char* config);
    virtual void Event (bz_EventData *eventData);
    virtual void Cleanup (void);

    virtual bool SlashCommand (int playerID, bz_ApiString, bz_ApiString, bz_APIStringList*);

    virtual void URLDone(const char* URL, const void* data, unsigned int size, bool complete);
    virtual void URLTimeout(const char* URL, int errorCode);
    virtual void URLError(const char* URL, int errorCode, const char *errorString);

    virtual std::string buildBZIDString(bz_eTeamType team);
    virtual void loadConfig(const char *cmdLine);

    // We will be storing information about the players who participated in a match so we will
    // be storing that information inside a struct
    struct MatchParticipant
    {
        std::string bzID;
        std::string callsign;
        std::string teamName;
        bz_eTeamType team;

        MatchParticipant(std::string _bzID, std::string _callsign, std::string _teamName, bz_eTeamType _teamColor) :
            bzID(_bzid),
            callsign(_callsign),
            teamName(_teamName),
            teamColor(_teamColor)
        {}
    };

    // Simply out of preference, we will be storing all the information regarding a match inside
    // of a struct where the struct will be NULL if it is current a fun match
    struct OfficialMatch
    {
        bool        playersRecorded,   // Whether or not the players participating in the match have been recorded or not
                    canceled;          // Whether or not the official match was canceled

        std::string cancelationReason; // If the match was canceled, store the reason as to why it was canceled

        double      startTime,         // The time of the the match was started (in server seconds). Used to calculate roll call
                    duration;          // The length of the match in seconds. Used when reporting a match to the server

        // We keep the number of points scored in the case where all the members of a team leave and their team
        // score get reset to 0
        int         teamOnePoints,
                    teamTwoPoints;

        // We will be storing all of the match participants in this vector
        std::vector<MatchParticipant> matchParticipants;

        // Set the default values for this struct
        OfficialMatch() :
            canceled(false),
            cancelationReason(""),
            playersRecorded(false),
            startTime(-1.0f),
            duration(-1.0f),
            teamOnePoints(0),
            teamTwoPoints(0),
            matchParticipants()
        {}
    };

    // All the variables that will be used in the plugin
    bool         ROTATION_LEAGUE; // Whether or not we are watching a league that uses different maps

    int          DEBUG_LEVEL;     // The DEBUG level the server owner wants the plugin to use for its messages

    double       MATCH_ROLLCALL;  // The number of seconds the plugin needs to wait before recording who's matching

    std::string  LEAGUE_URL,      // The URL the plugin will use to report matches. This should be the URL the PHP counterpart of this plugin
                 MAP_NAME,        // The name of the map that is currently be played if it's a rotation league (i.e. OpenLeague uses multiple maps)
                 MAPCHANGE_PATH;  // The path to the file that contains the name of current map being played

    bz_eTeamType TEAM_ONE,        // Because we're serving more than just GU league, we need to support different colors therefore, call the teams
                 TEAM_TWO;        // ONE and TWO

    // This is the only pointer of the struct for the official match that we will be using. If this
    // variable is set to NULL, that means that there is currently no official match occurring.
    std::unique_ptr<OfficialMatch> officialMatch;

    // We will be using a map to handle the team name mottos in the format of
    // <BZID, Team Name>
    typedef std::map<std::string, std::string> TeamNameMottoMap;
    TeamNameMottoMap teamMottos;
};

BZ_PLUGIN(LeagueOverseer)

void LeagueOverseer::Init (const char* commandLine)
{
    // Register our events with Register()
    Register(bz_eCaptureEvent);
    Register(bz_eGameEndEvent);
    Register(bz_eGameStartEvent);
    Register(bz_eGetPlayerMotto);
    Register(bz_ePlayerJoinEvent);
    Register(bz_eTickEvent);

    // Register our custom slash commands
    bz_registerCustomSlashCommand('cancel', this);
    bz_registerCustomSlashCommand('finish', this);
    bz_registerCustomSlashCommand('fm', this);
    bz_registerCustomSlashCommand('official', this);
    bz_registerCustomSlashCommand('spawn', this);
    bz_registerCustomSlashCommand('pause', this);
    bz_registerCustomSlashCommand('resume', this);

    // Set some default values
    MATCH_ROLLCALL = 90;
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

        //Remove the '.conf' from the mapchange.out file
        MAP_NAME = MAP_NAME.substr(0, MAP_NAME.length() - 5);

        bz_debugMessagef(DEBUG_LEVEL, "DEBUG :: League Over Seer :: Current map being played: %s", MAP_NAME.c_str());
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
                break;
            }
        }
    }

    // Make sure both teams were found, if they weren't then notify in the logs
    ASSERT(TEAM_ONE != eNoTeam && TEAM_TWO != eNoTeam);
}

void LeagueOverseer::Cleanup (void)
{
    Flush(); // Clean up all the events

    // Clean up our custom slash commands
    bz_removeCustomSlashCommand('cancel');
    bz_removeCustomSlashCommand('finish');
    bz_removeCustomSlashCommand('fm');
    bz_removeCustomSlashCommand('official');
    bz_removeCustomSlashCommand('spawn');
    bz_removeCustomSlashCommand('pause');
    bz_removeCustomSlashCommand('resume');
}

void LeagueOverseer::Event(bz_EventData *eventData)
{
    switch (eventData->eventType)
    {
        case bz_eCaptureEvent: // This event is called each time a team's flag has been captured
        {
            bz_CTFCaptureEventData_V1* captureData = (bz_CTFCaptureEventData_V1*)eventData;

            // Data
            // ---
            //    (bz_eTeamType)  teamCapped    - The team whose flag was captured.
            //    (bz_eTeamType)  teamCapping   - The team who did the capturing.
            //    (int)           playerCapping - The player who captured the flag.
            //    (float[3])      pos           - The world position(X,Y,Z) where the flag has been captured
            //    (float)         rot           - The rotational orientation of the capturing player
            //    (double)        time          - This value is the local server time of the event.
        }
        break;

        case bz_eGameEndEvent: // This event is called each time a game ends
        {
            bz_GameStartEndEventData_V1* gameEndData = (bz_GameStartEndEventData_V1*)eventData;

            // Data
            // ---
            //    (double) duration  - The duration (in seconds) of the game.
            //    (double) eventTime - The server time the event occurred (in seconds).
        }
        break;

        case bz_eGameStartEvent: // This event is triggered when a timed game begins
        {
            bz_GameStartEndEventData_V1* gameStartData = (bz_GameStartEndEventData_V1*)eventData;

            // Data
            // ---
            //    (double) duration  - The duration (in seconds) of the game.
            //    (double) eventTime - The server time the event occurred (in seconds).
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
        }
        break;

        case bz_eTickEvent: // This event is called once for each BZFS main loop
        {
            bz_TickEventData_V1* tickData = (bz_TickEventData_V1*)eventData;

            // Data
            // ---
            //    (double)  eventTime - Local Server time of the event (in seconds)
        }
        break;

        default: break;
    }
}

bool LeagueOverseer::SlashCommand(int playerID, bz_ApiString command, bz_ApiString /*message*/, bz_APIStringList *params)
{
    if (command == "cancel")
    {

        return true;
    }
    else if (command == "finish")
    {

        return true;
    }
    else if (command == "fm")
    {

        return true;
    }
    else if (command == "official")
    {

        return true;
    }
    else if (command == "spawn")
    {

        return true;
    }
    else if (command == "pause")
    {

        return true;
    }
    else if (command == "resume")
    {

        return true;
    }
}