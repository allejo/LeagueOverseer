/*
League Overseer
    Copyright (C) 2013-2016 Vladimir Jimenez & Ned Anderson

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
#include <string>
#include <time.h>
#include <vector>

#include "bzfsAPI.h"
#include "plugin_utils.h"

// Define plugin version numbering
const int MAJOR = 1;
const int MINOR = 1;
const int REV = 3;
const int BUILD = 287;

// The API number used to notify the PHP counterpart about how to handle the data
const int API_VERSION = 1;

const double FM_MIN_RATIO = 2.0/7.0;
const double OFFI_MIN_TIME = 300.0;
const double IDLE_FORGIVENESS = 0.9;

// Log failed assertions at debug level 0 since this will work for non-member functions and it is important enough.
#define ASSERT(x) { if (!(x)) { bz_debugMessagef(0, "ERROR :: League Overseer :: Failed assertion '%s' at %s:%d", #x, __FILE__, __LINE__); }}

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
        static std::string pluginBuild = "";

        if (!pluginBuild.size())
        {
            std::ostringstream pluginBuildStream;

            pluginBuildStream << "League Overseer " << MAJOR << "." << MINOR << "." << REV << " (" << BUILD << ")";
            pluginBuild = pluginBuildStream.str();
        }

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
        // Basic player info
        int slotID;
        std::string bzID;
        std::string callsign;
        std::string ipAddress;

        std::string teamName;
        bz_eTeamType teamColor;

        bool hasSpawned; // Set to true if the player has ever spawned in the match

        double startTime;     // The time the player started playing their last session
        double lastDeathTime; // The time the player last died
        double totalPlayTime; // The total amount of time a player has played in a match in seconds
        double totalIdleTime; // An estimated amount of idle time a player has had during the match

        // The amount of seconds a player has played on each respective team
        std::map<bz_eTeamType, double> playTimeByTeam;

        MatchParticipant() :
            slotID(-1),
            startTime(-1),
            hasSpawned(false),
            totalIdleTime(0),
            totalPlayTime(0)
        {}

        MatchParticipant(bz_BasePlayerRecord *pr)
        {
            MatchParticipant();

            slotID    = pr->playerID;
            bzID      = pr->bzID;
            callsign  = pr->callsign;
            ipAddress = pr->ipAddress;
            teamColor = pr->team;
        }

        bz_eTeamType getLoyalty(bz_eTeamType team1, bz_eTeamType team2)
        {
            if (playTimeByTeam[team1] >= playTimeByTeam[team2])
            {
                return team1;
            }

            return team2;
        }

        void updatePlayingTime (bz_eTeamType team)
        {
            if (!hasSpawned)
            {
                return;
            }

            double sessionPlaytime = bz_getCurrentTime() - startTime;

            totalPlayTime += sessionPlaytime;
            playTimeByTeam[team] += sessionPlaytime;
            startTime = -1;
        }
    };

    // Simply out of preference, we will be storing all the information regarding a match inside
    // of a struct where the struct will be NULL if it is current a fun match
    struct CurrentMatch
    {
        bool        playersRecorded,    // Whether or not the players participating in the match have been recorded or not
                    isOfficialMatch,    // Whether or not the match is official; when set to false it'll be a fun match
                    canceled;           // Whether or not the official match was canceled

        std::string cancelationReason,  // If the match was canceled, store the reason as to why it was canceled
                    teamOneName,        // We'll be doing our best to store the team names of who's playing so store the names for
                    teamTwoName;        //     each team respectively. This will be blank if it's a fun match

        double      duration,           // The length of the match in seconds. Used when reporting a match to the server
                    matchRollCall;      // The amount of seconds that need to pass in a match before the first roll call

        // We keep the number of points scored in the case where all the members of a team leave and their team
        // score get reset to 0
        int         teamOnePoints,
                    teamTwoPoints;

        time_t      matchStart,         // The timestamp of when a match was started in order to calculate the timer
                    matchPaused;        // If the match is paused, it will be stored here in order to update matchStart appropriately for the timer

        std::map<std::string, MatchParticipant> matchRoster;

        // Set the default values for this struct
        CurrentMatch () :
            playersRecorded(false),
            isOfficialMatch(true),
            canceled(false),
            cancelationReason(""),
            teamOneName("Team-A"),
            teamTwoName("Team-B"),
            duration(-1.0f),
            matchRollCall(90.0),
            teamOnePoints(0),
            teamTwoPoints(0),
            matchStart(time(NULL)),
            matchPaused(time(NULL)),
            matchRoster()
        {}
    };

    virtual void buildPlayerStrings (bz_eTeamType team, std::string &bzidString, std::string &ipString);
    virtual int getMatchProgress ();
    virtual std::string getMatchTime ();
    virtual void loadConfig (const char *cmdLine);
    virtual void requestTeamName (bz_eTeamType team);
    virtual void requestTeamName (std::string callsign, std::string bzID);
    virtual void updateTeamNames (void);

    // All the variables that will be used in the plugin
    bool         ROTATION_LEAGUE,  // Whether or not we are watching a league that uses different maps
                 MATCH_INFO_SENT,  // Whether or not the information returned by a URL job pertains to a match report
                 DISABLE_REPORT,   // Whether or not to disable automatic match reports if a server is not used as an official match server
                 DISABLE_MOTTO,    // Whether or not to set a player's motto to their team name
                 RECORDING;        // Whether or not we are recording a match

    int          DEBUG_LEVEL,      // The DEBUG level the server owner wants the plugin to use for its messages
                 VERBOSE_LEVEL;    // This is the spamming/ridiculous level of debug that the plugin uses

    std::string  MATCH_REPORT_URL, // The URL the plugin will use to report matches. This should be the URL the PHP counterpart of this plugin
                 TEAM_NAME_URL,
                 MAP_NAME,         // The name of the map that is currently be played if it's a rotation league (i.e. OpenLeague uses multiple maps)
                 MAPCHANGE_PATH;   // The path to the file that contains the name of current map being played

    bz_eTeamType TEAM_ONE,         // Because we're serving more than just GU league, we need to support different colors therefore, call the teams
                 TEAM_TWO;         //     ONE and TWO

    // This is the only pointer of the struct for the official match that we will be using. If this
    // variable is set to NULL, that means that there is currently no official match occurring.
    std::unique_ptr<CurrentMatch> currentMatch;

    // We will be using a map to handle the team name mottos in the format of
    // <BZID, Team Name>
    std::map<std::string, std::string> teamMottos;
};

BZ_PLUGIN(LeagueOverseer)

void LeagueOverseer::Init (const char* commandLine)
{
    // Register our events with Register()
    Register(bz_eGameEndEvent);
    Register(bz_eGameResumeEvent);
    Register(bz_eGameStartEvent);
    Register(bz_eGetPlayerMotto);
    Register(bz_ePlayerDieEvent);
    Register(bz_ePlayerJoinEvent);
    Register(bz_ePlayerPartEvent);
    Register(bz_ePlayerSpawnEvent);
    Register(bz_eTeamScoreChanged);
    Register(bz_eTickEvent);

    // Register our custom slash commands
    bz_registerCustomSlashCommand("cancel", this);
    bz_registerCustomSlashCommand("finish", this);
    bz_registerCustomSlashCommand("fm", this);
    bz_registerCustomSlashCommand("offi", this);
    bz_registerCustomSlashCommand("official", this);
    bz_registerCustomSlashCommand("spawn", this);
    bz_registerCustomSlashCommand("pause", this);
    bz_registerCustomSlashCommand("resume", this);

    bz_registerCustomSlashCommand("gameover", this);
    bz_registerCustomSlashCommand("countdown", this);

    // Set some default values
    currentMatch = NULL;

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

    if (bz_getTimeLimit() == 0.0)
    {
        bz_debugMessage(DEBUG_LEVEL, "WARNING :: League Overseer :: No time limit is specified with '-time'. Default value used: 1800 seconds.");
        bz_setTimeLimit(1800);
    }
}

void LeagueOverseer::Cleanup (void)
{
    Flush(); // Clean up all the events

    // Clean up our custom slash commands
    bz_removeCustomSlashCommand("cancel");
    bz_removeCustomSlashCommand("finish");
    bz_removeCustomSlashCommand("fm");
    bz_removeCustomSlashCommand("offi");
    bz_removeCustomSlashCommand("official");
    bz_removeCustomSlashCommand("spawn");
    bz_removeCustomSlashCommand("pause");
    bz_removeCustomSlashCommand("resume");

    bz_removeCustomSlashCommand("gameover");
    bz_removeCustomSlashCommand("countdown");
}

void LeagueOverseer::Event (bz_EventData *eventData)
{
    switch (eventData->eventType)
    {
        case bz_eGameEndEvent: // This event is called each time a game ends
        {
            bz_debugMessage(VERBOSE_LEVEL, "DEBUG :: League Overseer :: A match has ended.");

            // Get the current standard UTC time
            bz_Time standardTime;
            bz_getUTCtime(&standardTime);
            std::string recordingFileName;

            // Only save the recording buffer if we actually started recording when the match started
            if (RECORDING)
            {
                bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer :: Recording was in progress during the match.");

                // We'll be formatting the file name, so create a variable to store it
                char tempRecordingFileName[512];

                // Let's get started with formatting
                if (currentMatch->isOfficialMatch)
                {
                    // If the official match was finished, then mark it as canceled
                    std::string matchCanceled = (currentMatch->canceled) ? "-Canceled" : "",
                                _teamOneName  = currentMatch->teamOneName.c_str(),
                                _teamTwoName  = currentMatch->teamTwoName.c_str(),
                                _matchTeams   = "";

                    // We want to standardize the names, so replace all spaces with underscores and
                    // any weird HTML symbols should have been stripped already by the PHP script
                    std::replace(_teamOneName.begin(), _teamOneName.end(), ' ', '_');
                    std::replace(_teamTwoName.begin(), _teamTwoName.end(), ' ', '_');

                    if (!currentMatch->matchRoster.empty())
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
                bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer :: Replay file will be named: %s", recordingFileName.c_str());

                // Save the recording buffer and stop recording
                bz_saveRecBuf(recordingFileName.c_str(), 0);
                bz_stopRecBuf();
                bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer :: Replay file has been saved and recording has stopped.");

                // We're no longer recording, so set the boolean and announce to players that the file has been saved
                RECORDING = false;
                bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Match saved as: %s", recordingFileName.c_str());
            }

            std::unique_ptr<bz_APIIntList> playerList(bz_getPlayerIndexList());

            // We can't do a roll call if the player list wasn't created
            if (!playerList)
            {
                bz_debugMessagef(VERBOSE_LEVEL, "ERROR :: League Overseer :: Failure to create player list for roll call.");
                return;
            }

            for (unsigned int i = 0; i < playerList->size(); i++)
            {
                std::unique_ptr<bz_BasePlayerRecord> playerRecord(bz_getPlayerByIndex(playerList->get(i)));

                if (playerRecord && bz_getPlayerTeam(playerList->get(i)) != eObservers) // If player is not an observer
                {
                    std::string bzid = playerRecord->bzID;

                    MatchParticipant &player = currentMatch->matchRoster[bzid];

                    player.updatePlayingTime(playerRecord->team);
                }
            }

            if (!DISABLE_REPORT)
            {
                if (currentMatch->canceled)
                {
                    // The match was canceled for some reason so output the reason to both the players and the server logs

                    bz_debugMessagef(DEBUG_LEVEL, "DEBUG :: League Overseer :: %s", currentMatch->cancelationReason.c_str());
                    bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, currentMatch->cancelationReason.c_str());
                }
                else if (currentMatch->matchRoster.empty())
                {
                    // Oops... I darn goofed. Somehow the players were not recorded properly

                    bz_debugMessage(DEBUG_LEVEL, "DEBUG :: League Overseer :: No recorded players for this official match.");
                    bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Current match could not be reported due to not having a list of valid match participants.");
                }
                else
                {
                    // This is a completed match (official or fm), so let's report it

                    // Format the date to -> year-month-day hour:minute:second
                    char matchDate[20];
                    sprintf(matchDate, "%02d-%02d-%02d %02d:%02d:%02d", standardTime.year, standardTime.month, standardTime.day, standardTime.hour, standardTime.minute, standardTime.second);

                    // Keep references to values for quick reference
                    std::string teamOnePointsFinal = intToString(currentMatch->teamOnePoints);
                    std::string teamTwoPointsFinal = intToString(currentMatch->teamTwoPoints);
                    std::string matchDuration      = intToString(currentMatch->duration/60);
                    std::string matchType          = (currentMatch->isOfficialMatch) ? "official" : "fm";

                    // Store match data in the logs
                    bz_debugMessagef(0, "Match Data :: League Overseer Match Report");
                    bz_debugMessagef(0, "Match Data :: -----------------------------");
                    bz_debugMessagef(0, "Match Data :: Match Time      : %s", matchDate);
                    bz_debugMessagef(0, "Match Data :: Duration        : %s", matchDuration.c_str());
                    bz_debugMessagef(0, "Match Data :: %s  Score  : %s", formatTeam(TEAM_ONE, true).c_str(), teamOnePointsFinal.c_str());
                    bz_debugMessagef(0, "Match Data :: %s  Score  : %s", formatTeam(TEAM_TWO, true).c_str(), teamTwoPointsFinal.c_str());

                    // Start building POST data to be sent to the league website
                    std::string matchToSend = "query=reportMatch";
                                matchToSend += "&apiVersion="   + std::string(bz_urlEncode(intToString(API_VERSION).c_str()));
                                matchToSend += "&matchType="    + std::string(bz_urlEncode(matchType.c_str()));
                                matchToSend += "&teamOneColor=" + std::string(bz_urlEncode(formatTeam(TEAM_ONE).c_str()));
                                matchToSend += "&teamTwoColor=" + std::string(bz_urlEncode(formatTeam(TEAM_TWO).c_str()));
                                matchToSend += "&teamOneWins="  + std::string(bz_urlEncode(teamOnePointsFinal.c_str()));
                                matchToSend += "&teamTwoWins="  + std::string(bz_urlEncode(teamTwoPointsFinal.c_str()));
                                matchToSend += "&duration="     + std::string(bz_urlEncode(matchDuration.c_str()));
                                matchToSend += "&matchTime="    + std::string(bz_urlEncode(matchDate));
                                matchToSend += "&server="       + std::string(bz_urlEncode(bz_getPublicAddr().c_str()));
                                matchToSend += "&port="         + std::string(bz_urlEncode(intToString(bz_getPublicPort()).c_str()));
                                matchToSend += "&replayFile="   + std::string(bz_urlEncode(recordingFileName.c_str()));

                    // Only add this parameter if it's a rotational league such as Leagues United
                    if (ROTATION_LEAGUE)
                    {
                        matchToSend += "&mapPlayed=" + std::string(bz_urlEncode(MAP_NAME.c_str()));
                    }

                    std::string teamOneBZIDs, teamOneIPs,
                                teamTwoBZIDs, teamTwoIPs;

                    buildPlayerStrings(TEAM_ONE, teamOneBZIDs, teamOneIPs);
                    buildPlayerStrings(TEAM_TWO, teamTwoBZIDs, teamTwoIPs);

                    // Build a string of BZIDs and also output the BZIDs to the server logs while we're at it
                    matchToSend += "&teamOnePlayers=" + teamOneBZIDs;
                    matchToSend += "&teamTwoPlayers=" + teamTwoBZIDs;

                    // Send the IPs that players used during the match
                    matchToSend += "&teamOneIPs=" + teamOneIPs;
                    matchToSend += "&teamTwoIPs=" + teamTwoIPs;

                    // Finish prettifying the server logs
                    bz_debugMessagef(0, "Match Data :: -----------------------------");
                    bz_debugMessagef(0, "Match Data :: End of Match Report");
                    bz_debugMessagef(0, "DEBUG :: League Overseer :: Reporting match data...");
                    bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Reporting match...");

                    // Send the match data to the league website
                    bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer :: Post data submitted: %s", matchToSend.c_str());
                    bz_addURLJob(MATCH_REPORT_URL.c_str(), this, matchToSend.c_str());
                    MATCH_INFO_SENT = true;
                }
            }

            // We're done with the struct, so make it NULL until the next match
            currentMatch = NULL;
        }
        break;

        case bz_eGameResumeEvent:
        {
            // We've resumed an official match, so we need to properly edit the start time so we can calculate the roll call
            time_t now = time(NULL);
            double timePaused = difftime(now, currentMatch->matchPaused);

            struct tm modMatchStart = *localtime(&currentMatch->matchStart);
            modMatchStart.tm_sec += timePaused;

            currentMatch->matchStart = mktime(&modMatchStart);
            bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer :: Match paused for %.f seconds. Match continuing at %s.", timePaused, getMatchTime().c_str());

            // Go through our roster and offset their playing time to behave like there was never a game pause
            for (auto &kv : currentMatch->matchRoster)
            {
                if (kv.second.startTime >= 0)
                {
                    kv.second.startTime += timePaused;
                }
            }
        }
        break;

        case bz_eGameStartEvent: // This event is triggered when a timed game begins
        {
            bz_debugMessage(VERBOSE_LEVEL, "DEBUG :: League Overseer :: A match has started");

            // We started recording a match, so save the status
            RECORDING = bz_startRecBuf();

            // We want to notify the logs if we couldn't start recording just in case an issue were to occur and the server
            // owner needs to check to see if players were lying about there no replay
            if (RECORDING)
            {
                bz_debugMessage(VERBOSE_LEVEL, "DEBUG :: League Overseer :: Match recording has started successfully");
            }
            else
            {
                bz_debugMessage(0, "ERROR :: League Overseer :: This match could not be recorded");
            }

            // Reset scores in case Caps happened during countdown delay.
            currentMatch->teamOnePoints = currentMatch->teamTwoPoints = 0;
            currentMatch->matchStart = time(NULL);
            currentMatch->duration = bz_getTimeLimit();

            // Take an initial roll call of the players
            std::unique_ptr<bz_APIIntList> playerList(bz_getPlayerIndexList());

            // We can't do a roll call if the player list wasn't created
            if (!playerList)
            {
                bz_debugMessagef(VERBOSE_LEVEL, "ERROR :: League Overseer :: Failure to create player list for roll call.");
                return;
            }

            for (unsigned int i = 0; i < playerList->size(); i++)
            {
                std::unique_ptr<bz_BasePlayerRecord> playerRecord(bz_getPlayerByIndex(playerList->get(i)));

                if (playerRecord && bz_getPlayerTeam(playerList->get(i)) != eObservers) // If player is not an observer
                {
                    std::string bzid = playerRecord->bzID;

                    MatchParticipant currentPlayer(playerRecord.get());

                    currentPlayer.teamName = teamMottos[bzid];
                    currentPlayer.startTime = currentPlayer.lastDeathTime = bz_getCurrentTime();

                    currentMatch->matchRoster[bzid] = currentPlayer;

                    // Some helpful debug messages
                    bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer :: Adding player '%s' to roll call...", currentPlayer.callsign.c_str());
                    bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer ::   BZID       : %s", currentPlayer.bzID.c_str());
                    bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer ::   IP Address : %s", currentPlayer.ipAddress.c_str());
                    bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer ::   Team Name  : %s", currentPlayer.teamName.c_str());
                    bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer ::   Team Color : %s", formatTeam(currentPlayer.teamColor).c_str());
                    bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer ::   Start Time : %0.f", currentPlayer.startTime);
                }
            }
        }
        break;

        case bz_eGetPlayerMotto: // This event is called when the player joins. It gives us the motto of the player
        {
            bz_GetPlayerMottoData_V2* mottoData = (bz_GetPlayerMottoData_V2*)eventData;

            if (!DISABLE_MOTTO)
            {
                mottoData->motto = teamMottos[mottoData->record->bzID.c_str()];
            }
        }
        break;

        case bz_ePlayerDieEvent:
        {
            bz_PlayerDieEventData_V1 *dieData = (bz_PlayerDieEventData_V1*)eventData;

            if (currentMatch != NULL)
            {
                std::unique_ptr<bz_BasePlayerRecord> playerRecord(bz_getPlayerByIndex(dieData->playerID));
                MatchParticipant &player = currentMatch->matchRoster[playerRecord->bzID];
                player.lastDeathTime = bz_getCurrentTime();
            }
        }
        break;

        case bz_ePlayerJoinEvent: // This event is called each time a player joins the game
        {
            bz_PlayerJoinPartEventData_V1* joinData = (bz_PlayerJoinPartEventData_V1*)eventData;

            // Only notify a player if they exist, have joined the observer team, and there is a match in progress
            if ((bz_isCountDownActive() || bz_isCountDownInProgress()) && isValidPlayerID(joinData->playerID) && joinData->record->team == eObservers)
            {
                bz_sendTextMessagef(BZ_SERVER, joinData->playerID, "*** There is currently %s match in progress, please be respectful. ***",
                                    ((currentMatch->isOfficialMatch) ? "an official" : "a fun"));
            }

            if (!DISABLE_MOTTO)
            {
                // Only send a URL job if the user is verified
                if (joinData->record->verified)
                {
                    requestTeamName(joinData->record->callsign.c_str(), joinData->record->bzID.c_str());
                }
            }

            if (bz_isCountDownActive() && joinData->record->team != eObservers)
            {
                MatchParticipant player(joinData->record);

                player.startTime = player.lastDeathTime = bz_getCurrentTime();
                player.teamName  = teamMottos[joinData->record->bzID];

                currentMatch->matchRoster[joinData->record->bzID] = player;

                // Some helpful debug messages
                bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer :: Adding player '%s' to roll call...", player.callsign.c_str());
                bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer ::   BZID       : %s", player.bzID.c_str());
                bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer ::   IP Address : %s", player.ipAddress.c_str());
                bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer ::   Team Name  : %s", player.teamName.c_str());
                bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer ::   Team Color : %s", formatTeam(player.teamColor).c_str());
                bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer ::   Start Time : %0.f", player.startTime);
            }
        }
        break;

        case bz_ePlayerPartEvent:
        {
            bz_PlayerJoinPartEventData_V1 *partData = (bz_PlayerJoinPartEventData_V1*)eventData;
            std::string bzid = partData->record->bzID;

            if (currentMatch != NULL && currentMatch->matchRoster.find(bzid) != currentMatch->matchRoster.end() && partData->record->team != eObservers)
            {
                MatchParticipant &participant = currentMatch->matchRoster[bzid];

                participant.updatePlayingTime(partData->record->team);

                bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer :: %s has left with %.0f seconds of playing time",
                                 participant.callsign.c_str(), participant.totalPlayTime);
            }
        }
        break;

        case bz_ePlayerSpawnEvent:
        {
            bz_PlayerSpawnEventData_V1 *spawnData = (bz_PlayerSpawnEventData_V1*)eventData;

            if (currentMatch != NULL)
            {
                std::unique_ptr<bz_BasePlayerRecord> playerRecord(bz_getPlayerByIndex(spawnData->playerID));

                if (playerRecord)
                {
                    MatchParticipant &player = currentMatch->matchRoster[playerRecord->bzID];

                    player.hasSpawned = true;
                    player.totalIdleTime += std::max(0.0, (bz_getCurrentTime() - player.lastDeathTime - (bz_getBZDBDouble("_explodeTime") * 1.5)));
                }
            }
        }
        break;

        case bz_eTeamScoreChanged:
        {
            bz_TeamScoreChangeEventData_V1* teamScoreChange = (bz_TeamScoreChangeEventData_V1*)eventData;

            // We only need to keep track of the store if it's a match
            if (currentMatch != NULL && teamScoreChange->element == bz_eWins)
            {
                (teamScoreChange->team == TEAM_ONE) ? currentMatch->teamOnePoints++ : currentMatch->teamTwoPoints++;

                bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer :: %s team scored.", formatTeam(teamScoreChange->team).c_str());
                bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer :: %s Match Score %s [%i] vs %s [%i]",
                                 (currentMatch->isOfficialMatch) ? "Official" : "Fun",
                                 formatTeam(TEAM_ONE).c_str(), currentMatch->teamOnePoints,
                                 formatTeam(TEAM_TWO).c_str(), currentMatch->teamTwoPoints);
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
                if (currentMatch != NULL)
                {
                    currentMatch->canceled = true;
                    currentMatch->cancelationReason = "Current match automatically canceled due to all players leaving the match.";
                }

                // If there is a countdown active an no tanks are playing, then cancel it
                if (bz_isCountDownActive())
                {
                    bz_gameOver(253, eObservers);
                    bz_debugMessage(VERBOSE_LEVEL, "DEBUG :: League Overseer :: Game ended because no players were found playing with an active countdown.");
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

    // For some reason, the player record could not be created
    if (!playerData)
    {
        return true;
    }

    // If the player is not verified and does not have the spawn permission, they can't use any of the commands
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
        else if (bz_isCountDownInProgress()) // Cancel the countdown if it's in progress
        {
            bz_cancelCountdown(playerID);

            currentMatch = NULL;
        }
        else if (bz_isCountDownActive()) // We can only cancel a match if the countdown is active
        {
            // We're canceling an official match
            if (currentMatch->isOfficialMatch)
            {
                currentMatch->canceled = true;
                currentMatch->cancelationReason = "Official match cancellation requested by " + std::string(playerData->callsign.c_str());
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
    else if (command == "countdown")
    {
        if (params->size() > 0)
        {
            if (params->get(0) == "pause")
            {
                bz_sendTextMessagef(BZ_SERVER, playerID, "** '/countdown pause' is disabled, please use /pause instead **");
            }
            else if (params->get(0) == "resume")
            {
                bz_sendTextMessagef(BZ_SERVER, playerID, "** '/countdown resume' is disabled, please use /resume instead **");
            }
            else if (params->get(0) == "cancel")
            {
                bz_sendTextMessagef(BZ_SERVER, playerID, "** '/countdown cancel' is disabled, please use /cancel instead **");
            }
            else
            {
                bz_sendTextMessage(BZ_SERVER, playerID, "** '/countdown TIME' is disabled, please use /official or /fm instead **");
            }
        }
        else
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "** '/countdown' is disabled, please use /official or /fm instead **");
        }

        return true;
    }
    else if (command == "finish")
    {
        if (playerData->team == eObservers) // Observers can't cancel matches
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "Observers are not allowed to finish matches.");
        }
        else if (bz_isCountDownInProgress()) // Refer them to the /cancel command to stop a countdown
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "Use the /cancel command to cancel the countdown.");
        }
        else if (bz_isCountDownActive()) // Only finish if the countdown is active
        {
            // We can only '/finish' official matches because I wanted to have a command only dedicated to
            // reporting partially completed matches
            if (currentMatch->isOfficialMatch)
            {
                // Let's check if we can report the match, in other words, at least half of the match has been reported
                if (getMatchProgress() >= currentMatch->duration / 2)
                {
                    bz_debugMessagef(DEBUG_LEVEL, "DEBUG :: League Overseer :: Official match ended early by %s (%s)", playerData->callsign.c_str(), playerData->ipAddress.c_str());
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
        else if (currentMatch != NULL || bz_isCountDownActive() || bz_isCountDownInProgress()) // There is already a countdown
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "There is already a game in progress; you cannot start another.");
        }
        else // They are verified, not an observer, there is no match. So start one
        {
            currentMatch.reset(new CurrentMatch());

            // We signify an FM whenever the 'currentMatch' variable is set to NULL so set it to null
            currentMatch->isOfficialMatch = false;

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
    else if (command == "gameover")
    {
        bz_sendTextMessagef(BZ_SERVER, playerID, "** '/gameover' is disabled, please use /finish or /cancel instead **");

        return true;
    }
    else if (command == "offi" || command == "official")
    {
        if (playerData->team == eObservers) // Observers can't start matches
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "Observers are not allowed to start matches.");
        }
        else if (bz_getTeamCount(TEAM_ONE) < 2 || bz_getTeamCount(TEAM_TWO) < 2) // An official match cannot be 1v1 or 2v1
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "You may not have an official match with less than 2 players per team.");
        }
        else if (currentMatch != NULL || bz_isCountDownActive() || bz_isCountDownInProgress()) // A countdown is in progress already
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "There is already a game in progress; you cannot start another.");
        }
        else // They are verified non-observer with valid team sizes and no existing match. Start one!
        {
            currentMatch.reset(new CurrentMatch());

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
            currentMatch->matchPaused = time(NULL);
            bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer :: Match paused at %s by %s.", getMatchTime().c_str(), playerData->callsign.c_str());
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

            bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer :: Match resumed by %s.", playerData->callsign.c_str());
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
                bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer :: %s has executed the /spawn command.", playerData->callsign.c_str());

                std::unique_ptr<bz_BasePlayerRecord> target(bz_getPlayerBySlotOrCallsign(params->get(0).c_str()));

                if (target)
                {
                    bz_grantPerm(target->playerID, "spawn");
                    bz_sendTextMessagef(BZ_SERVER, eAdministrators, "%s granted %s the ability to spawn.", playerData->callsign.c_str(), target->callsign.c_str());

                }
                else
                {
                    bz_sendTextMessagef(BZ_SERVER, playerID, "player %s not found", params->get(0).c_str());
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

    return false;
}

// We got a response from one of our URL jobs
void LeagueOverseer::URLDone(const char* /*URL*/, const void* data, unsigned int /*size*/, bool /*complete*/)
{
    // This variable will only be set to true for the duration of one URL job, so just set it back to false regardless
    MATCH_INFO_SENT = false;

    // Convert the data we get from the URL job to a std::string
    std::string siteData = (const char*)(data);
    bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer :: URL Job returned: %s", siteData.c_str());

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
                    bz_debugMessage(VERBOSE_LEVEL, "DEBUG :: League Overseer :: Team dump JSON data received.");

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

                                    bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer :: Team '%s' recorded. Getting team members...", teamName.c_str());
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

                                        bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer :: BZID %s set to team %s.", bzID.c_str(), teamName.c_str());
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
                    bz_debugMessage(VERBOSE_LEVEL, "DEBUG :: League Overseer :: Team name JSON data received.");

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

            bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer :: Motto saved for BZID %s.", urlJobBZID.c_str());

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

    if (MATCH_INFO_SENT)
    {
        MATCH_INFO_SENT = false;
        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "The match could not be reported due to the connection to the league site timing out.");
        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "If the league site is not down, please notify the server owner to reconfigure this plugin.");
    }
}

// The server owner must have set up the URLs wrong because this shouldn't happen
void LeagueOverseer::URLError(const char* /*URL*/, int errorCode, const char *errorString)
{
    bz_debugMessage(DEBUG_LEVEL, "ERROR :: League Overseer :: Match report failed with the following error:");
    bz_debugMessagef(DEBUG_LEVEL, "ERROR :: League Overseer :: Error code: %i - %s", errorCode, errorString);

    if (MATCH_INFO_SENT)
    {
        MATCH_INFO_SENT = false;
        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "An unknown error has occurred, please notify the server owner to reconfigure this plugin.");
    }
}

void LeagueOverseer::buildPlayerStrings (bz_eTeamType team, std::string &bzidString, std::string &ipString)
{
    if (currentMatch == NULL)
    {
        return;
    }

    // Send a debug message of the players on the specified team
    bz_debugMessagef(0, "Match Data :: %s Team Players", formatTeam(team).c_str());

    for (auto &kv : currentMatch->matchRoster)
    {
        MatchParticipant &player = kv.second;

        double estimatedPlayTime = (player.totalPlayTime - (player.totalIdleTime * IDLE_FORGIVENESS));

        bool officialMatchAndPlayTime = (currentMatch->isOfficialMatch && estimatedPlayTime >= OFFI_MIN_TIME); // It's an official match and they played at least the minimum time
        bool funMatchAndPlayTime = (!currentMatch->isOfficialMatch && estimatedPlayTime >= (currentMatch->duration * FM_MIN_RATIO)); // It's a fun match and they played at least the minimum time

        if (player.getLoyalty(TEAM_ONE, TEAM_TWO) == team)
        {
            if ((officialMatchAndPlayTime || funMatchAndPlayTime) && player.hasSpawned)
            {
                // Add the BZID of the player to string with a comma at the end
                bzidString += std::string(bz_urlEncode(player.bzID.c_str())) + ",";
                ipString   += std::string(bz_urlEncode(player.ipAddress.c_str())) + ",";
            }

            // Output their information to the server logs
            bz_debugMessagef(0, "Match Data ::   %s [%s] (%s)", player.callsign.c_str(), player.bzID.c_str(), player.ipAddress.c_str());
            bz_debugMessagef(0, "Match Data ::     %.0f seconds of estimated play time", estimatedPlayTime);

            if (!player.hasSpawned)
            {
                bz_debugMessagef(0, "Match Data ::     Player never spawned");
            }

            if (!(officialMatchAndPlayTime || funMatchAndPlayTime))
            {
                bz_debugMessagef(0, "Match Data ::     Failed to meet minimum playtime requirement: %0.f seconds", player.totalPlayTime);
            }
        }
    }

    // Return the comma separated string minus the last character because the loop will always
    // add an extra comma at the end. If we leave it, it will cause issues with the PHP counterpart
    // which tokenizes the BZIDs by commas and we don't want an empty BZID
    if (!bzidString.empty())
    {
        bzidString = bzidString.erase(bzidString.size() - 1);
    }
    if (!ipString.empty())
    {
        ipString = ipString.erase(ipString.size() - 1);
    }
}

// Return the progress of a match in seconds. For example, 20:00 minutes remaining would return 600
int LeagueOverseer::getMatchProgress()
{
    if (currentMatch != NULL)
    {
        time_t now = time(NULL);

        return difftime(now, currentMatch->matchStart);
    }

    return -1;
}

// Get the literal time remaining in a match in the format of MM:SS
std::string LeagueOverseer::getMatchTime()
{
    int time = getMatchProgress();

    // Let's covert the seconds of a match's progress into minutes and seconds
    int minutes = (currentMatch->duration/60) - ceil(time / 60.0);
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

// Load the plugin configuration file
void LeagueOverseer::loadConfig(const char* cmdLine)
{
	// Setup to read the configuration file
    PluginConfig config = PluginConfig(cmdLine);
    std::string section = "leagueOverSeer";

    // Set a default value
    VERBOSE_LEVEL  = -1;

    // Shutdown the server if the configuration file has errors because we can't do anything
    // with a broken config
    if (config.errors)
    {
        bz_debugMessage(0, "ERROR :: League Overseer :: Your configuration file contains errors. Shutting down...");
        bz_shutdown();
    }

    // Deprecated configuration file options
    if (!config.item(section, "DEBUG_ALL").empty())
    {
        bz_debugMessage(0, "WARNING :: League Overseer :: The 'DEBUG_ALL' configuration file option has been deprecated.");
        bz_debugMessage(0, "WARNING :: League Overseer :: Please use the 'VERBOSE_LEVEL' option instead.");

        VERBOSE_LEVEL = atoi((config.item(section, "DEBUG_ALL")).c_str());
    }
    if (!config.item(section, "LEAGUE_OVER_SEER_URL").empty())
    {
        bz_debugMessage(0, "WARNING :: League Overseer :: The 'LEAGUE_OVER_SEER_URL' configuration file option has been deprecated.");
        bz_debugMessage(0, "WARNING :: League Overseer :: Please use the 'LEAGUE_OVERSEER_URL' option instead.");

        MATCH_REPORT_URL = config.item(section, "LEAGUE_OVER_SEER_URL");
        TEAM_NAME_URL    = config.item(section, "LEAGUE_OVER_SEER_URL");
    }

    // Extract all the data in the configuration file and assign it to plugin variables
    ROTATION_LEAGUE = toBool(config.item(section, "ROTATIONAL_LEAGUE"));
    MAPCHANGE_PATH  = config.item(section, "MAPCHANGE_PATH");
    DISABLE_REPORT  = toBool(config.item(section, "DISABLE_MATCH_REPORT"));
    DISABLE_MOTTO   = toBool(config.item(section, "DISABLE_TEAM_MOTTO"));
    DEBUG_LEVEL     = atoi((config.item(section, "DEBUG_LEVEL")).c_str());
    VERBOSE_LEVEL   = (VERBOSE_LEVEL < 0) ? atoi((config.item(section, "VERBOSE_LEVEL")).c_str()) : VERBOSE_LEVEL;

    if (!config.item(section, "LEAGUE_OVERSEER_URL").empty())
    {
        MATCH_REPORT_URL = config.item(section, "LEAGUE_OVERSEER_URL");
        TEAM_NAME_URL    = config.item(section, "LEAGUE_OVERSEER_URL");
    }
    else
    {
        if (!DISABLE_REPORT)
        {
            if (!config.item(section, "MATCH_REPORT_URL").empty())
            {
                MATCH_REPORT_URL = config.item(section, "MATCH_REPORT_URL");
            }
            else
            {
                bz_debugMessage(0, "ERROR :: League Overseer :: You are requesting to report matches but you have not specified a URL to report to.");
                bz_debugMessage(0, "ERROR :: League Overseer :: Please set the 'MATCH_REPORT_URL' or 'LEAGUE_OVERSEER_URL' option respectively.");
                bz_debugMessage(0, "ERROR :: League Overseer :: If you do not wish to report matches, set 'DISABLE_MATCH_REPORT' to true.");
                bz_shutdown();
            }
        }

        if (!DISABLE_MOTTO)
        {
            if (!config.item(section, "MOTTO_FETCH_URL").empty())
            {
                TEAM_NAME_URL = config.item(section, "MOTTO_FETCH_URL");
            }
            else
            {
                bz_debugMessage(0, "ERROR :: League Overseer :: You have requested to fetch team names but have not specified a URL to fetch them from.");
                bz_debugMessage(0, "ERROR :: League Overseer :: Please set the 'MOTTO_FETCH_URL' or 'LEAGUE_OVERSEER_URL' option respectively.");
                bz_debugMessage(0, "ERROR :: League Overseer :: If you do not wish to team names for mottos, set 'DISABLE_TEAM_MOTTO' to true.");
                bz_shutdown();
            }
        }
    }

    // Sanity check for our debug level, if it doesn't pass the check then set the debug level to 1
    if (DEBUG_LEVEL > 4 || DEBUG_LEVEL < 0)
    {
        bz_debugMessage(0, "WARNING :: League Overseer :: Invalid debug level in the configuration file.");
        bz_debugMessage(0, "WARNING :: League Overseer :: Debug level set to the default: 1.");
        DEBUG_LEVEL = 1;
    }

    // We don't need to advertise that VERBOSE_LEVEL failed so let's set it to 4, which is the default
    if (VERBOSE_LEVEL > 4 || VERBOSE_LEVEL < 0 || config.item(section, "VERBOSE_LEVEL").empty())
    {
        VERBOSE_LEVEL = 4;
    }

    // Output the configuration settings
    bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer :: Configuration File Settings");
    bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer :: ---------------------------");
    bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer :: Rotational league set to  : %s", (ROTATION_LEAGUE) ? "true" : "false");

    if (ROTATION_LEAGUE)
    {
        bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer :: Map change path set to    : %s", MAPCHANGE_PATH.c_str());
    }

    if (!DISABLE_REPORT)
    {
        bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer :: Reporting matches to URL  : %s", MATCH_REPORT_URL.c_str());
    }

    if (!DISABLE_MOTTO)
    {
        bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer :: Fetching Team Names from  : %s", TEAM_NAME_URL.c_str());
    }

    bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer :: Debug level set to        : %d", DEBUG_LEVEL);
    bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer :: Verbose level set to      : %d", VERBOSE_LEVEL);
}

// Request a team name update for all the members of a team
void LeagueOverseer::requestTeamName (bz_eTeamType team)
{
    bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer :: A team name update for the '%s' team has been requested.", formatTeam(team).c_str());
    std::unique_ptr<bz_APIIntList> playerList(bz_getPlayerIndexList());

    // Our player list couldn't be created so exit out of here
    if (!playerList)
    {
        bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer :: The player list to process team names failed to be created.");
        return;
    }

    for (unsigned int i = 0; i < playerList->size(); i++)
    {
        std::unique_ptr<bz_BasePlayerRecord> playerRecord(bz_getPlayerByIndex(playerList->get(i)));

        if (playerRecord && playerRecord->team == team) // Only request a new team name for the players of a certain team
        {
            bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer :: Player '%s' is a part of the '%s' team.", playerRecord->callsign.c_str(), formatTeam(team).c_str());
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
    bz_addURLJob(TEAM_NAME_URL.c_str(), this, teamMotto.c_str());
}

void LeagueOverseer::updateTeamNames ()
{
    // Build the POST data for the URL job
    std::string teamNameDump = "query=teamDump&apiVersion=" + intToString(API_VERSION);
    bz_debugMessagef(VERBOSE_LEVEL, "DEBUG :: League Overseer :: Updating Team name database...");

    bz_addURLJob(TEAM_NAME_URL.c_str(), this, teamNameDump.c_str()); //Send the team update request to the league website
}
