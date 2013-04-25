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

    //The bz-owl user id given to the person who will be entering the matches automatically
    $autoReportID = 0;

    //List of IPs that are allowed to report matches
    $ips = array('127.0.0.1');
    if (!in_array($_SERVER['REMOTE_ADDR'], $ips))
        die('Error: 403 - Forbidden');

    //Create an object to access the database
    require_once 'CMS/siteinfo.php';
    $site = new siteinfo();
    $dbc = $site->connect_to_db();

    if ($_POST['query'] == 'reportMatch') //We'll be reporting a match
    {
        //Clean up user input [I'm using whatever ts is using, sqlSafeString()]
        $teamOneWins = $_POST['teamOneWins'];
        $teamOneWins = sqlSafeString($teamOneWins);
        $teamTwoWins = $_POST['teamTwoWins'];
        $teamTwoWins = sqlSafeString($teamTwoWins);
        $timestamp = $_POST['matchTime'];
        $timestamp = sqlSafeString($timestamp);
        $duration = $_POST['duration'];
        $duration = sqlSafeString($duration);
        $teamOnePlayers = $_POST['teamOnePlayers'];
        $teamOnePlayers = sqlSafeString($teamOnePlayers);
        $teamTwoPlayers = $_POST['teamTwoPlayers'];
        $teamTwoPlayers = sqlSafeString($teamTwoPlayers);

        //Get the team ids judging by the players who participated in the match and store them
        $getTeamOne = ('SELECT teamid FROM players WHERE external_id IN (' . $teamOnePlayers . ') LIMIT 1');
        $teamOneResult = @$site->execute_query('players', $getTeamOne, $dbc);
        $getTeamTwo = "SELECT teamid FROM players WHERE external_id IN (" . $teamTwoPlayers . ") LIMIT 1";
        $teamTwoResult = @$site->execute_query('players', $getTeamTwo, $dbc);
        $teamOneIDs = mysql_fetch_array($teamOneResult);
        $teamTwoIDs = mysql_fetch_array($teamTwoResult);
        $teamOneID = $teamOneIDs[0];
        $teamTwoID = $teamTwoIDs[0];

        //A player that didn't belong to the team ruined the match
        if (mysql_num_rows($teamOneResult) == 0 || mysql_num_rows($teamTwoResult) == 0)
        {
            echo "Teams could not be reported properly. Please message a referee with the match data.";
            die();
        }

        /*
            Go through all the IDs to make sure everyone is on the same
            team. If someone isn't part of the same team, don't report
            the match as it was invalid.
        */
        foreach ($teamOneIDs as $rID)
        {
            if ($rID != $teamOneID)
            {
                echo "There was an invalid team member on either team. Please report the match to a referee or admin.";
                die();
            }
        }
        foreach ($teamTwoIDs as $pID)
        {
            if ($pID != $teamTwoID)
            {
                echo "There was an invalid team member on either team. Please report the match to a referee or admin.";
                die();
            }
        }

        /*
            This code is commented out for the future when ts has finished the matchServices add-on
        */
        /*
            require('CMS/add-ons/pathLoaderSystem/pathLoaderSystem.php');
            $pathLoaderObject = new pathLoaderSystem(true);
            $matchServicesObject = $pathLoaderObject->loadAddon('matchServices', '','');

            if (!$matchServicesObject)
            {
                // you may present message to user or whatever
                return;
            }

            define('matchDataFormat', 1);

            $matchData = array('timestamp' => $timestamp, 'duration' => (int) $duration,'team1ID' => (int) $teamOneID, 'team2ID' => (int) $teamTwoID, 'team1Score' => (int) $teamOneWins, 'team2Score' => (int) $teamTwoWins);
            $matchServicesObject->enterMatch($matchData, matchDataFormat);
        */

        //Check which team won or if it was a tie
        if ($teamOneWins > $teamTwoWins)
        {
            $winningTeamPoints = $teamOneWins;
            $losingTeamPoints = $teamTwoWins;
            $winningTeamPlayers = $teamOnePlayers;
            $losingTeamPlayers = $teamTwoPlayers;
            $pointsForElo = 1;
        }
        else
        {
            $winningTeamPoints = $teamTwoWins;
            $losingTeamPoints = $teamOneWins;
            $winningTeamPlayers = $teamTwoPlayers;
            $losingTeamPlayers = $teamOnePlayers;
            $pointsForElo = 0.5;
        }

        /*
            === League Time Limit Settings ===
            This has been set up for GU league where 30 minutes is the full
            length and 20 minute matches are 2/3 of the ELO. For Duc, you
            would change the settings accordingly.
        */
        if ($duration == 30) $eloFraction = 1;
        else if ($duration == 20) $eloFraction = 2/3;

        //A very very very long batch MySQL queries, I'll explain as I see fit.
        @$site->execute_query("enter_matches", "SET @points := (SELECT " . $pointsForElo . ");");
        @$site->execute_query("enter_matches", "SET @winningTeamPoints := (SELECT " . $winningTeamPoints . ");");
        @$site->execute_query("enter_matches", "SET @losingTeamPoints := (SELECT " . $losingTeamPoints . ");");
        @$site->execute_query("enter_matches", "SET @winningTeamID := (SELECT teamid FROM players WHERE external_id IN (" . $winningTeamPlayers . ") LIMIT 1);"); //Just save the IDs as a MySQL variable
        @$site->execute_query("enter_matches", "SET @losingTeamID := (SELECT teamid FROM players WHERE external_id IN (" . $losingTeamPlayers . ") LIMIT 1);");
        @$site->execute_query("enter_matches", "SET @winningTeamOldScore := (SELECT score FROM teams_overview WHERE teamid = @winningTeamID);"); //Save the old scores
        @$site->execute_query("enter_matches", "SET @losingTeamOldScore := (SELECT score FROM teams_overview WHERE teamid = @losingTeamID);");
        @$site->execute_query("enter_matches", "SET @difference := (SELECT FLOOR(" . $eloFraction . " * 50 * (@points - (SELECT (1 / (1 + POW(10, ((@losingTeamOldScore - @winningTeamOldScore)/400))))))));"); //Calculate the ELO difference
        @$site->execute_query("enter_matches", "SET @winningTeamNewScore := (SELECT (@winningTeamOldScore + @difference));"); //Update the ELO for the winner
        @$site->execute_query("enter_matches", "SET @losingTeamNewScore := (SELECT (@losingTeamOldScore - @difference));"); //Update the ELO for the loser
        @$site->execute_query("enter_matches", "INSERT INTO matches (userid, timestamp, team1ID, team2ID, team1_points, team2_points, team1_new_score, team2_new_score, duration) VALUES (" . $autoReportID . ", \"" . $timestamp . "\", @winningTeamID, @losingTeamID, @winningTeamPoints, @losingTeamPoints, @winningTeamNewScore, @losingTeamNewScore, " . $duration . ");"); //Actually enter the match
        @$site->execute_query("enter_matches", "UPDATE teams_overview SET score = @winningTeamNewScore, num_matches_played = num_matches_played + 1 WHERE teamid = @winningTeamID;"); //Update the number of matches played
        @$site->execute_query("enter_matches", "UPDATE teams_overview SET score = @losingTeamNewScore, num_matches_played = num_matches_played + 1 WHERE teamid = @losingTeamID;");
        @$site->execute_query("enter_matches", "UPDATE teams_profile SET num_matches_draw = num_matches_draw + 1 WHERE teamid = @winningTeamID AND @winningTeamPoints = @losingTeamPoints;");
        @$site->execute_query("enter_matches", "UPDATE teams_profile SET num_matches_draw = num_matches_draw + 1 WHERE teamid = @losingTeamID AND @winningTeamPoints = @losingTeamPoints;");
        @$site->execute_query("enter_matches", "UPDATE teams_profile SET num_matches_won = num_matches_won + 1 WHERE teamid = @winningTeamID AND @winningTeamPoints != @losingTeamPoints;");
        @$site->execute_query("enter_matches", "UPDATE teams_profile SET num_matches_lost = num_matches_lost + 1 WHERE teamid = @losingTeamID AND @winningTeamPoints != @losingTeamPoints;");
        $difference = mysql_fetch_array(@$site->execute_query("enter_matches", "SELECT @difference;"));

        //Get the team names so we can announce them
        $getTeamOneName = "SELECT `name` FROM `teams` WHERE `id` = " . $teamOneIDs[0] . " LIMIT 1";
        $getTeamOneNameQuery = @$site->execute_query('teams', $getTeamOneName, $dbc);
        $teamOneName = mysql_fetch_array($getTeamOneNameQuery);
        $getTeamTwoName = "SELECT `name` FROM `teams` WHERE `id` = " . $teamTwoIDs[0] . " LIMIT 1";
        $getTeamTwoNameQuery = @$site->execute_query('teams', $getTeamTwoName, $dbc);
        $teamTwoName = mysql_fetch_array($getTeamTwoNameQuery);

        //Output the match stats that will be sent back to BZFS
        echo "(+/- " . abs($difference[0]) . ") " . $teamTwoName[0] . "[" . $teamTwoWins . "] vs [" . $teamOneWins . "] " . $teamOneName[0];

        //Have the league site perform maintainence as it sees fit
        ob_start();
        require_once ('CMS/maintenance/index.php');
        ob_end_clean();
    }
    else if ($_POST['query'] == 'teamNameQuery') //We would like to get the team name for a user
    {
        $teamPlayers = $_POST['teamPlayers'];
        $teamPlayers = sqlSafeString($teamPlayers);

        $getTeam = "SELECT teamid FROM players WHERE external_id IN (" . $teamPlayers . ") LIMIT 1";
        $teamResult = @$site->execute_query('players', $getTeam);

        $teamIDs = mysql_fetch_array($teamResult);

        if (mysql_num_rows($teamResult) == 0) //A player that didn't belong to the team ruined the match
        {
            echo "DELETE FROM players WHERE bzid = " . $teamPlayers;
            die();
        }

        $getTeamName = "SELECT `name` FROM `teams` WHERE `id` = " . $teamIDs[0] . " LIMIT 1"; //Get the name of the team with the teamid that we got before
        $getTeamNameQuery = @$site->execute_query('teams', $getTeamName);
        $teamName = mysql_fetch_array($getTeamNameQuery);

        if (mysql_num_rows($getTeamNameQuery) == 0) //A player belongs to a team that doesn't exist
        {
            echo "DELETE FROM players WHERE bzid = " . $teamPlayers;
            die();
        }

        echo "INSERT OR REPLACE INTO players (bzid, team) VALUES (" . $teamPlayers . ", \"" . sqlSafeString($teamName[0]) . "\")";
    }
    else if ($_POST['query'] == 'teamDump') //We are starting a server and need a database dump of all the team names
    {
        $getTeams = "SELECT players.external_id, teams.name FROM players, teams WHERE players.teamid = teams.id AND players.external_id != ''";
        $getTeamsQuery = @$site->execute_query('players, teams', $getTeams);

        while ($entry = mysql_fetch_array($getTeamsQuery)) //For each player, we'll output a SQLite query for BZFS to execute
        {
            echo "INSERT OR REPLACE INTO players(bzid, team) VALUES (" . $entry[0] . ",\"" . sqlSafeString($entry[1]) . "\");";
        }
    }
    else //Oh noes! Someone is trying to h4x0r us!
    {
        echo "Error 404 - Page not found";
    }
