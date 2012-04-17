<?php
//List of IPs that are allowed to report matches
$ips = array('78.129.242.95', '78.129.242.11', '207.192.70.176', '97.107.129.174', '85.210.203.221');
if (!in_array($_SERVER['REMOTE_ADDR'], $ips)) die('Error: 403 - Forbidden');

ini_set('display_errors', 'off');

if ($_POST['league'] == "OL") require('.config/cfg.php');

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

if (isset($_POST['league']))
{
	if ($_POST['league'] == "DUC")
	{
		switch $_POST['query']
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
	else if ($_POST['league'] == "GU")
	{
		switch $_POST['query']
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
	else if ($_POST['league'] == "OL")
	{
		switch $_POST['query']
		{
			case 'teamNameMotto':
			{
				//TODO: Fix this section so it only looks for one team
				$players = $_POST['player'];
				$where = '';
				foreach ($players AS $p)
				{
					$where .= "l_player.callsign = '".mysql_real_escape_string($p)."' OR ";
				}
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
			{				die("{$_POST['redTeamWins']}|{$_POST['greenTeamWins']}|{$_POST['matchTime']}|{$_POST['matchDate']}|{$_POST['mapPlayed']}|{$_POST['redPlayers']}|{$_POST['greenPlayers']}|{$_POST['mapPlayed']}");

				$_POST['redTeamWins'] = (int)$_POST['redTeamWins'];
				$_POST['greenTeamWins'] = (int)$_POST['greenTeamWins'];
				$_POST['matchTime'] = (int)$_POST['matchTime'] / 60;
				$_POST['matchDate'] = mysql_real_escape_string($_POST['matchDate']);
				$_POST['mapPlayed'] = mysql_real_escape_string($_POST['mapPlayed']);
				$_POST['redPlayers'] = mysql_real_escape_string($_POST['redPlayers']);
				$_POST['greenPlayers'] = mysql_real_escape_string($_POST['greenPlayers']);

				$_POST['redPlayers'] = explode('"', stripslashes($_POST['redPlayers']));
				$_POST['greenPlayers'] = explode('"', stripslashes($_POST['greenPlayers']));

				$res = mysql_query("SELECT team,name,score FROM l_player LEFT JOIN l_team ON l_player.team = l_team.id WHERE callsign = '{$_POST['redPlayers'][0]}'") or die('Error: database error.'.mysql_error());
				$row = mysql_fetch_assoc($res);
				$redTeamID = $row['team'];
				$redOldScore = $row['score'];
				$redTeamName = $row['name'];

				$res = mysql_query("SELECT team,name,score FROM l_player LEFT JOIN l_team ON l_player.team = l_team.id WHERE callsign = '{$_POST['greenPlayers'][0]}'") or die('Error: database error.');
				$row = mysql_fetch_assoc($res);
				$greenTeamID = $row['team'];
				$greenOldScore = $row['score'];
				$greenTeamName = $row['name'];

				if (($redTeamID < 1) || ($greenTeamID < 1) || ($redTeamID == $greenTeamID)) die('Error: teams could not be detected.');

				section_entermatch_calculateRating($redScore, $greenScore, $redOldScore, $greenOldScore, &$newRed, &$newGreen);

				$diff = abs($newGreen - $greenOldScore);

				$now = gmdate("Y-m-d H:i:s");

				$log = "Red: {$_POST['redTeamWins']} - Green: {$_POST['greenTeamWins']} - Map: {$_POST['mapPlayed']} - Time: $now - Red Team: $redTeamID [$redOldScore => $newRed] - Green Team: $greenTeamID [$greenOldScore => $newGreen]\n";

				if ($_POST['redTeamWins'] > $_POST['greenTeamWins'])
					$sql = "INSERT INTO bzl_match (team1,score1,team2,score2,tsactual,identer,tsenter,oldrankt1,oldrankt2,newrankt1,newrankt2,length,map) VALUES($redTeamID,{$_POST['redTeamWins']},$greenTeamID,{$_POST['greenTeamWins']},'$now',337,'$now',$redOldScore,$greenOldScore,$newRed,$newGreen,{$_POST['matchTime']},'{$_POST['mapPlayed']}')";
				else
					$sql = "INSERT INTO bzl_match (team1,score1,team2,score2,tsactual,identer,tsenter,oldrankt1,oldrankt2,newrankt1,newrankt2,length,map) VALUES($greenTeamID,{$_POST['greenTeamWins']},$redTeamID,{$_POST['redTeamWins']},'$now',337,'$now',$greenOldScore,$redOldScore,$newGreen,$newRed,{$_POST['matchTime']},'{$_POST['mapPlayed']}')";

				mysql_query($sql) or die('Error: database error.');

				mysql_query("UPDATE l_team SET score = $newRed, active = 'yes' WHERE id = $redTeamID");
				mysql_query("UPDATE l_team SET score = $newGreen, active = 'yes' WHERE id = $greenTeamID");

				if ($_POST['redTeamWins'] > $_POST['greenTeamWins'])
					echo "Match entered: (+/- $diff) $redTeamName [{$_POST['redTeamWins']}] vs [{$_POST['greenTeamWins']}] $greenTeamName";
				else
					echo "Match entered: (+/- $diff) $greenTeamName [{$_POST['greenTeamWins']}] vs [{$_POST['redTeamWins']}] $redTeamName";

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

