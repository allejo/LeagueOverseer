<?php
//List of IPs that are allowed to report matches
$ips = array('78.129.242.95', '78.129.242.11', '207.192.70.176', '97.107.129.174', '85.210.203.221', '76.90.186.178', '96.242.120.91');
if (!in_array($_SERVER['REMOTE_ADDR'], $ips)) die('Error: 403 - Forbidden');

//ini_set('display_errors', 'off');

function section_entermatch_calculateRating ($scoreA, $scoreB, $oldA, $oldB, &$newRed, &$newGreen)
{
    if ($scoreA > $scoreB)
        $score = 1;
    elseif ($scoreA < $scoreB)
        $score = -1;
    else $score = 0;

    $factor = 50.0;
    $S = 1.0 / (1 + pow(10.0, ($oldB - $oldA) / 400.0));
    $Sc = ($score + 1) / 2.0;
    $diff = abs($factor * ($Sc - $S));
    
    if($diff < 1)
        $diff = 1;
    $d = floor($diff + 0.5);
    
    if($Sc - $S < 0)
        $d = -$d;
    if($Sc - $S == 0)
        $d = 0;

    $newRed = $oldA + $d;
    $newGreen = $oldB - $d;
}

if (isset($_GET['league']))
{
    if ($_GET['league'] == "DUC")
    {
        switch ($_GET['query'])
        {
            case 'teamNameMotto':
            {
            //TODO
            }
            case 'reportMatch':
            {
            //TODO
            }
            default:
            {
            echo "Error: 404 - Not Found";
            }
        }
    }
    else if ($_GET['league'] == "GU")
    {            
        require_once("./CMS/siteoptions.php");
        $dbc = new mysqli("localhost", pw_secret::mysqluser_secret(), pw_secret::mysqlpw_secret(), db_used_custom_name());
        
        switch ($_GET['query'])
        {
            case 'teamNameMotto':
            {
                $player = $_GET['player']; //Get the player name
                $player = mysqli_real_escape_string($dbc, $player); //Clean it up from MySQL injection
                $getTeam = "SELECT `teamid` FROM `players` WHERE external_id = '" . $player . "'"; //Get the teamid of the team the players belongs to
                $result = @mysqli_query($dbc, $getTeam); //Execute the query
                
                if (mysqli_num_rows($result) == 0)
                {
                    echo "Teamless";
                }
                else
                {
                    $playerID = mysqli_fetch_array($result);
                    
                    if ($playerID[0] == 0)
                    {
                        echo "Teamless";
                        die();
                    }
                    
                    $getTeamName = "SELECT `name` FROM `teams` WHERE `id` = " . $playerID[0] . " LIMIT 1"; //Get the name of the team with the teamid that we got before
                    $teamName = @mysqli_query($dbc, $getTeamName);
                    $playerTeamName = mysqli_fetch_array($teamName);
                    
                    echo $playerTeamName[0]; //Print it out
                }
            }
            break;
            
            case 'reportMatch':
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
                
                //ob_start();
                //$tmp = enter_match($redTeamID, $purpleTeamID, $redTeamWins, $purpleTeamWins, $timestamp, $duration);
                //ob_end_clean();
                
                
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
            break;
            
            default:
            {
                echo "Error: 404 - Not Found";
            }
        }
    }
    else if ($_GET['league'] == "OL")
    {
        require('.config/cfg.php');
        
        switch ($_GET['query'])
        {
            case 'teamNameMotto':
            {
                $player = $_GET['player'];
                $where = "l_player.callsign = '".mysql_real_escape_string($player)."'";
                
                $where = substr($where, 0, -4);
                $res = mysql_query("SELECT l_player.callsign,l_team.name league_team FROM l_player LEFT JOIN l_team ON l_player.team = l_team.id WHERE $where ORDER BY l_team.name");
                if (mysql_num_rows($res) == 0)
                {
                    echo "Could not get data";
                }
                else
                {
                    while ($row = mysql_fetch_object($res))
                    {
                        if (empty($row->league_team)) $row->league_team = 'Teamless';
                        if (($lastTeam != $row->league_team) || (empty($lastTeam))) echo "$row->league_team\n";
                    }
                }
            }
            case 'reportMatch':
            {                die("{$_GET['redTeamWins']}|{$_GET['greenTeamWins']}|{$_GET['matchTime']}|{$_GET['matchDate']}|{$_GET['mapPlayed']}|{$_GET['redPlayers']}|{$_GET['greenPlayers']}|{$_GET['mapPlayed']}");
        
                $_GET['redTeamWins'] = (int)$_GET['redTeamWins'];
                $_GET['greenTeamWins'] = (int)$_GET['greenTeamWins'];
                $_GET['matchTime'] = (int)$_GET['matchTime'] / 60;
                $_GET['matchDate'] = mysql_real_escape_string($_GET['matchDate']);
                $_GET['mapPlayed'] = mysql_real_escape_string($_GET['mapPlayed']);
                $_GET['redPlayers'] = mysql_real_escape_string($_GET['redPlayers']);
                $_GET['greenPlayers'] = mysql_real_escape_string($_GET['greenPlayers']);
                
                $_GET['redPlayers'] = explode('"', stripslashes($_GET['redPlayers']));
                $_GET['greenPlayers'] = explode('"', stripslashes($_GET['greenPlayers']));
                
                $res = mysql_query("SELECT team,name,score FROM l_player LEFT JOIN l_team ON l_player.team = l_team.id WHERE callsign = '{$_GET['redPlayers'][0]}'") or die('Error: database error.'.mysql_error());
                $row = mysql_fetch_assoc($res);
                $redTeamID = $row['team'];
                $redOldScore = $row['score'];
                $redTeamName = $row['name'];
                
                $res = mysql_query("SELECT team,name,score FROM l_player LEFT JOIN l_team ON l_player.team = l_team.id WHERE callsign = '{$_GET['greenPlayers'][0]}'") or die('Error: database error.');
                $row = mysql_fetch_assoc($res);
                $greenTeamID = $row['team'];
                $greenOldScore = $row['score'];
                $greenTeamName = $row['name'];
                
                if (($redTeamID < 1) || ($greenTeamID < 1) || ($redTeamID == $greenTeamID)) die('Error: teams could not be detected.');
                
                section_entermatch_calculateRating($redScore, $greenScore, $redOldScore, $greenOldScore, &$newRed, &$newGreen);
                
                $diff = abs($newGreen - $greenOldScore);
                
                $now = gmdate("Y-m-d H:i:s");
                
                $log = "Red: {$_GET['redTeamWins']} - Green: {$_GET['greenTeamWins']} - Map: {$_GET['mapPlayed']} - Time: $now - Red Team: $redTeamID [$redOldScore => $newRed] - Green Team: $greenTeamID [$greenOldScore => $newGreen]\n";
                
                if ($_GET['redTeamWins'] > $_GET['greenTeamWins'])
                    $sql = "INSERT INTO bzl_match (team1,score1,team2,score2,tsactual,identer,tsenter,oldrankt1,oldrankt2,newrankt1,newrankt2,length,map) VALUES($redTeamID,{$_GET['redTeamWins']},$greenTeamID,{$_GET['greenTeamWins']},'$now',337,'$now',$redOldScore,$greenOldScore,$newRed,$newGreen,{$_GET['matchTime']},'{$_GET['mapPlayed']}')";
                else
                    $sql = "INSERT INTO bzl_match (team1,score1,team2,score2,tsactual,identer,tsenter,oldrankt1,oldrankt2,newrankt1,newrankt2,length,map) VALUES($greenTeamID,{$_GET['greenTeamWins']},$redTeamID,{$_GET['redTeamWins']},'$now',337,'$now',$greenOldScore,$redOldScore,$newGreen,$newRed,{$_GET['matchTime']},'{$_GET['mapPlayed']}')";
                
                mysql_query($sql) or die('Error: database error.');
                
                mysql_query("UPDATE l_team SET score = $newRed, active = 'yes' WHERE id = $redTeamID");
                mysql_query("UPDATE l_team SET score = $newGreen, active = 'yes' WHERE id = $greenTeamID");
                
                if ($_GET['redTeamWins'] > $_GET['greenTeamWins'])
                    echo "Match entered: (+/- $diff) $redTeamName [{$_GET['redTeamWins']}] vs [{$_GET['greenTeamWins']}] $greenTeamName";
                else
                    echo "Match entered: (+/- $diff) $greenTeamName [{$_GET['greenTeamWins']}] vs [{$_GET['redTeamWins']}] $redTeamName";
                
                file_put_contents('autoreport39103.txt', $log, FILE_APPEND);
            }
            default:
            {
            echo "Error: 404 - Not Found";
            }
        }
    }
    else
        echo "Error: 404 - Not Found";
}
else
{
    echo "Error: 404 - Not Found";
}
?>

