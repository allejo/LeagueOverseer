<?php
//List of IPs that are allowed to report matches
$ips = array('127.0.0.1');
if (!in_array($_SERVER['REMOTE_ADDR'], $ips)) die('Error: 403 - Forbidden');

//ini_set('display_errors', 'off');

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
    $teamOnePlayers = $_GET['teamTwoPlayers'];
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
    $redTeamNameQuery = @mysqli_query($dbc, $getTeamOneName);
    $redTeamName = mysqli_fetch_array($redTeamNameQuery);

    $getTeamTwoName = "SELECT `name` FROM `teams` WHERE `id` = " . $teamTwoIDs[0] . " LIMIT 1"; //Get the name of the team with the teamid that we got before
    $purpleTeamNameQuery = @mysqli_query($dbc, $getTeamTwoName);
    $purpleTeamName = mysqli_fetch_array($purpleTeamNameQuery);

    $getDiff = "SELECT `team1_new_score`, `team2_new_score` FROM `matches` WHERE `timestamp` = \"" . $timestamp . "\" ORDER BY `timestamp` DESC LIMIT 1"; //Get the name of the team with the teamid that we got before
    $getDiffQuery = @mysqli_query($dbc, $getDiff);
    $diffs = mysqli_fetch_array($getDiffQuery);

    echo "(+/- " . abs($diffs[0] - $diffs[1])/2 . ") " . $purpleTeamName[0] . "[" . $teamTwoWins . "] vs [" . $teamOneWins . "] " . $redTeamName[0];
}