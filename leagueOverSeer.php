<?php
/*
League Over Seer Plug-in
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

///
///  ========================================== Configuration Start ===================================================
///

        // List of IPs that are allowed to report matches, any server whose IP is not in this list will be 403'd in
        // order to prevent abuse of unsanctioned matches. In other words, the IPs of all the official league servers
        // belong in here
        $ALLOWED_IPS = array('127.0.0.1');

        // The bz-owl user id of the person who will be entering the matches automatically. This is NOT their BZID but
        // instead the ID that is stored in the bz-owl database
        $AUTOREPORT_UID = 0;

        // When set to true, all match details and unauthorized access attempts will be reported in the specified log
        // file
        $LOG_DETAILS = true;

        // The path and name of the log file if you have set $LOG_DETAILS to true. If you wish, you can change the name
        // of the file to have the date
        $LOG_FILE = "leagueOverSeer.log";

        // Leagues have different ELO points for official matches, where shorter matches are worth a fraction of the
        // ELO points. The default setup is set to what GU league uses. To set your own lengths and ELO fractions,
        // simply create the array in the format of, 'array(<minutes> => <fraction of ELO>)' where each match length
        // and fraction is separated by commas.
        //
        // Example
        // -------
        //   Duc League: array(15 => .75, 20 => 1, 30 => 1.5)
        $DURATION = array(20 => 2/3, 30 => 1);

///  ========================================= Advanced Debug Options =================================================

        // Unless you know what you are doing, do NOT change any of these variables as they will disable any security
        // checks that are put in place. I'm looking at you brad, don't change these values.
        //
        // Really. Do. Not. Change. These. Values.
        //

        // This option should remain false on any production server. When this option is set to true, it will ignore
        // official servers and accept match reports from any IP at all therefore allowing abuse. This option only
        // exists for debugging the script.
        $DISABLE_IP_CHECK = false;

        // This variable should remain as $_POST on any production server. This option only exists as a method to debug
        // the queries via a browser using $_GET, which allows you to write the URL with the desired parameters. This
        // option may allow for heavy abuse and a lot of errors if changed from the default value.
        $REPORT_METHOD = $_POST;

///
///  =========================================== Configuration End ====================================================
///


// Everything below this point is actual code, so unless you know what you are doing, you shouldn't be changing this;
// yes, I'm looking at you brad. If you know what you're doing, then by all means send a pull request with your changes
// as I'm sure they would benefit other people.


// To prevent abuse of the automated system, we need to make sure that the IP making the request is one of the IPs we
// allowed in the $ALLOWED_IPS array.
if (!$DISABLE_IP_CHECK && !in_array($_SERVER['REMOTE_ADDR'], $ALLOWED_IPS))
{
    // If server making the request isn't an official server, then log the unauthorized attempt and kill the script

    writeToDebug("Unauthorized access attempt from " . $_SERVER['REMOTE_ADDR']);
    die('Error: 403 - Forbidden');
}

// Create an object to access the bz-owl database
require_once 'CMS/siteinfo.php';
$site = new siteinfo();
$dbc = $site->connect_to_db();

// After the first major rewrite of the league overseer plugin, the API was introduced in order to provided backwards
// compatibility for servers that have not updated to the latest version of the plugin.
$API_VERSION = (isset($REPORT_METHOD['apiVersion'])) ? $REPORT_METHOD['apiVersion'] : 0;

// The server would like to report a match
if ($REPORT_METHOD['query'] == 'reportMatch')
{
    writeToDebug("Match data received from " . $_SERVER['REMOTE_ADDR']);
    writeToDebug("--------------------------------------");

    // Clean up user input and store it in variables [I'm using whatever bz-owl is using, sqlSafeString()]
    $teamOneWins    = sqlSafeString($REPORT_METHOD['teamOneWins']);
    $teamTwoWins    = sqlSafeString($REPORT_METHOD['teamTwoWins']);
    $timestamp      = sqlSafeString($REPORT_METHOD['matchTime']);
    $duration       = sqlSafeString($REPORT_METHOD['duration']);
    $teamOnePlayers = sqlSafeString($REPORT_METHOD['teamOnePlayers']);
    $teamTwoPlayers = sqlSafeString($REPORT_METHOD['teamTwoPlayers']);

    // These variables were introduced in API Version 1 so we need to set default values for servers still using the
    // old version of the league overseer plugin.
    $mapPlayed      = (isset($REPORT_METHOD['mapPlayed'])) ? sqlSafeString($REPORT_METHOD['mapPlayed']) : null;
    $server         = ($API_VERSION >= 1) ? $REPORT_METHOD['server'] : null;
    $port           = ($API_VERSION >= 1) ? $REPORT_METHOD['port'] : null;
    $replayFile     = ($API_VERSION >= 1) ? $REPORT_METHOD['replayFile'] : null;

    // This new information was introduced when the API was introduced so at version 1 so we can only handle it if our
    // API version is greater than 1
    if ($API_VERSION >= 1)
    {
        writeToDebug("Server          : " . $server);
        writeToDebug("Port            : " . $port);
        writeToDebug("Replay File     : " . $replayFile);
    }

    // If we're using a rotational league, then we'll have different maps so display what map was used in the match. A
    // rotational league is defined as a league that allows for official matches to be played on different maps.
    if ($mapPlayed != null)
    {
        writeToDebug("Map Played      : " . $mapPlayed);
    }

    // Check which team won
    if ($teamOneWins > $teamTwoWins)
    {
        $winningTeamID      = getTeamID($teamOnePlayers);
        $winningTeamPoints  = $teamOneWins;
        $winningTeamPlayers = $teamOnePlayers;

        $losingTeamID       = getTeamID($teamTwoPlayers);
        $losingTeamPoints   = $teamTwoWins;
        $losingTeamPlayers  = $teamTwoPlayers;
    }
    else // Team two won or it was a draw
    {
        $winningTeamID      = getTeamID($teamTwoPlayers);
        $winningTeamPoints  = $teamTwoWins;
        $winningTeamPlayers = $teamTwoPlayers;

        $losingTeamID       = getTeamID($teamOnePlayers);
        $losingTeamPoints   = $teamOneWins;
        $losingTeamPlayers  = $teamOnePlayers;
    }

    // If we fail to get the the team ID for either the teams or both reported teams are the same team, we cannot
    // report the match due to it being illegal.
    if (($winningTeamID == -1 || $losingTeamID == -1) || $winningTeamID == $losingTeamID)
    {
        // An invalid team could be found in either or both teams, so we need to check both teams and log it the match
        // failure respectively.
        if ($winningTeamID == -1)
        {
            writeToDebug("The BZIDs (" . $winningTeamPlayers . ") were not found on the same team. Match invalidated.");
        }
        if ($losingTeamID == -1)
        {
            writeToDebug("The BZIDs (" . $losingTeamPlayers . ") were not found on the same team. Match invalidated.");
        }
        if ($winningTeamID == $losingTeamID)
        {
            writeToDebug("The '" . getTeamName($winningTeamID) . "' team played against each other in an official match. Match invalidated.");
        }

        writeToDebug("--------------------------------------");
        writeToDebug("End of Match Report");

        if ($winningTeamID == $losingTeamID)
        {
            echo "Holy sanity check, Batman! The same team can't play against each other in an official match.";
        }
        else
        {
            echo "An invalid player was found during the match. Please message a referee to manually report the match.";
        }
        die();
    }

    // These variables aren't score dependant since the parameter has already been set
    $winningTeamName    = getTeamName($winningTeamID);
    $winningTeamELO     = getTeamELO($winningTeamID);
    $losingTeamName     = getTeamName($losingTeamID);
    $losingTeamELO      = getTeamELO($losingTeamID);

    writeToDebug("Match Time      : " . $timestamp);
    writeToDebug("Duration        : " . $duration);
    writeToDebug("");
    writeToDebug("Team '" . $winningTeamName . "'");
    writeToDebug("--------");
    writeToDebug("* Old ELO       : " . $winningTeamELO);
    writeToDebug("* Score         : " . $winningTeamPoints);
    writeToDebug("* Players");
    echoParticipants($winningTeamPlayers);
    writeToDebug("");
    writeToDebug("Team '" . $losingTeamName . "'");
    writeToDebug("--------");
    writeToDebug("* Old ELO       : " . $losingTeamELO);
    writeToDebug("* Score         : " . $losingTeamPoints);
    writeToDebug("* Players");
    echoParticipants($losingTeamPlayers);
    writeToDebug("");

    // Set some variables so we can use them for when we are doing the math for the ELO difference
    $eloFraction = $DURATION[$duration];
    $pointsForElo = ($winningTeamPoints == $losingTeamPoints) ? 0.5 : 1;

    // Do the math to figure out the point difference
    $eloDifference = floor($eloFraction * 50 * ($pointsForElo - (1 / (1 + pow(10, ($losingTeamELO - $winningTeamELO)/400)))));
    writeToDebug("ELO Difference  : +/- " . $eloDifference);

    // Get the new ELO for both teams after the difference has been calculated
    $winningTeamNewELO = $winningTeamELO + $eloDifference;
    $losingTeamNewELO  = $losingTeamELO - $eloDifference;

    // Now form a SQL query to insert the new match into the database because there's no handy API for bz-owl...
    $insertNewMatchQuery = "INSERT INTO matches (userid, timestamp, team1ID, team2ID, team1_points, team2_points, team1_new_score, team2_new_score, duration)" .
                            "VALUES (" .
                                "'" . $AUTOREPORT_UID       . "', " .
                                "'" . $timestamp          . "', " .
                                "'" . $winningTeamID      . "', " .
                                "'" . $losingTeamID       . "', " .
                                "'" . $winningTeamPoints  . "', " .
                                "'" . $losingTeamPoints   . "', " .
                                "'" . $winningTeamNewELO  . "', " .
                                "'" . $losingTeamNewELO   . "', " .
                                "'" . $duration           . "');";

    // Execute the query we just formed so we can actually insert the match into the database
    @$site->execute_query("matches", $insertNewMatchQuery);

    // Now we have to update the number of matches both teams have played
    @$site->execute_query("teams_overview", "UPDATE teams_overview SET score = " . $winningTeamNewELO . ", num_matches_played = num_matches_played + 1 WHERE teamid = " . $winningTeamID . ";");
    @$site->execute_query("teams_overview", "UPDATE teams_overview SET score = " . $losingTeamNewELO . ", num_matches_played = num_matches_played + 1 WHERE teamid = " . $losingTeamID . ";");

    // Now we need to check if the match was a draw in order to update the 'matches draw' count or the 'matches won/lost' count
    if ($winningTeamPoints == $losingTeamPoints)
    {
        @$site->execute_query("teams_profile", "UPDATE teams_profile SET num_matches_draw = num_matches_draw + 1 WHERE teamid = " . $winningTeamID . ";");
        @$site->execute_query("teams_profile", "UPDATE teams_profile SET num_matches_draw = num_matches_draw + 1 WHERE teamid = " . $losingTeamID . ";");
    }
    else
    {
        @$site->execute_query("teams_profile", "UPDATE teams_profile SET num_matches_won = num_matches_won + 1 WHERE teamid = " . $winningTeamID . ";");
        @$site->execute_query("teams_profile", "UPDATE teams_profile SET num_matches_lost = num_matches_lost + 1 WHERE teamid = " . $losingTeamID . ";");
    }

    writeToDebug("--------------------------------------");
    writeToDebug("End of Match Report");

    // Output the match stats that will be sent back to BZFS
    echo "(+/- " . $eloDifference . ") " . $winningTeamName . " [" . $winningTeamPoints . "] vs [" . $losingTeamPoints . "] " . $losingTeamName;

    // Have the league site perform maintainence as it sees fit
    ob_start();
    require_once ('CMS/maintenance/index.php');
    ob_end_clean();
}
else if ($REPORT_METHOD['query'] == 'teamNameQuery') // We would like to get the team name for a user
{
    $player = sqlSafeString($REPORT_METHOD['teamPlayers']);
    $teamID = getTeamID($player);

    // We will only get -1 if a player did not belong to a team, so notify BZFS that they are teamless by sending it a
    // blank team name or a DELETE query respective to the API version.
    if ($teamID < 0)
    {
        if ($API_VERSION == 1)
        {
            echo json_encode(array("bzid" => "$player", "team" => ""));
        }
        else
        {
            echo "DELETE FROM players WHERE bzid = " . $player;
        }

        die();
    }

    // If we have made it this far, then that means the player has a team so notify BZFS of the team name by either
    // sending JSON or a INSERT query
    if ($API_VERSION == 1)
    {
        echo json_encode(array("bzid" => "$player", "team" => preg_replace("/&[^\s]*;/", "", sqlSafeString(getTeamName($teamID)))));
    }
    else
    {
        echo "INSERT OR REPLACE INTO players (bzid, team) VALUES (" . $player . ", \"" . preg_replace("/&[^\s]*;/", "", sqlSafeString(getTeamName($teamID))) . "\")";
    }
}
else if ($REPORT_METHOD['query'] == 'teamDump') // We are starting a server and need a database dump of all the team names
{
    if ($API_VERSION == 1)
    {
        // Create an array to store all teams and the BZIDs
        $teamArray = array();

        // Create a merged table of team names and BZID list
        $getTeams = "SELECT teams.name, GROUP_CONCAT(players.external_id separator ',') AS members FROM players, teams WHERE players.teamid = teams.id AND teams.leader_userid != 0 AND players.external_id != '' GROUP BY teams.name";
        $getTeamsQuery = @$site->execute_query('players, teams', $getTeams);

        // Store the team name and member list in the array we just created
        while ($entry = mysql_fetch_array($getTeamsQuery))
        {
            $teamArray[] = array("team" => preg_replace("/&[^\s]*;/", "", sqlSafeString($entry[0])), "members" => $entry[1]);
        }

        // Return the JSON
        echo json_encode(array("teamDump" => $teamArray));
    }
    else
    {
        // Create a merged table of players' BZID and team names
        $getTeams = "SELECT players.external_id, teams.name FROM players, teams WHERE players.teamid = teams.id AND players.external_id != ''";
        $getTeamsQuery = @$site->execute_query('players, teams', $getTeams);

        // For each player, we'll output a SQLite query for BZFS to execute
        while ($entry = mysql_fetch_array($getTeamsQuery))
        {
            echo "INSERT OR REPLACE INTO players(bzid, team) VALUES (" . $entry[0] . ",\"" . preg_replace("/&[^\s]*;/", "", sqlSafeString($entry[1])) . "\");";
        }
    }
}
else // Oh noes! Someone is trying to h4x0r us!
{
    echo "Error 404 - Page not found";
}


/**
 * Write all the match participants ot the log file
 *
 * @param The players who participated in the match
 */
function echoParticipants($players)
{
    $matchPlayers = explode(",", $players);

    foreach ($matchPlayers as $player)
    {
        writeToDebug("    (" . $player . ") " . getPlayerCallsign($player));
    }
}

/**
 * Get the recorded callsign of a player based on the BZID
 *
 * @param A players BZID
 * @return A player's callsign
 */
function getPlayerCallsign($bzid)
{
    global $site, $dbc;

    $query = "SELECT name FROM players WHERE external_id = " . sqlSafeString($bzid) . " LIMIT 1";
    $execution = @$site->execute_query('players', $query, $dbc);
    $results = mysql_fetch_array($execution);

    return $results[0];
}

/**
 * Queries the database to get the team ELO
 *
 * @param The team ID to be looked up
 * @return The team ELO
 */
function getTeamELO($teamID)
{
    global $site, $dbc;

    $query = "SELECT score FROM teams_overview WHERE teamid = " . $teamID . " LIMIT 1";
    $execution = @$site->execute_query('teams', $query, $dbc);
    $results = mysql_fetch_array($execution);

    return $results[0];
}

/**
 * Queries the database to get the team ID of which players belong to
 *
 * @param The BZIDs of players seperated by commas
 * @return The team ID
 */
function getTeamID($players)
{
    global $site, $dbc;

    $query = "SELECT teamid FROM players WHERE external_id IN (" . $players . ") AND status = 'active'";
    $execution = @$site->execute_query('players', $query, $dbc);

    // The specified BZIDs where not found to be in the same team, so return -1
    if (mysql_num_rows($execution) == 0)
    {
        return -1;
    }

    // We'll be storing all the team IDs in an array
    $teamIDs = array();

    // Loop through all the MySQL results to store the team IDs
    while ($results = mysql_fetch_array($execution, MYSQL_NUM))
    {
        $teamIDs[] = $results[0];
    }

    // Second check to be sure that all players belonged to the same team or else return -1
    if (count(array_unique($teamIDs)) != 1 || $teamIDs[0] == 0)
    {
        return -1;
    }

    return $teamIDs[0];
}

/**
 * Queries the database to get the team name from a given team ID
 *
 * @param The team ID to be looked up
 * @return The team name
 */
function getTeamName($teamID)
{
    global $site, $dbc;

    $query = "SELECT `name` FROM `teams` WHERE `id` = " . $teamID . " LIMIT 1";
    $execution = @$site->execute_query('teams', $query, $dbc);
    $results = mysql_fetch_array($execution);

    return $results[0];
}

/**
 * Writes the specified string to the log file if logging is enabled
 *
 * @param The string that will written
 */
function writeToDebug($string)
{
    global $LOG_DETAILS, $LOG_FILE;

    if ($LOG_DETAILS === true)
    {
        $file_handler = fopen($LOG_FILE, 'a');
        fwrite($file_handler, date("Y-m-d H:i:s") . " :: " . $string . "\n");
        fclose($file_handler);
    }
}
