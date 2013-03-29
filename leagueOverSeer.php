<?php
//List of IPs that are allowed to report matches
$autoReportID = 0;
$ips = array('127.0.0.1', '127.0.0.2');
if (!in_array($_SERVER['REMOTE_ADDR'], $ips)) die('Error: 403 - Forbidden');

require_once("./CMS/siteoptions.php");
$dbc = new mysqli("localhost", pw_secret::mysqluser_secret(), pw_secret::mysqlpw_secret(), db_used_custom_name());

if ($_POST['query'] == 'reportMatch')
{
    $teamOneWins = $_POST['teamOneWins'];
    $teamOneWins = mysqli_real_escape_string($dbc, $teamOneWins);
    $teamTwoWins = $_POST['teamTwoWins'];
    $teamTwoWins = mysqli_real_escape_string($dbc, $teamTwoWins);
    $timestamp = $_POST['matchTime'];
    $timestamp = mysqli_real_escape_string($dbc, $timestamp);
    $duration = $_POST['duration'];
    $duration = mysqli_real_escape_string($dbc, $duration);
    $teamOnePlayers = $_POST['teamOnePlayers'];
    $teamOnePlayers = mysqli_real_escape_string($dbc, $teamOnePlayers);
    $teamTwoPlayers = $_POST['teamTwoPlayers'];
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
    $viewerid = $autoReportID;

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
else if ($_POST['query'] == 'teamNameQuery')
{
    $teamPlayers = $_POST['teamPlayers'];
    $teamPlayers = mysqli_real_escape_string($dbc, $teamPlayers);

    $getTeam = "SELECT teamid FROM players WHERE external_id IN (" . $teamPlayers . ") LIMIT 1";
    $teamResult = @mysqli_query($dbc, $getTeam);

    $teamIDs = mysqli_fetch_array($teamResult);

    if (mysqli_num_rows($teamResult) == 0) //A player that didn't belong to the team ruined the match
    {
        echo "Teamless";
        die();
    }

    $getTeamName = "SELECT `name` FROM `teams` WHERE `id` = " . $teamIDs[0] . " LIMIT 1"; //Get the name of the team with the teamid that we got before
    $getTeamNameQuery = @mysqli_query($dbc, $getTeamName);
    $teamName = mysqli_fetch_array($getTeamNameQuery);

    if (mysqli_num_rows($getTeamNameQuery) == 0) //A player belongs to a team that doesn't exist
    {
        echo "Teamless";
        die();
    }

    echo $teamName[0];
}
else if ($_POST['query'] == 'teamDump')
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
    echo "Error 404 - Page not found";
}
