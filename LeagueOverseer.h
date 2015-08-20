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

#include "PluginSettings.h"
#include "Match.h"
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

        ///
        /// Custom functions defined
        ///

        virtual PluginSettings::GameMode getCurrentGameMode();

        virtual bz_BasePlayerRecord  *getPlayerFromCallsignOrID (std::string callsignOrID),
                                     *bz_getPlayerByCallsign (const char* callsign),
                                     *bz_getPlayerByBZID (const char* bzID);

        virtual std::string          getPlayerTeamNameByBZID (std::string bzID),
                                     getPlayerTeamNameByID (int playerID),
                                     buildBZIDString (bz_eTeamType team),
                                     getMatchTime (void);

        virtual bool                 isOfficialMatchInProgress (void),
                                     isFunMatchInProgress (void),
                                     playerAlreadyJoined (std::string bzID),
                                     isMatchInProgress (void),
                                     isOfficialMatch (void),
                                     isLeagueMember (int playerID),
                                     isFunMatch (void);

        // @TODO Rewrite this function...
        virtual void                 /*validateTeamName (bool &invalidate, bool &teamError, MatchParticipant currentPlayer, std::string &teamName, bz_eTeamType team),*/
                                     removePlayerInfo (std::string bzID, std::string callsign),
                                     storePlayerInfo (int playerID, std::string bzID, std::string callsign),
                                     requestTeamName (std::string callsign, std::string bzID),
                                     requestTeamName (bz_eTeamType team),
                                     setLeagueMember (int playerID),
                                     resetTimeLimit (void);

        virtual int                  getTeamIdFromBZID (std::string _bzID),
                                     getMatchProgress (void);


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

        UrlQuery     TeamUrlRepo,
                     MatchUrlRepo;

        PluginSettings pluginSettings;
        Match currentMatch;

        // Player database storing BZIDs and callsigns without having to loop through the entire playerlist each time
        std::map<std::string, int> BZID_MAP;
        std::map<std::string, int> CALLSIGN_MAP;

        // Custom BZDB variables that this plug-in registers and watches
        std::vector<std::string> BZDB_VARS;

        // The slash commands that are supported and used by this plug-in
        std::vector<std::string> SLASH_COMMANDS;

        // The vector that is storing all of the active players
        std::vector<Player> activePlayerList;

        // We will be using a map to handle the team name mottos in the format of
        // <BZID, Team Name>
        typedef std::map<std::string, std::string> TeamNameMottoMap;
        TeamNameMottoMap teamMottos;

        // @TODO Actually get and fill this map
        std::map<std::string, int> TEAM_ID_MAP;
};