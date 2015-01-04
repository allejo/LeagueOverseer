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

#include <memory>

#include "bzfsAPI.h"

#include "ConfigurationOptions.h"
#include "UrlQuery.h"

class LeagueOverseer : public bz_Plugin, public bz_CustomSlashCommandHandler, public bz_BaseURLHandler
{
    public:
        virtual const char* Name (void);
        virtual void Init (const char* config);
        virtual void Event (bz_EventData *eventData);
        virtual void Cleanup (void);

        virtual int  GeneralCallback (const char* name, void* data);

        virtual bool SlashCommand (int playerID, bz_ApiString, bz_ApiString, bz_APIStringList*);

        virtual void URLDone    (const char* URL, const void* data, unsigned int size, bool complete);
        virtual void URLTimeout (const char* URL, int errorCode);
        virtual void URLError   (const char* URL, int errorCode, const char *errorString);


        ///
        /// Structs we'll be using throughout the plug-in
        ///

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
                json(_json),
                bzID(_bzID),
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


        ///
        /// Custom functions defined
        ///

        virtual bz_BasePlayerRecord  *getPlayerFromCallsignOrID (std::string callsignOrID),
                                     *bz_getPlayerByCallsign (const char* callsign),
                                     *bz_getPlayerByBZID (const char* bzID);

        virtual std::string          getPlayerTeamNameByBZID (std::string bzID),
                                     getPlayerTeamNameByID (int playerID),
                                     buildBZIDString (bz_eTeamType team),
                                     getMatchTime (void);

        virtual bool                 isOfficialMatchInProgress (void),
                                     playerAlreadyJoined (std::string bzID),
                                     isMatchInProgress (void),
                                     isOfficialMatch (void),
                                     isLeagueMember (int playerID);

        virtual void                 validateTeamName (bool &invalidate, bool &teamError, MatchParticipant currentPlayer, std::string &teamName, bz_eTeamType team),
                                     removePlayerInfo (std::string bzID, std::string callsign),
                                     storePlayerInfo (int playerID, std::string bzID, std::string callsign),
                                     requestTeamName (std::string callsign, std::string bzID),
                                     requestTeamName (bz_eTeamType team),
                                     setLeagueMember (int playerID),
                                     resetTimeLimit (void);

        virtual int                  getMatchProgress (void);


        ///
        /// All of the instance variables used throughout the plug-in
        ///

        bool         IS_LEAGUE_MEMBER[256],  // Whether or not the player is a registered player for the league
                     MATCH_INFO_SENT,        // Whether or not the information returned by a URL job pertains to a match report
                     RECORDING;              // Whether or not we are recording a match

        double       LAST_CAP;               // The event time of the last flag capture

        time_t       MATCH_START,            // The timestamp of when a match was started in order to calculate the timer
                     MATCH_PAUSED;           // If the match is paused, it will be stored here in order to update MATCH_START appropriately for the timer

        std::string  CONFIG_PATH,            // The location of the configuration file so we can reload it if needed
                     MAP_NAME;               // The name of the map that is currently be played if it's a rotation league (i.e. OpenLeague uses multiple maps)

        bz_eTeamType CAP_VICTIM_TEAM,        // The team who had their flag captured most recently
                     CAP_WINNER_TEAM,        // The team who captured the flag most recently
                     TEAM_ONE,               // Because we're serving more than just GU league, we need to support different colors therefore, call the teams
                     TEAM_TWO;               //     ONE and TWO

        int          PC_PROTECTION_DELAY;    // The delay (in seconds) of how long the PC protection will be in effect

        UrlQuery     TeamUrlRepo,
                     MatchUrlRepo;

        ConfigurationOptions pluginSettings;

        // Player database storing BZIDs and callsigns without having to loop through the entire playerlist each time
        std::map<std::string, int> BZID_MAP;
        std::map<std::string, int> CALLSIGN_MAP;

        // The slash commands that are supported and used by this plug-in
        std::vector<std::string> SLASH_COMMANDS;

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