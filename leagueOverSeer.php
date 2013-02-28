<?php
//List of IPs that are allowed to report matches
$ips = array('127.0.0.1', '127.0.0.2');
if (!in_array($_SERVER['REMOTE_ADDR'], $ips)) die('Error: 403 - Forbidden');

//ini_set('display_errors', 'off');

if (isset($_GET['league']))
{
    if ($_GET['league'] == "GU" || $_GET['league'] == "DUC" || $_GET['league'] == "OL")
    {
        require_once("./CMS/siteoptions.php");
        $dbc = new mysqli("localhost", pw_secret::mysqluser_secret(), pw_secret::mysqlpw_secret(), db_used_custom_name());

        if ($_GET['query'] == 'reportMatch')
        {
            $redTeamWins = $_GET['redTeamWins'];
            $redTeamWins = mysqli_real_escape_string($dbc, $redTeamWins);
            $purpleTeamWins = $_GET['purpleTeamWins'];
            $purpleTeamWins = mysqli_real_escape_string($dbc, $purpleTeamWins);
            $timestamp = $_GET['matchTime'];
            $timestamp = mysqli_real_escape_string($dbc, $timestamp);
            $duration = $_GET['duration'];
            $duration = mysqli_real_escape_string($dbc, $duration);

            //Let's get all the red players' callsigns
            $redPlayers = explode('"', $_GET['redPlayers']); //Get individual callsigns
            $redPlayerCount = count ($redPlayers); //Get the number of players
            $counter = 0; //We're keeping count

            $getRedTeam = "SELECT `teamid` FROM `players` WHERE `name` IN("; //Start the MySQL query
            foreach ($redPlayers as $rp) //Add all the player's callsigns to the MySQL query
            {
                $counter++;
                $rp = mysqli_real_escape_string($dbc, $rp);
                $getRedTeam .= "'" . $rp . "'";

                if ($counter + 1 === $redPlayerCount) //Only add an comma in there is still another element in the array
                    $getRedTeam .= ", ";
                if ($counter === $redPlayerCount) //Only add a ')' if it's the last item
                    $getRedTeam .= ")";
            }
            $redTeamResult = @mysqli_query($dbc, $getRedTeam);

            //Let's get all the purple players' callsign, do the same things as with the red team
            $purplePlayers = explode('"', $_GET['purplePlayers']);
            $purplePlayerCount = count($purplePlayers);
            $counter = 0;
            $getPurpleTeam = "SELECT `teamid` FROM `players` WHERE `name` IN(";
            foreach ($purplePlayers as $pp)
            {
                $counter++;
                $pp = mysqli_real_escape_string($dbc, $pp);
                $getPurpleTeam .= "'" . $pp . "'";

                if ($counter + 1 === $purplePlayerCount)
                    $getPurpleTeam .= ", ";
                if ($counter === $purplePlayerCount)
                    $getPurpleTeam .= ")";
            }
            $purpleTeamResult = @mysqli_query($dbc, $getPurpleTeam);

            $redTeamIDs = mysqli_fetch_array($redTeamResult);
            $purpleTeamIDs = mysqli_fetch_array($purpleTeamResult);

            if (mysqli_num_rows($redTeamResult) == 0 || mysqli_num_rows($purpleTeamResult) == 0) //A player that didn't belong to the team ruined the match
            {
                echo "Teams could not be reported properly. Please message a referee with the match data.";
                die();
            }

            //Get a player's team id
            $redTeamID = $redTeamIDs[0];
            $purpleTeamID = $purpleTeamIDs[0];

            /*
            Go through all the IDs to make sure everyone is on the same
            team. If someone isn't part of the same team, don't report
            the match as it was invalid.
            */
            foreach ($redTeamIDs as $rID)
            {
                if ($rID != $redTeamID)
                {
                    echo "There was an invalid team member on the red team. Please report the match to a referee.";
                    die();
                }
            }
            foreach ($purpleTeamIDs as $pID)
            {
                if ($pID != $purpleTeamID)
                {
                    echo "There was an invalid team member on the purple team. Please report the match to a referee.";
                    die();
                }
            }

            require_once './CMS/siteinfo.php';
            $site = new siteinfo();
            $connection = $site->connect_to_db();

            require("./Matches/match.php");
            $viewerid = 2156;

            los_enter_match($redTeamID, $purpleTeamID, $redTeamWins, $purpleTeamWins, $timestamp, $duration);

            $getRedTeamName = "SELECT `name` FROM `teams` WHERE `id` = " . $redTeamIDs[0] . " LIMIT 1"; //Get the name of the team with the teamid that we got before
            $redTeamNameQuery = @mysqli_query($dbc, $getRedTeamName);
            $redTeamName = mysqli_fetch_array($redTeamNameQuery);

            $getPurpleTeamName = "SELECT `name` FROM `teams` WHERE `id` = " . $purpleTeamIDs[0] . " LIMIT 1"; //Get the name of the team with the teamid that we got before
            $purpleTeamNameQuery = @mysqli_query($dbc, $getPurpleTeamName);
            $purpleTeamName = mysqli_fetch_array($purpleTeamNameQuery);

            $getDiff = "SELECT `team1_new_score`, `team2_new_score` FROM `matches` WHERE `timestamp` = \"" . $timestamp . "\" ORDER BY `timestamp` DESC LIMIT 1"; //Get the name of the team with the teamid that we got before
            $getDiffQuery = @mysqli_query($dbc, $getDiff);
            $diffs = mysqli_fetch_array($getDiffQuery);

            echo "(+/- " . abs($diffs[0] - $diffs[1])/2 . ") " . $purpleTeamName[0] . "[" . $purpleTeamWins . "] vs [" . $redTeamWins . "] " . $redTeamName[0];
        }
    }
    else
        echo "Error: 404 - Not Found";
}
else
    echo "Error: 404 - Not Found";

//End of file