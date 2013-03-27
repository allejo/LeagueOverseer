<?php
//List of IPs that are allowed to report matches
$ips = array('127.0.0.1', '108.0.61.94', '97.107.129.174', '130.166.214.110');
if (!in_array($_SERVER['REMOTE_ADDR'], $ips)) die('Error: 403 - Forbidden');

require_once("./CMS/siteoptions.php");
$dbc = new mysqli("localhost", pw_secret::mysqluser_secret(), pw_secret::mysqlpw_secret(), db_used_custom_name());

if ($_GET['query'] == 'reportMatch')
{
    $teamOneWins = $_GET['teamOneWins'];
    $teamOneWins = mysqli_real_escape_string($dbc, $teamOneWins);
    $teamTwoWins = $_GET['teamTwoWins'];
    $teamTwoWins = mysqli_real_escape_string($dbc, $teamTwoWins);
    $timestamp = $_GET['matchTime'];
    $timestamp = mysqli_real_escape_string($dbc, $timestamp);
    $duration = $_GET['duration'];
    $duration = mysqli_real_escape_string($dbc, $duration);
    $teamOnePlayers = $_GET['teamOnePlayers'];
    $teamOnePlayers = mysqli_real_escape_string($dbc, $teamOnePlayers);
    $teamTwoPlayers = $_GET['teamTwoPlayers'];
    $teamTwoPlayers = mysqli_real_escape_string($dbc, $teamTwoPlayers);

    $getTeamOne = "SELECT teamid FROM players WHERE external_id IN (" . $teamOnePlayers . ") LIMIT 1";
    $teamOneResult = @mysqli_query($dbc, $getTeamOne);

    $getTeamTwo = "SELECT teamid FROM players WHERE external_id IN (" . $teamTwoPlayers . ") LIMIT 1";
    $teamTwoResult = @mysqli_query($dbc, $getTeamTwo);

    $teamOneIDs = mysqli_fetch_array($teamOneResult);
    $teamTwoIDs = mysqli_fetch_array($teamTwoResult);

    if (mysqli_num_rows($teamOneResult) == 0 || mysqli_num_rows($teamTwoResult) == 0) //A player that didn't belong to the team ruined the match
    {
        echo "Teams could not be reported properly. Please message a referee with the match data.";
        die();
    }

    //Get a player's team id
    $teamOneID = $teamOneIDs[0];
    $teamTwoID = $teamTwoIDs[0];

    /*
        Go through all the IDs to make sure everyone is on the same
        team. If someone isn't part of the same team, don't report
        the match as it was invalid.
    */
    foreach ($teamOneIDs as $rID)
    {
        if ($rID != $teamOneID)
        {
            echo "There was an invalid team member on the red team. Please report the match to a referee.";
            die();
        }
    }
    foreach ($teamTwoIDs as $pID)
    {
        if ($pID != $teamTwoID)
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

    los_enter_match($teamOneID, $teamTwoID, $teamOneWins, $teamTwoWins, $timestamp, $duration);

    $getTeamOneName = "SELECT `name` FROM `teams` WHERE `id` = " . $teamOneIDs[0] . " LIMIT 1"; //Get the name of the team with the teamid that we got before
    $getTeamOneNameQuery = @mysqli_query($dbc, $getTeamOneName);
    $teamOneName = mysqli_fetch_array($getTeamOneNameQuery);

    $getTeamTwoName = "SELECT `name` FROM `teams` WHERE `id` = " . $teamTwoIDs[0] . " LIMIT 1"; //Get the name of the team with the teamid that we got before
    $getTeamTwoNameQuery = @mysqli_query($dbc, $getTeamTwoName);
    $teamTwoName = mysqli_fetch_array($getTeamTwoNameQuery);

    $getDiff = "SELECT `team1_new_score`, `team2_new_score` FROM `matches` WHERE `timestamp` = \"" . $timestamp . "\" ORDER BY `timestamp` DESC LIMIT 1"; //Get the name of the team with the teamid that we got before
    $getDiffQuery = @mysqli_query($dbc, $getDiff);
    $diffs = mysqli_fetch_array($getDiffQuery);

    echo "(+/- " . abs($diffs[0] - $diffs[1])/2 . ") " . $teamTwoName[0] . "[" . $teamTwoWins . "] vs [" . $teamOneWins . "] " . $teamOneName[0];
}
else if ($_GET['query'] == 'matchTeamQuery')
{
    $teamPlayers = $_GET['teamPlayers'];
    $teamPlayers = mysqli_real_escape_string($dbc, $teamPlayers);

    $getTeam = "SELECT teamid FROM players WHERE external_id IN (" . $teamPlayers . ") LIMIT 1";
    $teamResult = @mysqli_query($dbc, $getTeam);

    $teamIDs = mysqli_fetch_array($teamResult);

    if (mysqli_num_rows($teamResult) == 0) //A player that didn't belong to the team ruined the match
    {
        echo "error";
        die();
    }

    $getTeamName = "SELECT `name` FROM `teams` WHERE `id` = " . $teamIDs[0] . " LIMIT 1"; //Get the name of the team with the teamid that we got before
    $getTeamNameQuery = @mysqli_query($dbc, $getTeamName);
    $teamName = mysqli_fetch_array($getTeamNameQuery);

    echo $teamName[0];
}
else if ($_GET['query'] == 'teamDump')
{
    $getTeams = "SELECT players.external_id, teams.name FROM players, teams WHERE players.teamid = teams.id AND players.external_id != ''";
    $getTeamsQuery = @mysqli_query($dbc, $getTeams);

    while ($entry = mysqli_fetch_array($getTeamsQuery))
    {
        echo "INSERT OR REPLACE INTO players(bzid, team)VALUES(" . $entry[0] . ",'" . $entry[1] . "');";
    }
}
else
{
    $pageURL = 'http';
 if ($_SERVER["HTTPS"] == "on") {$pageURL .= "s";}
 $pageURL .= "://";
 if ($_SERVER["SERVER_PORT"] != "80") {
  $pageURL .= $_SERVER["SERVER_NAME"].":".$_SERVER["SERVER_PORT"].$_SERVER["REQUEST_URI"];
 } else {
  $pageURL .= $_SERVER["SERVER_NAME"].$_SERVER["REQUEST_URI"];
 }
    echo "Error 404 - $pageURL";
}
