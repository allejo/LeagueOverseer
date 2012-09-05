<?php
//List of IPs that are allowed to report matches
$ips = array('78.129.242.95', '78.129.242.11', '207.192.70.176', '97.107.129.174', '85.210.203.221', '76.90.186.178');
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
                $getTeam = "SELECT `teamid` FROM `players` WHERE name = '" . $player . "'"; //Get the teamid of the team the players belongs to
                $result = mysqli_query($dbc, $getTeam); //Execute the query
                
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
                    $teamName = mysqli_query($dbc, $getTeamName);
                    $playerTeamName = mysqli_fetch_array($teamName);
                    
                    echo $playerTeamName[0]; //Print it out
                }
            }
            break;
            
            case 'reportMatch':
            {
                require_once(dirname(dirname(__FILE__)) . '/Matches/match.php');
                //enter_match($team_id1, $team_id2, $team1_caps, $team2_caps, $timestamp, $duration)
                
                $redPlayers = explode('"', $_GET['redPlayers']);
                $getRedTeam = "SELECT `teamid` FROM `players` WHERE";
                foreach ($redPlayers as $redPlayer)
                {
                    $redPlayer = mysql_real_escape_string($redPlayer);
                    $getRedTeam .= " `name` = " . $redPlayer;
                }
                
                $redTeamResult = mysql_query($getRedTeam);
                
                $greenPlayers = explode('"', $_GET['redPlayers']);
                $getGreenTeam = "SELECT `teamid` FROM `players` WHERE";
                foreach ($greenPlayers as $greenPlayer)
                {
                    $greenPlayer = mysql_real_escape_string($greenPlayer);
                    $getGreenTeam .= " `name` = " . $greenPlayer;
                }
                
                $greenTeamResult = mysql_query($getGreenTeam);
                
                $redTeamIDs = mysql_fetch_array($redTeamResult);
                $greenTeamIDs = mysql_fetch_array($greenTeamResult);
                
                foreach ($redTeamIDs as $rID)
                {
                
                }
                
                $where = '';
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

