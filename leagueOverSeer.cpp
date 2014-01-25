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
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "bzfsAPI.h"
#include "plugin_utils.h"

// Define plugin version numbering
const int MAJOR = 1;
const int MINOR = 1;
const int REV = 0;
const int BUILD = 248;

// The API number used to notify the PHP counterpart about how to handle the data
const int API_VERSION = 1;

// Log failed assertions at debug level 0 since this will work for non-member functions and it is important enough.
#define ASSERT(x) { if (!(x)) { bz_debugMessagef(0, "ERROR :: League Over Seer :: Failed assertion '%s' at %s:%d", #x, __FILE__, __LINE__); }}

// A function that will get the player record by their callsign
static bz_BasePlayerRecord* bz_getPlayerByCallsign (const char* callsign)
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
    std::unique_ptr<bz_BasePlayerRecord> playerData(bz_getPlayerByIndex(playerID));

    // If the pointer doesn't exist, that means the playerID does not exist
    return (playerData) ? true : false;
}

// Split a string by a delimeter and return a vector of elements
static std::vector<std::string> split (const char *str, char c = ' ')
{
    std::vector<std::string> result;

    do
    {
        const char *begin = str;

        while(*str != c && *str)
        {
            str++;
        }

        result.push_back(std::string(begin, str));
    } while (0 != *str++);

    return result;
}

// Convert a string representation of a boolean to a boolean
static bool toBool (std::string str)
{
    return !str.empty() && (strcasecmp(str.c_str (), "true") == 0 || atoi(str.c_str ()) != 0);
}

class LeagueOverseer : public bz_Plugin, public bz_CustomSlashCommandHandler, public bz_BaseURLHandler
{
public:
    virtual const char* Name ()
    {
        std::string        pluginBuild;
        std::ostringstream pluginBuildStream;

        pluginBuildStream << "League Overseer " << MAJOR << "." << MINOR << "." << REV << " (" << BUILD << ")";
        pluginBuild = pluginBuildStream.str();

        return pluginBuild.c_str();
    }
    virtual void Init (const char* config);
    virtual void Event (bz_EventData *eventData);
    virtual void Cleanup (void);

    virtual bool SlashCommand (int playerID, bz_ApiString, bz_ApiString, bz_APIStringList*);

    virtual void URLDone (const char* URL, const void* data, unsigned int size, bool complete);
    virtual void URLTimeout (const char* URL, int errorCode);
    virtual void URLError (const char* URL, int errorCode, const char *errorString);

    // We will be storing information about the players who participated in a match so we will
    // be storing that information inside a struct
    struct MatchParticipant
    {
        std::string bzID;
        std::string callsign;
        std::string ipAddress;
        std::string teamName;
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
                    timePaused,         // The server seconds at which a game was paused at. This value is used to calculate the roll call time
                    approxTimeProgress; // The approximate amount of seconds that have passed since the game start; this value is affected by
                                        //     the time paused and the 5 second resume countdown. This value is only used for the roll call time

        // We keep the number of points scored in the case where all the members of a team leave and their team
        // score get reset to 0
        int         teamOnePoints,
                    teamTwoPoints;

        // We will be storing all of the match participants in this vector
        std::vector<MatchParticipant> matchParticipants;

        // Set the default values for this struct
        OfficialMatch () :
            playersRecorded(false),
            canceled(false),
            cancelationReason(""),
            teamOneName("Team-A"),
            teamTwoName("Team-B"),
            duration(-1.0f),
            timePaused(0.0f),
            approxTimeProgress(-1.0f),
            teamOnePoints(0),
            teamTwoPoints(0),
            matchParticipants()
        {}
    };

    virtual std::string buildBZIDString (bz_eTeamType team);
    virtual void loadConfig (const char *cmdLine);
    virtual void requestTeamName (bz_eTeamType team);
    virtual void requestTeamName (std::string callsign, std::string bzID);
    virtual void validateTeamName (bool &invalidate, bool &teamError, MatchParticipant currentPlayer, std::string &teamName, bz_eTeamType team);
    virtual void updateTeamNames (void);

    // All the variables that will be used in the plugin
    bool         ROTATION_LEAGUE, // Whether or not we are watching a league that uses different maps
                 RECORDING;       // Whether or not we are recording a match

    int          DEBUG_LEVEL,     // The DEBUG level the server owner wants the plugin to use for its messages
                 DEBUG_ALL;       // This is the spamming/ridiculous level of debug that the plugin uses

    double       MATCH_ROLLCALL;  // The number of seconds the plugin needs to wait before recording who's matching

    std::string  LEAGUE_URL,      // The URL the plugin will use to report matches. This should be the URL the PHP counterpart of this plugin
                 MAP_NAME,        // The name of the map that is currently be played if it's a rotation league (i.e. OpenLeague uses multiple maps)
                 MAPCHANGE_PATH;  // The path to the file that contains the name of current map being played

    bz_eTeamType TEAM_ONE,        // Because we're serving more than just GU league, we need to support different colors therefore, call the teams
                 TEAM_TWO;        //     ONE and TWO

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
    Register(bz_eSlashCommandEvent);
    Register(bz_eTickEvent);

    // Register our custom slash commands
    bz_registerCustomSlashCommand("cancel", this);
    bz_registerCustomSlashCommand("finish", this);
    bz_registerCustomSlashCommand("fm", this);
    bz_registerCustomSlashCommand("official", this);
    bz_registerCustomSlashCommand("spawn", this);
    bz_registerCustomSlashCommand("pause", this);
    bz_registerCustomSlashCommand("resume", this);

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

        // Remove the '.conf' from the mapchange.out file
        MAP_NAME = MAP_NAME.substr(0, MAP_NAME.length() - 5);

        bz_debugMessagef(DEBUG_LEVEL, "DEBUG :: League Overseer :: Current map being played: %s", MAP_NAME.c_str());
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
    ASSERT(TEAM_ONE != eNoTeam && TEAM_TWO != eNoTeam);

    updateTeamNames();
}

void LeagueOverseer::Cleanup (void)
{
    Flush(); // Clean up all the events

    // Clean up our custom slash commands
    bz_removeCustomSlashCommand("cancel");
    bz_removeCustomSlashCommand("finish");
    bz_removeCustomSlashCommand("fm");
    bz_removeCustomSlashCommand("official");
    bz_removeCustomSlashCommand("spawn");
    bz_removeCustomSlashCommand("pause");
    bz_removeCustomSlashCommand("resume");
}

void LeagueOverseer::Event (bz_EventData *eventData)
{
    switch (eventData->eventType)
    {
        case bz_eCaptureEvent: // This event is called each time a team's flag has been captured
        {
            // We only need to keep track of the store if it's an official match
            if (officialMatch != NULL)
            {
                bz_CTFCaptureEventData_V1* captureData = (bz_CTFCaptureEventData_V1*)eventData;
                (captureData->teamCapping == TEAM_ONE) ? officialMatch->teamOnePoints++ : officialMatch->teamTwoPoints++;

                bz_debugMessagef(DEBUG_ALL, "DEBUG :: League Overseer :: %s team scored.", formatTeam(captureData->teamCapping).c_str());
                bz_debugMessagef(DEBUG_ALL, "DEBUG :: Leauve Overseer :: Official Match Score %s [%i] vs %s [%i]",
                    formatTeam(TEAM_ONE).c_str(), officialMatch->teamOnePoints,
                    formatTeam(TEAM_TWO).c_str(), officialMatch->teamTwoPoints);
            }
        }
        break;

        case bz_eGameEndEvent: // This event is called each time a game ends
        {
            bz_debugMessage(DEBUG_ALL, "DEBUG :: League Overseer :: A match has ended.");

            // Get the current standard UTC time
            bz_Time standardTime;
            bz_getUTCtime(&standardTime);
            std::string recordingFileName;

            // Only save the recording buffer if we actually started recording when the match started
            if (RECORDING)
            {
                bz_debugMessagef(DEBUG_ALL, "DEBUG :: League Overseer :: Recording was in progress during the match.");

                // We'll be formatting the file name, so create a variable to store it
                char tempRecordingFileName[512];

                // Let's get started with formatting
                if (officialMatch != NULL)
                {
                    // If the official match was finished, then mark it as canceled
                    std::string matchCanceled = (officialMatch->canceled) ? "-Canceled" : "",
                                _teamOneName  = officialMatch->teamOneName.c_str(),
                                _teamTwoName  = officialMatch->teamTwoName.c_str();

                    // We want to standardize the names, so replace all spaces with underscores and
                    // any weird HTML symbols should have been stripped already by the PHP script
                    std::replace(_teamOneName.begin(), _teamOneName.end(), ' ', '_');
                    std::replace(_teamTwoName.begin(), _teamTwoName.end(), ' ', '_');

                    sprintf(tempRecordingFileName, "offi-%d%02d%02d-%s-vs-%s-%02d%02d%s.rec",
                        standardTime.year, standardTime.month, standardTime.day,
                        _teamOneName.c_str(), _teamTwoName.c_str(),
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
                bz_debugMessagef(DEBUG_ALL, "DEBUG :: League Overseer :: Replay file will be named: %s", recordingFileName.c_str());

                // Save the recording buffer and stop recording
                bz_saveRecBuf(recordingFileName.c_str(), 0);
                bz_stopRecBuf();
                bz_debugMessagef(DEBUG_ALL, "DEBUG :: League Overseer :: Replay file has been saved and recording has stopped.");

                // We're no longer recording, so set the boolean and announce to players that the file has been saved
                RECORDING = false;
                bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Match saved as: %s", recordingFileName.c_str());
            }

            if (officialMatch == NULL)
            {
                // It was a fun match, so there is no need to do anything

                bz_debugMessage(DEBUG_LEVEL, "DEBUG :: League Overseer :: Fun match has completed.");
            }
            else if (officialMatch->canceled)
            {
                // The match was canceled for some reason so output the reason to both the players and the server logs

                bz_debugMessagef(DEBUG_LEVEL, "DEBUG :: League Overseer :: %s", officialMatch->cancelationReason.c_str());
                bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, officialMatch->cancelationReason.c_str());
            }
            else if (officialMatch->matchParticipants.empty())
            {
                // Oops... I darn goofed. Somehow the players were not recorded properly

                bz_debugMessage(DEBUG_LEVEL, "DEBUG :: League Overseer :: No recorded players for this official match.");
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
                bz_debugMessagef(0, "Match Data :: League Over Seer Match Report");
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
                bz_debugMessagef(0, "DEBUG :: League Overseer :: Reporting match data...");
                bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Reporting match...");

                //Send the match data to the league website
                bz_addURLJob(LEAGUE_URL.c_str(), this, matchToSend.c_str());
            }

            // We're done with the struct, so make it NULL until the next official match
            officialMatch = NULL;
        }
        break;

        case bz_eGameStartEvent: // This event is triggered when a timed game begins
        {
            bz_debugMessage(DEBUG_ALL, "DEBUG :: League Overseer :: A match has started");

            // We started recording a match, so save the status
            RECORDING = bz_startRecBuf();

            // We want to notify the logs if we couldn't start recording just in case an issue were to occur and the server
            // owner needs to check to see if players were lying about there no replay
            if (RECORDING)
            {
                bz_debugMessage(DEBUG_ALL, "DEBUG :: League Overseer :: Match recording has started successfully");
            }
            else
            {
                bz_debugMessage(0, "ERROR :: League Overseer :: This match could could not be recorded");
            }

            // Check if this is an official match
            if (officialMatch != NULL)
            {
                // Reset scores in case Caps happened during countdown delay.
                officialMatch->teamOnePoints = officialMatch->teamTwoPoints = 0;
                officialMatch->approxTimeProgress = bz_getCurrentTime();
                officialMatch->duration = bz_getTimeLimit();
            }
        }
        break;

        case bz_eGetPlayerMotto: // This event is called when the player joins. It gives us the motto of the player
        {
            bz_GetPlayerMottoData_V2* mottoData = (bz_GetPlayerMottoData_V2*)eventData;

            mottoData->motto = teamMottos[mottoData->record->bzID.c_str()];
        }
        break;

        case bz_ePlayerJoinEvent: // This event is called each time a player joins the game
        {
            bz_PlayerJoinPartEventData_V1* joinData = (bz_PlayerJoinPartEventData_V1*)eventData;

            // Only notify a player if they exist, have joined the observer team, and there is a match in progress
            if ((bz_isCountDownActive() || bz_isCountDownInProgress()) && isValidPlayerID(joinData->playerID) && joinData->record->team == eObservers)
            {
                bz_sendTextMessagef(BZ_SERVER, joinData->playerID, "*** There is currently %s match in progress, please be respectful. ***",
                                    ((officialMatch != NULL) ? "an official" : "a fun"));
            }

            // Only send a URL job if the user is verified
            if (joinData->record->verified)
            {
                requestTeamName(joinData->record->callsign.c_str(), joinData->record->bzID.c_str());
            }
        }
        break;

        case bz_eSlashCommandEvent: // This event is called each time a player sends a slash command
        {
            bz_SlashCommandEventData_V1* slashCommandData = (bz_SlashCommandEventData_V1*)eventData;

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
            else if (isdigit(atoi(command.c_str()) + 12))
            {
                bz_sendTextMessage(BZ_SERVER, playerID, "** '/countdown TIME' is disabled, please use /official or /fm instead **");
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

                // If there is a countdown active an no tanks are playing, then cancel it
                if (bz_isCountDownActive())
                {
                    bz_gameOver(253, eObservers);
                    bz_debugMessage(DEBUG_ALL, "DEBUG :: League Overseer :: Game ended because no players were found playing with an active countdown.");
                }
            }

            // Let's get the roll call only if there is an official match
            if (officialMatch != NULL)
            {
                // Check if the start time is not negative since our default value for the approxTimeProgress is -1. Also check
                // if it's time to do a roll call, which is defined as 90 seconds after the start of the match by default,
                // and make sure we don't have any match participants recorded and the match isn't paused
                if (officialMatch->approxTimeProgress >= 0.0f &&
                    officialMatch->approxTimeProgress + MATCH_ROLLCALL < bz_getCurrentTime() &&
                    officialMatch->matchParticipants.empty() &&
                    !bz_isCountDownPaused())
                {
                    bz_debugMessagef(DEBUG_ALL, "DEBUG :: League Overseer :: Processing roll call...");

                    std::unique_ptr<bz_APIIntList> playerList(bz_getPlayerIndexList());
                    bool invalidateRollcall, teamOneError, teamTwoError;
                    std::string teamOneMotto, teamTwoMotto;

                    invalidateRollcall = teamOneError = teamTwoError = false;
                    teamOneMotto = teamTwoMotto = "";

                    for (unsigned int i = 0; i < playerList->size(); i++)
                    {
                        std::unique_ptr<bz_BasePlayerRecord> playerRecord(bz_getPlayerByIndex(playerList->get(i)));

                        if (bz_getPlayerTeam(playerList->get(i)) != eObservers) // If player is not an observer
                        {
                            MatchParticipant currentPlayer(playerRecord->bzID.c_str(), playerRecord->callsign.c_str(),
                                                           playerRecord->ipAddress.c_str(), teamMottos[playerRecord->bzID.c_str()],
                                                           playerRecord->team);

                            // In order to see what is going wrong with the roll call if anything, display all of the player's information
                            bz_debugMessagef(DEBUG_ALL, "DEBUG :: League Overseer :: Adding player '%s' to roll call...", currentPlayer.callsign.c_str());
                            bz_debugMessagef(DEBUG_ALL, "DEBUG :: League Overseer ::   >  BZID       : %s", currentPlayer.bzID.c_str());
                            bz_debugMessagef(DEBUG_ALL, "DEBUG :: League Overseer ::   >  IP Address : %s", currentPlayer.ipAddress.c_str());
                            bz_debugMessagef(DEBUG_ALL, "DEBUG :: League Overseer ::   >  Team Name  : %s", currentPlayer.teamName.c_str());
                            bz_debugMessagef(DEBUG_ALL, "DEBUG :: League Overseer ::   >  Team Color : %s", formatTeam(currentPlayer.teamColor).c_str());

                            // Check if there is any need to invalidate a roll call from a team
                            validateTeamName(invalidateRollcall, teamOneError, currentPlayer, teamOneMotto, TEAM_ONE);
                            validateTeamName(invalidateRollcall, teamTwoError, currentPlayer, teamTwoMotto, TEAM_TWO);

                            if (currentPlayer.bzID.empty()) // Someone is playing without a BZID, how did this happen?
                            {
                                invalidateRollcall = true;
                                bz_debugMessagef(DEBUG_ALL, "ERROR :: League Overseer :: Roll call has been marked as invalid due to '%s' not having a valid BZID.", currentPlayer.callsign.c_str());
                            }

                            // Add the player to the struct of participants
                            officialMatch->matchParticipants.push_back(currentPlayer);
                            bz_debugMessagef(DEBUG_ALL, "DEBUG :: League Overseer :: Player '%s' successfully added to the roll call.", currentPlayer.callsign.c_str());
                        }
                    }

                    // We were asked to invalidate the roll call because of some issue so let's check if there is still time for
                    // another roll call
                    if (invalidateRollcall && MATCH_ROLLCALL + 30 < officialMatch->duration)
                    {
                        bz_debugMessagef(DEBUG_LEVEL, "DEBUG :: League Overseer :: Invalid player found on field at %i:%i.", (int)(MATCH_ROLLCALL/60), (int)(fmod(MATCH_ROLLCALL,60.0)));

                        // There was an error with one of the members of either team, so request a team name update for all of
                        // the team members to try to fix any inconsistencies of different team names
                        if (teamOneError) { requestTeamName(TEAM_ONE); }
                        if (teamTwoError) { requestTeamName(TEAM_TWO); }

                        // Delay the next roll call by 60 seconds
                        MATCH_ROLLCALL += 60;
                        bz_debugMessagef(DEBUG_ALL, "DEBUG :: League Overseer :: Match roll call time has been delayed by 60 seconds.");

                        // Clear the struct because it's useless data
                        officialMatch->matchParticipants.clear();
                        bz_debugMessagef(DEBUG_ALL, "DEBUG :: League Overseer :: Match participants have been cleared.");
                    }

                    // There is no need to invalidate the roll call so the team names must be right so save them in the struct
                    if (!invalidateRollcall)
                    {
                        officialMatch->teamOneName = teamOneMotto;
                        officialMatch->teamTwoName = teamTwoMotto;

                        bz_debugMessagef(DEBUG_ALL, "DEBUG :: League Overseer :: Team One set to: %s", officialMatch->teamOneName.c_str());
                        bz_debugMessagef(DEBUG_ALL, "DEBUG :: League Overseer :: Team Two set to: %s", officialMatch->teamTwoName.c_str());
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
    std::unique_ptr<bz_BasePlayerRecord> playerData(bz_getPlayerByIndex(playerID));

    if (!playerData->verified || !bz_hasPerm(playerID, "spawn"))
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

            bz_debugMessagef(DEBUG_LEVEL, "DEBUG :: League Overseer :: Match ended by %s (%s).", playerData->callsign.c_str(), playerData->ipAddress.c_str());
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
                if (officialMatch->approxTimeProgress >= 0.0f && officialMatch->approxTimeProgress + (officialMatch->duration/2) < bz_getCurrentTime())
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
    else if (command == "fm")
    {
        if (playerData->team == eObservers) // Observers can't start matches
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
            bz_debugMessagef(DEBUG_LEVEL, "DEBUG :: League Overseer :: Fun match started by %s (%s).", playerData->callsign.c_str(), playerData->ipAddress.c_str());
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
    else if (command == "official")
    {
        if (playerData->team == eObservers) // Observers can't start matches
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
            bz_debugMessagef(DEBUG_LEVEL, "DEBUG :: League Overseer :: Official match started by %s (%s).", playerData->callsign.c_str(), playerData->ipAddress.c_str());
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
    else if (command == "pause")
    {
        if (bz_isCountDownPaused())
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "The match is already paused!");
        }
        else if (bz_isCountDownActive())
        {
            bz_pauseCountdown(playerData->callsign.c_str());

            // We've paused an official match, so we need to delay the approxTimeProgress in order to calculate the roll call time properly
            if (officialMatch != NULL)
            {
                officialMatch->timePaused = bz_getCurrentTime();
                bz_debugMessagef(DEBUG_ALL, "DEBUG :: League Overseer :: Match paused at %f, in server seconds.", officialMatch->timePaused);
            }
        }
        else
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "There is no active match to pause right now.");
        }

        return true;
    }
    else if (command == "resume")
    {
        if (!bz_isCountDownPaused())
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "The match is not paused!");
        }
        else if (bz_isCountDownActive())
        {
            bz_resumeCountdown(playerData->callsign.c_str());

            // We've resumed an official match, so we need to properly edit the start time so we can calculate the roll call
            if (officialMatch != NULL)
            {
                officialMatch->approxTimeProgress -= bz_getCurrentTime() - officialMatch->timePaused - 5;
                bz_debugMessagef(DEBUG_ALL, "DEBUG :: League Overseer :: Match progress time set to %f, in server seconds.", officialMatch->approxTimeProgress);
            }
        }
        else
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "There is no active match to resume right now.");
        }

        return true;
    }
    else if (command == "spawn")
    {
        if (bz_hasPerm(playerID, "ban"))
        {
            if (params->size() > 0)
            {
                bz_debugMessagef(DEBUG_ALL, "DEBUG :: League Overseer :: %s has executed the /spawn command.", playerData->callsign.c_str());

                std::string callsignToLookup; // Store the callsign we're going to search for

                for (unsigned int i = 0; i < params->size(); i++) // Piece together the callsign from the slash command parameters
                {
                    callsignToLookup += params->get(i).c_str();

                    if (i != params->size() - 1) // So we don't stick a whitespace on the end
                    {
                        callsignToLookup += " "; // Add a whitespace between each chat text parameter
                    }
                }

                bz_debugMessagef(DEBUG_ALL, "DEBUG :: League Overseer :: Callsign or Player slot to look for: %s", callsignToLookup.c_str());

                // We have a pound sign followed by a valid player index
                if (std::string::npos != std::string(params->get(0).c_str()).find("#") && isValidPlayerID(atoi(std::string(params->get(0).c_str()).erase(0, 1).c_str())))
                {
                    // Let's make some easy reference variables
                    int victimPlayerID = atoi(std::string(params->get(0).c_str()).erase(0, 1).c_str());
                    std::unique_ptr<bz_BasePlayerRecord> victim(bz_getPlayerByIndex(victimPlayerID));

                    bz_grantPerm(victim->playerID, "spawn");
                    bz_sendTextMessagef(BZ_SERVER, eAdministrators, "%s granted %s the ability to spawn.", playerData->callsign.c_str(), victim->callsign.c_str());
                }
                else if (bz_getPlayerByCallsign(callsignToLookup.c_str()) != NULL) // It's a valid callsign
                {
                    std::unique_ptr<bz_BasePlayerRecord> victim(bz_getPlayerByCallsign(callsignToLookup.c_str()));

                    bz_grantPerm(victim->playerID, "spawn");
                    bz_sendTextMessagef(BZ_SERVER, eAdministrators, "%s granted %s the ability to spawn.", playerData->callsign.c_str(), victim->callsign.c_str());
                }
                else
                {
                    bz_sendTextMessagef(BZ_SERVER, playerID, "player %s not found", params->get(0).c_str());
                    bz_debugMessagef(DEBUG_ALL, "DEBUG :: League Overseer :: Player %s was not found.", callsignToLookup.c_str());
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
}

// We got a response from one of our URL jobs
void LeagueOverseer::URLDone(const char* /*URL*/, const void* data, unsigned int /*size*/, bool /*complete*/)
{
    // Convert the data we get from the URL job to a std::string
    std::string siteData = (const char*)(data);
    bz_debugMessagef(DEBUG_LEVEL, "DEBUG :: League Overseer :: URL Job returned: %s", siteData.c_str());

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
                    bz_debugMessage(DEBUG_ALL, "DEBUG :: League Overseer :: Team dump JSON data received.");

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

                                    bz_debugMessagef(DEBUG_ALL, "DEBUG :: League Overseer :: Team '%s' recorded. Getting team members...", teamName.c_str());
                                }
                                // Our second key is going to be the team members' BZIDs seperated by commas
                                else if (strcmp(_key, "members") == 0)
                                {
                                    // Now we need to handle each BZID separately so we will split the elements
                                    // by each comma and stuff it into a vector
                                    std::vector<std::string> bzIDs = split(json_object_get_string(_value), ',');

                                    // Now iterate through the vector of team members so we can save them to our
                                    // map
                                    for (std::vector<std::string>::const_iterator it = bzIDs.begin(); it != bzIDs.end(); ++it)
                                    {
                                        std::string bzID = std::string(*it);
                                        teamMottos[bzID.c_str()] = teamName.c_str();

                                        bz_debugMessagef(DEBUG_ALL, "DEBUG :: League Overseer :: BZID %s set to team %s.", bzID.c_str(), teamName.c_str());
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
                    bz_debugMessage(DEBUG_ALL, "DEBUG :: League Overseer :: Team name JSON data received.");

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

            bz_debugMessagef(DEBUG_ALL, "DEBUG :: League Overseer :: Motto saved for BZID %s.", urlJobBZID.c_str());

            // If the team name is equal to an empty string that means a player is teamless and if they are in our motto
            // map, that means they recently left a team so remove their entry in the map
            if (urlJobTeamName == "" && teamMottos.count(urlJobBZID))
            {
                teamMottos.erase(urlJobBZID);
            }
        }
    }
    else if (siteData.find("<html>") == std::string::npos)
    {
        bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%s", siteData.c_str());
        bz_debugMessagef(DEBUG_LEVEL, "%s", siteData.c_str());
    }
}

// The league website is down or is not responding, the request timed out
void LeagueOverseer::URLTimeout(const char* /*URL*/, int /*errorCode*/)
{
    bz_debugMessage(DEBUG_LEVEL, "WARNING :: League Overseer :: The request to the league site has timed out.");
}

// The server owner must have set up the URLs wrong because this shouldn't happen
void LeagueOverseer::URLError(const char* /*URL*/, int errorCode, const char *errorString)
{
    bz_debugMessage(DEBUG_LEVEL, "ERROR :: League Overseer :: Match report failed with the following error:");
    bz_debugMessagef(DEBUG_LEVEL, "ERROR :: League Overseer :: Error code: %i - %s", errorCode, errorString);
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

// Load the plugin configuration file
void LeagueOverseer::loadConfig(const char* cmdLine)
{
    PluginConfig config = PluginConfig(cmdLine);
    std::string section = "leagueOverSeer";

    // Shutdown the server if the configuration file has errors because we can't do anything
    // with a broken config
    if (config.errors) { bz_shutdown(); }

    // Extract all the data in the configuration file and assign it to plugin variables
    ROTATION_LEAGUE = toBool(config.item(section, "ROTATIONAL_LEAGUE"));
    MAPCHANGE_PATH  = config.item(section, "MAPCHANGE_PATH");
    LEAGUE_URL      = config.item(section, "LEAGUE_OVER_SEER_URL");
    DEBUG_LEVEL     = atoi((config.item(section, "DEBUG_LEVEL")).c_str());
    DEBUG_ALL       = atoi((config.item(section, "DEBUG_ALL")).c_str());

    // Sanity check for our debug level, if it doesn't pass the check then set the debug level to 1
    if (DEBUG_LEVEL > 4 || DEBUG_LEVEL < 0)
    {
        bz_debugMessage(0, "WARNING :: League Overseer :: Invalid debug level in the configuration file.");
        bz_debugMessage(0, "WARNING :: League Overseer :: Debug level set to the default: 1.");
        DEBUG_LEVEL = 1;
    }

    // We don't need to advertise that DEBUG_ALL failed so let's set it to 4, which is the default
    if (DEBUG_ALL > 4 || DEBUG_ALL < 0)
    {
        DEBUG_ALL = 4;
    }

    // Check for errors in the configuration data. If there is an error, shut down the server
    if (strcmp(LEAGUE_URL.c_str(), "") == 0)
    {
        bz_debugMessage(0, "ERROR :: League Overseer :: No URLs were choosen to report matches or query teams.");
        bz_debugMessage(0, "ERROR :: League Overseer :: I won't be able to do anything without a URL, shutting down.");
        bz_shutdown();
    }
}

// Request a team name update for all the members of a team
void LeagueOverseer::requestTeamName (bz_eTeamType team)
{
    bz_debugMessagef(DEBUG_ALL, "DEBUG :: League Overseer :: A team name update for the %s team has been requested.", formatTeam(team).c_str());
    std::unique_ptr<bz_APIIntList> playerList(bz_getPlayerIndexList());

    for (unsigned int i = 0; i < playerList->size(); i++)
    {
        std::unique_ptr<bz_BasePlayerRecord> playerRecord(bz_getPlayerByIndex(playerList->get(i)));

        if (playerRecord->team == team) // Only request a new team name for the players of a certain team
        {
            bz_debugMessagef(DEBUG_ALL, "DEBUG :: League Overseer :: Player '%s' is a part of the %s team.", playerRecord->callsign.c_str(), formatTeam(team).c_str());
            requestTeamName(playerRecord->callsign.c_str(), playerRecord->bzID.c_str());
        }
    }
}

// Because there will be different times where we request a team name motto, let's make into a function
void LeagueOverseer::requestTeamName (std::string callsign, std::string bzID)
{
    // Build the POST data for the URL job
    std::string teamMotto = "query=teamNameQuery&apiVersion=" + intToString(API_VERSION);
    teamMotto += "&teamPlayers=" + std::string(bzID.c_str());

    bz_debugMessagef(DEBUG_LEVEL, "DEBUG :: League Overseer :: Sending motto request for '%s'", callsign.c_str());

    // Send the team update request to the league website
    bz_addURLJob(LEAGUE_URL.c_str(), this, teamMotto.c_str());
}

// Check if there is any need to invalidate a roll call team
void LeagueOverseer::validateTeamName (bool &invalidate, bool &teamError, MatchParticipant currentPlayer, std::string &teamName, bz_eTeamType team)
{
    bz_debugMessagef(DEBUG_ALL, "DEBUG :: League Overseer :: Starting validation of the %s team.", formatTeam(team).c_str());

    // Check if the player is a part of the team we're validating
    if (currentPlayer.teamColor == team)
    {
        // Check if the team name of team one has been set yet, if it hasn't then set it
        // and we'll be able to set it so we can conclude that we have the same team for
        // all of the players
        if (teamName == "")
        {
            teamName = currentPlayer.teamName;
            bz_debugMessagef(DEBUG_ALL, "DEBUG :: League Overseer :: The team name for the %s team has been set to: %s", formatTeam(team).c_str(), teamName.c_str());
        }
        // We found someone with a different team name, therefore we need invalidate the
        // roll call and check all of the member's team names for sanity
        else if (teamName != currentPlayer.teamName)
        {
            bz_debugMessagef(DEBUG_ALL, "ERROR :: League Overseer :: Player '%s' is not part of the '%s' team.", currentPlayer.callsign.c_str(), teamName.c_str());
            invalidate = true; // Invalidate the roll call
            teamError = true;  // We need to check team one's members for their teams
        }
    }

    bz_debugMessagef(DEBUG_ALL, "DEBUG :: League Overseer :: Player '%s' belongs to the '%s' team.", currentPlayer.callsign.c_str(), currentPlayer.teamName.c_str());
}

void LeagueOverseer::updateTeamNames ()
{
    // Build the POST data for the URL job
    std::string teamNameDump = "query=teamDump&apiVersion=" + intToString(API_VERSION);
    bz_debugMessagef(DEBUG_ALL, "DEBUG :: League Over Seer :: Updating Team name database...");

    bz_addURLJob(LEAGUE_URL.c_str(), this, teamNameDump.c_str()); //Send the team update request to the league website
}