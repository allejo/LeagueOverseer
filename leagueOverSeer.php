<?php
    /*
    League Over Seer Plug-in
        Copyright (C) 2013 Vladimir Jimenez & Ned Anderson

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

    /*
        === League Over Seer Settings ===
        Feel free to modify these settings
    */

    // List of IPs that are allowed to report matches
    $ips = array('127.0.0.1');

    $autoReportID = 0;                            // The bz-owl user id given to the person who will be entering the matches automatically
    $keepLog = true;                              // Set it respectively whether or not you want to log match data
    $pathToLogFile = "leagueOverSeer.log";        // The path to the log file



    /*
        === League Over Seer Code ===
        Even though patches are welcome, you're on your
        own for modifications below
    */
    if (!in_array($_SERVER['REMOTE_ADDR'], $ips))
    {
        writeToDebug("Unauthorized access attempt from " . $_SERVER['REMOTE_ADDR']);
        die('Error: 403 - Forbidden');
    }

    //Create an object to access the database
    require_once 'CMS/siteinfo.php';
    $site = new siteinfo();
    $dbc = $site->connect_to_db();

    if ($_POST['query'] == 'reportMatch') //We'll be reporting a match
    {
        writeToDebug("Match data received from " . $_SERVER['REMOTE_ADDR']);
        writeToDebug("--------------------------------------");

        //Clean up user input [I'm using whatever ts is using, sqlSafeString()]
        $teamOneWins    = sqlSafeString($_POST['teamOneWins']);
        $teamTwoWins    = sqlSafeString($_POST['teamTwoWins']);
        $timestamp      = sqlSafeString($_POST['matchTime']);
        $duration       = sqlSafeString($_POST['duration']);
        $teamOnePlayers = sqlSafeString($_POST['teamOnePlayers']);
        $teamTwoPlayers = sqlSafeString($_POST['teamTwoPlayers']);

        //Check which team won
        if ($teamOneWins > $teamTwoWins)
        {
            $winningTeamID      = getTeamID($teamOnePlayers);
            $winningTeamPoints  = $teamOneWins;
            $winningTeamPlayers = $teamOnePlayers;

            $losingTeamID       = getTeamID($teamTwoPlayers);
            $losingTeamPoints   = $teamTwoWins;
            $losingTeamPlayers  = $teamTwoPlayers;
        }
        else //Team two won or it was a draw
        {
            $winningTeamID      = getTeamID($teamTwoPlayers);
            $winningTeamPoints  = $teamTwoWins;
            $winningTeamPlayers = $teamTwoPlayers;

            $losingTeamID       = getTeamID($teamOnePlayers);
            $losingTeamPoints   = $teamOneWins;
            $losingTeamPlayers  = $teamOnePlayers;
        }

        if (($winningTeamID == -1 || $losingTeamID == -1) || $winningTeamID == $losingTeamID)
        {
            if ($winningTeamID == -1)
                writeToDebug("The BZIDs (" . $winningTeamPlayers . ") were not found on the same team. Match invalidated.");
            else
                writeToDebug("The BZIDs (" . $losingTeamPlayers . ") were not found on the same team. Match invalidated.");

            writeToDebug("--------------------------------------");
            writeToDebug("End of Match Report");

            echo "Teams could not be reported properly. Please message a referee with the match data.";
            die();
        }

        //These variables aren't score dependant since the parameter has already been set
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


        /*
            === GU League Time Limit Settings ===
        */
        if ($duration == 30)        $eloFraction = 1;
        else if ($duration == 20)   $eloFraction = 2/3;

        /*
            ===  DUC League Time Limit Settings ===
        */
        /*
        if ($duration == 30)        $eloFraction = 1.5;
        else if ($duration == 20)   $eloFraction = 1;
        else if ($duration == 15)   $eloFraction = 2/3
        */


        if ($winningTeamPoints == $losingTeamPoints)
            $pointsForElo = 0.5;
        else
            $pointsForElo = 1;

        //Do the math to figure out the point difference
        $eloDifference = floor($eloFraction * 50 * ($pointsForElo - (1 / (1 + pow(10, ($losingTeamELO - $winningTeamELO)/400)))));
        writeToDebug("ELO Difference  : +/- " . $eloDifference);

        $winningTeamNewELO = $winningTeamELO + $eloDifference;
        $losingTeamNewELO  = $losingTeamELO - $eloDifference;

        $insertNewMatchQuery = "INSERT INTO matches (userid, timestamp, team1ID, team2ID, team1_points, team2_points, team1_new_score, team2_new_score, duration)" .
                                "VALUES (" .
                                    "'" . $autoReportID       . "', " .
                                    "'" . $timestamp          . "', " .
                                    "'" . $winningTeamID      . "', " .
                                    "'" . $losingTeamID       . "', " .
                                    "'" . $winningTeamPoints  . "', " .
                                    "'" . $losingTeamPoints   . "', " .
                                    "'" . $winningTeamNewELO  . "', " .
                                    "'" . $losingTeamNewELO   . "', " .
                                    "'" . $duration           . "');";

        @$site->execute_query("matches", $insertNewMatchQuery); //Actually enter the match
        @$site->execute_query("teams_overview", "UPDATE teams_overview SET score = " . $winningTeamNewELO . ", num_matches_played = num_matches_played + 1 WHERE teamid = " . $winningTeamID . ";"); //Update the number of matches played
        @$site->execute_query("teams_overview", "UPDATE teams_overview SET score = " . $losingTeamNewELO . ", num_matches_played = num_matches_played + 1 WHERE teamid = " . $losingTeamID . ";");

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

        //Output the match stats that will be sent back to BZFS
        echo "(+/- " . $eloDifference . ") " . $winningTeamName . " [" . $winningTeamPoints . "] vs [" . $losingTeamPoints . "] " . $losingTeamName;

        //Have the league site perform maintainence as it sees fit
        ob_start();
        require_once ('CMS/maintenance/index.php');
        ob_end_clean();
    }
    else if ($_POST['query'] == 'teamNameQuery') //We would like to get the team name for a user
    {
        $player = sqlSafeString($_POST['teamPlayers']);
        $teamID = getTeamID($player);

        if ($teamID < 0) //A player that didn't belong to the team ruined the match
        {
            echo "DELETE FROM players WHERE bzid = " . $player;
            die();
        }

        echo "INSERT OR REPLACE INTO players (bzid, team) VALUES (" . $player . ", \"" . preg_replace("/&[^\s]*;/", "", sqlSafeString(getTeamName($teamID))) . "\")";
    }
    else if ($_POST['query'] == 'teamDump') //We are starting a server and need a database dump of all the team names
    {
        $getTeams = "SELECT players.external_id, teams.name FROM players, teams WHERE players.teamid = teams.id AND players.external_id != ''";
        $getTeamsQuery = @$site->execute_query('players, teams', $getTeams);

        while ($entry = mysql_fetch_array($getTeamsQuery)) //For each player, we'll output a SQLite query for BZFS to execute
        {
            echo "INSERT OR REPLACE INTO players(bzid, team) VALUES (" . $entry[0] . ",\"" . preg_replace("/&[^\s]*;/", "", sqlSafeString($entry[1])) . "\");";
        }
    }
    else //Oh noes! Someone is trying to h4x0r us!
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
            writeToDebug("    (" . $player . ") " . getPlayerCallsign($player));
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

        if (mysql_num_rows($execution) == 0)
            return -1;

        $teamIDs = array();
        while($results = mysql_fetch_array($execution, MYSQL_NUM))
        {
            $teamIDs[] = $results[0];
        }

        if (count(array_unique($teamIDs)) != 1 || $teamIDs[0] == 0)
            return -1;

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
        global $keepLog, $pathToLogFile;

        if ($keepLog === true)
        {
            $file_handler = fopen($pathToLogFile, 'a');
            fwrite($file_handler, date("Y-m-d H:i:s") . " :: " . $string . "\n");
            fclose($file_handler);
        }
    }
