<?php
$ips = array('78.129.242.95', '78.129.242.11', '207.192.70.176', '97.107.129.174', '85.210.203.221');
if (!in_array($_SERVER['REMOTE_ADDR'], $ips)) die('Error: 403 - Forbidden');

ini_set('display_errors', 'off');

require('.config/cfg.php');

function section_entermatch_calculateRating ($scoreA, $scoreB, $oldA, $oldB, &$newRed, &$newGreen){
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

//$_POST['players'] = 'mdskpr"brad"allejo"kierra"dexter"wayney"red-der"nth"rabbit"-faith-';
if (isset($_POST['players'])) {
	$players = explode('"', $_POST['players']);
	//if (count($players) == 0))
	$where = '';
	foreach ($players AS $p) {
		$where .= "l_player.callsign = '".mysql_real_escape_string($p)."' OR ";
	}
	$where = substr($where, 0, -4);
	$res = mysql_query("SELECT l_player.callsign,l_team.name league_team FROM l_player LEFT JOIN l_team ON l_player.team = l_team.id WHERE $where ORDER BY l_team.name");
	if (mysql_num_rows($res) == 0) {
		echo "Could not get data";
	}
	else {
		$lastTeam = '';
		while ($row = mysql_fetch_object($res)) {
			if (empty($row->league_team)) $row->league_team = 'Teamless';
			if (($lastTeam != $row->league_team) || (empty($lastTeam))) echo "*** $row->league_team ***\n";
			echo "$row->callsign\n";
			$lastTeam = $row->league_team;
		}
	}
}
else if (isset($_POST['redTeamWins'])) {

// remove this
// 0|1|140|2011-12-19 23:56:56|hix|mdskpr"Rexflex"llrr"Your Imagination"ayden"apodemus sylvaticus"D3ad Turtle|tox"123456789"dgdog"brad|hix
die("{$_POST['redTeamWins']}|{$_POST['greenTeamWins']}|{$_POST['matchTime']}|{$_POST['matchDate']}|{$_POST['mapPlayed']}|{$_POST['redPlayers']}|{$_POST['greenPlayers']}|{$_POST['mapPlayed']}");
/*$_POST['redTeamWins'] = 0;
$_POST['greenTeamWins'] = 1;
$_POST['matchTime'] = 140;
$_POST['matchDate'] = '2011-12-19 23:56:56';
$_POST['mapPlayed'] = 'hix';
$_POST['redPlayers'] = 'mdskpr"Rexflex"llrr"Your Imagination"ayden"apodemus sylvaticus"D3ad Turtle';
$_POST['greenPlayers'] = 'tox"123456789"dgdog"brad';*/


$_POST['redTeamWins'] = (int)$_POST['redTeamWins'];
$_POST['greenTeamWins'] = (int)$_POST['greenTeamWins'];
$_POST['matchTime'] = (int)$_POST['matchTime'] / 60;
$_POST['matchDate'] = mysql_real_escape_string($_POST['matchDate']);
$_POST['mapPlayed'] = mysql_real_escape_string($_POST['mapPlayed']);
$_POST['redPlayers'] = mysql_real_escape_string($_POST['redPlayers']);
$_POST['greenPlayers'] = mysql_real_escape_string($_POST['greenPlayers']);


/*$arr = explode("\n", $_POST['data']);
$info = explode(" ", $arr[0]);
$redScore = $info[0];
$greenScore = $info[1];
$map = substr($info[2], 0, -5);
$time = round($info[3]/60);
unset($arr[0]);*/



$_POST['redPlayers'] = explode('"', stripslashes($_POST['redPlayers']));
$_POST['greenPlayers'] = explode('"', stripslashes($_POST['greenPlayers']));

/*foreach ($arr AS $p) {
	if ($p == "") continue;
	$team = substr($p, 0, 1);
	if ($team == 1) $redTeam[] = substr($p, 1);
	else if ($team == 2) $greenTeam[] = substr($p, 1);
}*/

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
//echo '<br /><br />'.$sql.'<br /><br />';

mysql_query("UPDATE l_team SET score = $newRed, active = 'yes' WHERE id = $redTeamID");
mysql_query("UPDATE l_team SET score = $newGreen, active = 'yes' WHERE id = $greenTeamID");

if ($_POST['redTeamWins'] > $_POST['greenTeamWins'])
	echo "Match entered: (+/- $diff) $redTeamName [{$_POST['redTeamWins']}] vs [{$_POST['greenTeamWins']}] $greenTeamName";
else
	echo "Match entered: (+/- $diff) $greenTeamName [{$_POST['greenTeamWins']}] vs [{$_POST['redTeamWins']}] $redTeamName";

//file_put_contents('lol.txt', "\n\n(+/- $diff) $redTeamName [$redScore] vs [$greenScore] $greenTeamName\n{$_POST['redTeamWins']}|{$_POST['greenTeamWins']}|{$_POST['matchTime']}|{$_POST['matchDate']}|{$_POST['mapPlayed']}|{$_POST['redPlayers']}|{$_POST['greenPlayers']}|{$_POST['mapPlayed']}", FILE_APPEND);

file_put_contents('autoreport39103.txt', $log, FILE_APPEND);
}
else {
echo "Error: 404 - Not Found";
}
?>
