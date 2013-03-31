<?php
    /*
        This file is distributed as part leagueOverSeer
        (http://github.com/allejo/leagueOverSeer)

        Copyright belongs to the authors of bz-owl

        This is a modified match.php file for use with the leagueOverSeer.
        plugin. The only difference is that there is a new function called
        los_enter_match() which was modified slightly in order to support
        the plugin without interferring with the website interface.
    */

	if (!isset($site))
	{
		die('This file is meant to be only included by other files!');
	}

	function enter_match($team_id1, $team_id2, $team1_caps, $team2_caps, $timestamp, $duration)
	{
		global $site;
		global $connection;
		global $tables_locked;
		global $viewerid;

		lock_tables();

		$team1_score = get_score_at_that_time($team_id1, $timestamp);
		$team2_score = get_score_at_that_time($team_id2, $timestamp);


		// create array that keeps track of team score changes
		$team_stats_changes = array();

		// we got the score for both team 1 and team 2 at that point
		// thus we can enter the match at this point
		$diff = 0;
		compute_scores($team_id1, $team_id2,
					   $team1_score, $team2_score,
					   $team1_caps, $team2_caps,
					   $diff, $team_stats_changes, $duration);

		// insert new entry
		$query = ('INSERT INTO `matches` (`userid`, `timestamp`, `team1ID`,'
				  . ' `team2ID`, `team1_points`, `team2_points`, `team1_new_score`, `team2_new_score`, `duration`)'
				  . ' VALUES (' . sqlSafeStringQuotes($viewerid) . ', ' . sqlSafeStringQuotes($timestamp)
				  . ', ' . sqlSafeStringQuotes($team_id1) .', ' . sqlSafeStringQuotes($team_id2)
				  . ', ' . sqlSafeStringQuotes($team1_caps) . ', ' . sqlSafeStringQuotes($team2_caps)
				  . ', ' . sqlSafeStringQuotes($team1_score) . ', ' . sqlSafeStringQuotes($team2_score)
				  . ', ' . sqlSafeStringQuotes($duration) . ')');
		if (!($result = $site->execute_query('matches', $query, $connection)))
		{
			unlock_tables();
			$site->dieAndEndPage('The match reported by user #' . sqlSafeString($viewerid)
								 . ' could not be entered due to a sql problem!');
		}

		// update win/draw/loose count
		require_once('update_match_stats.php');
		update_match_stats_entered($team_id1, $team_id2, $team1_caps, $team2_caps, $site, $connection);


		// trigger score updates for newer matches
		update_later_matches($team_id1, $team_id2,
							 $team1_caps, $team2_caps,
							 $timestamp, $team_stats_changes,
							 $viewerid,  $duration);

		show_score_changes($team_stats_changes, array_keys($team_stats_changes));

		// done with entering that match
		unlock_tables();

		require_once ('../CMS/maintenance/index.php');

		echo '<p>The match was entered successfully.</p>' . "\n";
		echo "</div><div>\n";	// move button out of the static page box
		echo '<a class="button" href="./?enter">Enter another match</a>' . $connection ."\n";

		$site->dieAndEndPage();
	}

	function los_enter_match($team_id1, $team_id2, $team1_caps, $team2_caps, $timestamp, $duration)
	{
		global $site;
		global $connection;
		global $tables_locked;
		global $viewerid;

		lock_tables();

		$team1_score = get_score_at_that_time($team_id1, $timestamp);
		$team2_score = get_score_at_that_time($team_id2, $timestamp);


		// create array that keeps track of team score changes
		$team_stats_changes = array();

		// we got the score for both team 1 and team 2 at that point
		// thus we can enter the match at this point
		$diff = 0;
		compute_scores($team_id1, $team_id2,
					   $team1_score, $team2_score,
					   $team1_caps, $team2_caps,
					   $diff, $team_stats_changes, $duration);

		// insert new entry
		$query = ('INSERT INTO `matches` (`userid`, `timestamp`, `team1ID`,'
				  . ' `team2ID`, `team1_points`, `team2_points`, `team1_new_score`, `team2_new_score`, `duration`)'
				  . ' VALUES (' . sqlSafeStringQuotes($viewerid) . ', ' . sqlSafeStringQuotes($timestamp)
				  . ', ' . sqlSafeStringQuotes($team_id1) .', ' . sqlSafeStringQuotes($team_id2)
				  . ', ' . sqlSafeStringQuotes($team1_caps) . ', ' . sqlSafeStringQuotes($team2_caps)
				  . ', ' . sqlSafeStringQuotes($team1_score) . ', ' . sqlSafeStringQuotes($team2_score)
				  . ', ' . sqlSafeStringQuotes($duration) . ')');
		if (!($result = $site->execute_query('matches', $query, $connection)))
		{
			unlock_tables();
			$site->dieAndEndPage('This match could not be report due to a sql problem! Please contact a referee with match details.');
		}

		// update win/draw/loose count
		require_once('update_match_stats.php');
		update_match_stats_entered($team_id1, $team_id2, $team1_caps, $team2_caps, $site, $connection);


		// trigger score updates for newer matches
		update_later_matches($team_id1, $team_id2,
							 $team1_caps, $team2_caps,
							 $timestamp, $team_stats_changes,
							 $viewerid,  $duration);

		los_show_score_changes($team_stats_changes, array_keys($team_stats_changes));

		// done with entering that match
		unlock_tables();
	}


	function edit_match($team_id1, $team_id2, $team1_caps, $team2_caps, $timestamp, $match_id, $duration)
	{
		global $site;
		global $connection;
		global $tables_locked;
		global $viewerid;

		lock_tables();


		// prepare to update win/draw/loose count

		// who entered/edited the match before?
		$userid_old = 0;
		// when was the match in question
		$timestamp_old = '';

		// old team id's: initialise variables
		$team1_id_old = 0;
		$team2_id_old = 0;
		$team1_points_old = 0;
		$team2_points_old = 0;

		// find out the appropriate team id list for the edited match (to modify total/win/draw/loose count)
		$query = ('SELECT `userid`, `timestamp`, `team1ID`, `team2ID`, `team1_points`, `team2_points`, `duration` FROM `matches`'
				  . ' WHERE `id`=' . sqlSafeStringQuotes($match_id));
		if (!($result = $site->execute_query('matches', $query, $connection)))
		{
			unlock_tables();
			$site->dieAndEndPage('Could not find out id for team 1 given match id '
								 . sqlSafeString($match_id) . ' due to a sql problem!');
		}
		while ($row = mysql_fetch_array($result))
		{
			$userid_old =  intval($row['userid']);
			$timestamp_old = $row['timestamp'];

			$team1_id_old = intval($row['team1ID']); // team id 1 before
			$team2_id_old = intval($row['team2ID']); // team id 2 before
			$team1_points_old = intval($row['team1_points']);
			$team2_points_old = intval($row['team2_points']);
			$duration_old = intval($row['duration']);
		}
		mysql_free_result($result);

		// create array that keeps track of team score changes
		$team_stats_changes = array();

		// mark the old teams as having a potentially changed score
		$team_stats_changes[$team1_id_old] = '';
		$team_stats_changes[$team2_id_old] = '';

		// save old match into edit history table
		$query = ('INSERT INTO `matches_edit_stats` (`match_id`, `userid`, `timestamp`, `team1ID`,'
				  . ' `team2ID`, `team1_points`, `team2_points`, `duration`) VALUES ('
				  . sqlSafeStringQuotes($match_id) . ', ' . sqlSafeStringQuotes($userid_old) . ', '
				  . sqlSafeStringQuotes($timestamp_old) . ', ' . sqlSafeStringQuotes($team1_id_old) .', '
				  . sqlSafeStringQuotes($team2_id_old) . ', ' . sqlSafeStringQuotes($team1_points_old) . ', '
				  . sqlSafeStringQuotes($team2_points_old) . ', ' . sqlSafeStringQuotes($duration) . ')');

		if (!($result = $site->execute_query('matches_edit_stats', $query, $connection)))
		{
			unlock_tables();
			$site->dieAndEndPage('The match reported by user #' . sqlSafeString($viewerid)
								 . ' could not be entered due to a sql problem!');
		}


		// update win/draw/loose count
		require_once('team_match_count.php');

		// if a team did not participate in the newer version, it will be marked inside the following function
		update_team_match_edit($team1_points_old, $team2_points_old,
							   $team1_caps, $team2_caps,
							   $team1_id_old, $team2_id_old,
							   $team_id1, $team_id2, $duration_old);


		// find out oldest timestamp involved
		$oldest_timestamp = $timestamp_old;
		if ((strtotime($timestamp_old) - strtotime($timestamp)) >= 0)
		{
			$oldest_timestamp = $timestamp;
		}

		// old match data variables no longer needed
		unset($userid_old);
		unset($timestamp_old);

		unset($team1_id_old);
		unset($team2_id_old);
		unset($team1_points_old);
		unset($team2_points_old);
		unset($duration_old);


		// find out old score
		$team1_score = get_score_at_that_time($team_id1, $timestamp);
		$team2_score = get_score_at_that_time($team_id2, $timestamp);

		// we got the score for both team 1 and team 2 at that point and did the preparation
		// thus we can edit the match at this point
		$diff = 0;
		compute_scores($team_id1, $team_id2,
					   $team1_score, $team2_score,
					   $team1_caps, $team2_caps,
					   $diff, $team_stats_changes, $duration);

		// update match table (perform the actual editing)
		// use current row id to access the entry
		// only one row needs to be updated
		$query = ('UPDATE `matches` SET `userid`=' . sqlSafeStringQuotes($viewerid)
				  . ',`timestamp`=' . sqlSafeStringQuotes($timestamp)
				  . ',`team1ID`=' . sqlSafeStringQuotes($team_id1)
				  . ',`team2ID`=' . sqlSafeStringQuotes($team_id2)
				  . ',`team1_points`=' . sqlSafeStringQuotes($team1_caps)
				  . ',`team2_points`=' . sqlSafeStringQuotes($team2_caps)
				  . ',`team1_new_score`=' . sqlSafeStringQuotes($team1_score)
				  . ',`team2_new_score`=' . sqlSafeStringQuotes($team2_score)
				  . ',`duration`=' . sqlSafeStringQuotes($duration)
				  . ' WHERE `id`=' . sqlSafeStringQuotes($match_id)
				  . ' LIMIT 1');

		if (!($result = $site->execute_query('matches', $query, $connection)))
		{
			unlock_tables();
			$site->dieAndEndPage('The match reported by user #' . sqlSafeString($viewerid)
								 . ' could not be edited due to a sql problem!');
		}


		// trigger score updates for newer matches
		update_later_matches($team_id1, $team_id2,
							 $team1_caps, $team2_caps,
							 $oldest_timestamp, $team_stats_changes,
							 $viewerid, $duration);

		show_score_changes($team_stats_changes, array_keys($team_stats_changes));

		// done with editing that match
		unlock_tables();

		require_once ('../CMS/maintenance/index.php');

		echo '<p>The match was edited successfully.</p>' . "\n";
		$site->dieAndEndPage();
	}


	function delete_match($match_id)
	{
		global $site;
		global $connection;
		global $tables_locked;
		global $viewerid;

		lock_tables();


		// who entered/edited the match before?
		$userid = 0;

		// find out the appropriate team id list for the edited match (to modify total/win/draw/loose count)
		$query = ('SELECT `userid`, `timestamp`, `team1ID`, `team2ID`,'
				  . ' `team1_points`, `team2_points`, `team1_new_score`, `team2_new_score`, `duration` FROM `matches`'
				  . ' WHERE `id`=' . sqlSafeStringQuotes($match_id));
		if (!($result = $site->execute_query('matches', $query, $connection)))
		{
			unlock_tables();
			$site->dieAndEndPage('Could not find out id for team 1 given match id '
								 . sqlSafeString($match_id) . ' due to a sql problem!');
		}
		while ($row = mysql_fetch_array($result))
		{
			$userid =  intval($row['userid']);
			$timestamp = $row['timestamp'];
			$team_id1 = intval($row['team1ID']);
			$team_id2 = intval($row['team2ID']);
			$team1_caps = intval($row['team1_points']);
			$team2_caps = intval($row['team2_points']);
			$duration = intval($row['duration']);
		}
		mysql_free_result($result);


		// prepare to update win/draw/loose count

		// create array that keeps track of team score changes
		$team_stats_changes = array();

		// mark the participating teams as potentially having a changed score
		$team_stats_changes[$team_id1] = '';
		$team_stats_changes[$team_id2] = '';

		// save old match into edit history table
		$query = ('INSERT INTO `matches_edit_stats` (`match_id`, `userid`, `timestamp`, `team1ID`,'
				  . ' `team2ID`, `team1_points`, `team2_points`, `duration`) VALUES ('
				  . sqlSafeStringQuotes($match_id) . ', ' . sqlSafeStringQuotes($userid) . ', '
				  . sqlSafeStringQuotes($timestamp) . ', ' . sqlSafeStringQuotes($team_id1) .', '
				  . sqlSafeStringQuotes($team_id2) . ', ' . sqlSafeStringQuotes($team1_caps) . ', '
				  . sqlSafeStringQuotes($team2_caps) . ', ' . sqlSafeStringQuotes($duration) . ')');

		if (!($result = $site->execute_query('matches_edit_stats', $query, $connection)))
		{
			unlock_tables();
			$site->dieAndEndPage('The match reported by user #' . sqlSafeString($viewerid)
								 . ' could not be entered due to a sql problem!');
		}


		// we saved the old match in the editing stats thus we can now delete the actual match

		// update match table (perform the actual editing)
		// use current row id to access the entry
		// only one row needs to be updated
		$query = ('DELETE FROM `matches` WHERE `id`=' . sqlSafeStringQuotes($match_id)
				  . ' LIMIT 1');

		if (!($result = $site->execute_query('matches', $query, $connection)))
		{
			unlock_tables();
			$site->dieAndEndPage('The match reported by user #' . sqlSafeString($viewerid) . ' could not be deleted due to a sql problem!');
		}


		// update win/draw/loose count
		require_once('team_match_count.php');

		// both teams didn't play
		decrease_total_match_count($team_id1);
		decrease_total_match_count($team_id2);

		// team 1 won
		if ($team1_caps > $team2_caps)
		{
			// update team 1 data
			decrease_won_match_count($team_id1);

			// update team 2 data
			decrease_lost_match_count($team_id2);
		}

		// team 2 won
		if ($team1_caps < $team2_caps)
		{
			// update team 1 data
			decrease_lost_match_count($team_id1);

			// update team 2 data
			decrease_won_match_count($team_id2);
		}

		// the match ended in a draw
		if ($team1_caps === $team2_caps)
		{
			// update team 1 data
			decrease_draw_match_count($team_id1);

			// update team 2 data
			decrease_draw_match_count($team_id2);
		}

		// old match data variables no longer needed
		unset($userid_old);



		// trigger score updates for newer matches
		update_later_matches($team_id1, $team_id2,
							 $team1_caps, $team2_caps,
							 $timestamp, $team_stats_changes,
							 $viewerid, $duration);

		show_score_changes($team_stats_changes, array_keys($team_stats_changes));



		// done with deleting that match
		unlock_tables();

		require_once ('../CMS/maintenance/index.php');

		echo '<p>The match was deleted successfully.</p>' . "\n";
		$site->dieAndEndPage();
	}


	function update_later_matches($team_id1, $team_id2, $team1_caps, $team2_caps, $timestamp, &$team_stats_changes, $viewerid, $duration)
	{
		global $site;
		global $connection;
		global $tables_locked;


		$viewerid = getUserID();

		// update only newer matches
		$query = ('SELECT `id`, `team1ID`, `team2ID`, `team1_points`, `team2_points`, `timestamp`, `duration` FROM `matches`'
				  . ' WHERE `timestamp`>' . sqlSafeStringQuotes($timestamp));
		if (!($result = $site->execute_query('matches', $query, $connection)))
		{
			$site->dieAndEndPage('Could not find out if later matches than '
								 . sqlSafeStringQuotes($timestamp) . ' were played.');
		}

		while ($row = mysql_fetch_array($result))
		{
			$cur_team1 = intval($row['team1ID']);
			$cur_team2 = intval($row['team2ID']);

			$team1_score = get_score_at_that_time($cur_team1, $row['timestamp']);
			$team2_score = get_score_at_that_time($cur_team2, $row['timestamp']);
			$duration_old = intval($row['duration']);

			// update existing entry
			$diff = 0;
			compute_scores($cur_team1, $cur_team2,
						   $team1_score, $team2_score,
						   $row['team1_points'], $row['team2_points'],
						   $diff, $team_stats_changes, $duration);

			// update score if necessary
			if (!($diff === 0))
			{
				// update current match in loop
				// use current row id to access the entry
				// only one row needs to be updated
				$query = ('UPDATE `matches` SET `team1_new_score`=' . sqlSafeStringQuotes($team1_score)
						  . ',`team2_new_score`=' . sqlSafeStringQuotes($team2_score)
						  . ' WHERE `id`=' . sqlSafeStringQuotes($row['id'])
						  . ' LIMIT 1');
				if (!($result_update = @$site->execute_query('matches', $query, $connection)))
				{
					// query was bad, error message was already given in $site->execute_query(...)
					unlock_tables();
					$site->dieAndEndPage('Updating team scores in nested query update of team scores failed.'
										 . ' One needs to use the backup.');
				}
			}

			// flush timestamp to current timestamp in update schedule
			$timestamp = $row['timestamp'];
		}
		mysql_free_result($result);
	}


	function show_form($team_id1, $team_id2, $team1_points, $team2_points, $readonly, $duration=30)
	{
		global $site;
		global $connection;

		global $match_day;

		// displays match form
		$query = ('SELECT `teams`.`id`,`teams`.`name` FROM `teams`,`teams_overview`'
				  . ' WHERE (`teams_overview`.`deleted`<>' . sqlSafeStringQuotes('2') . ')'
				  . ' AND `teams`.`id`=`teams_overview`.`teamid`'
				  . ' ORDER BY `teams`.`name`');
		if (!($result = @$site->execute_query('teams, teams_overview', $query, $connection)))
		{
			// query was bad, error message was already given in $site->execute_query(...)
			$site->dieAndEndPage('');
		}

		$rows = (int) mysql_num_rows($result);
		// only show a confirmation question, the case is not too unusual and
		if ($rows < 1)
		{
			echo '<p class="first_p">There are no teams in the database. A valid match requires at least 2 teams<p/>';
			$site->dieAndEndPage('');
		}
		if ($rows < 2)
		{
			echo '<p class="first_p">There is only 1 team in the database. A valid match requires at least 2 teams<p/>';
			$site->dieAndEndPage('');
		}

		$team_name_list = Array();
		$team_id_list = Array();
		while($row = mysql_fetch_array($result))
		{
			$team_name_list[] = $row['name'];
			$team_id_list[] = $row['id'];
		}
		mysql_free_result($result);

		$list_team_id_and_name = Array();

		$list_team_id_and_name[] = $team_id_list;
		$list_team_id_and_name[] = $team_name_list;

		echo '<p><label for="match_team_id1">First team: </label>' . "\n";
		echo '<span><select id="match_team_id1" name="match_team_id1';
		if ($readonly)
		{
			echo '" disabled="disabled';
		}
		echo '">' . "\n";

		$n = ((int) count($team_id_list)) - 1;
		for ($i = 0; $i <= $n; $i++)
		{
			echo '<option value="';
			// no strval because team id 0 is reserved
			echo $list_team_id_and_name[0][$i];
			if (isset($team_id1) && ((int) $list_team_id_and_name[0][$i] === ((int) $team_id1)))
			{
				echo '" selected="selected';
			}
			echo '">' . $list_team_id_and_name[1][$i];
			echo '</option>' . "\n";
		}

		echo '</select></span>' . "\n";
		echo '<label for="match_points_team1">Points: </label>' . "\n";
		echo '<span>';
		if ($readonly)
		{
			$site->write_self_closing_tag('input type="text" class="small_input_field" id="match_points_team1"'
										  . ' name="team1_points" value="' . strval(intval($team1_points))
										  . '" readonly="readonly"');
		} else
		{
			$site->write_self_closing_tag('input type="text" class="small_input_field" id="match_points_team1"'
										  . ' name="team1_points" value="' . strval(intval($team1_points)) . '"');
		}
		echo '</span></p>' . "\n\n";

		echo '<p><label for="match_team_id2">Second team: </label>' . "\n";
		echo '<span><select id="match_team_id2" name="match_team_id2';
		if ($readonly)
		{
			echo '" disabled="disabled';
		}
		echo '">' . "\n";

		$n = ((int) count($team_id_list)) - 1;
		for ($i = 0; $i <= $n; $i++)
		{
			echo '<option value="';
			// no strval because team id 0 is reserved
			echo $list_team_id_and_name[0][$i];
			if (isset($team_id2) && ((int) $list_team_id_and_name[0][$i] === ((int) $team_id2)))
			{
				echo '" selected="selected';
			}
			echo '">' . $list_team_id_and_name[1][$i];
			echo '</option>' . "\n";
		}
		echo '</select></span>' . "\n";

		echo '<label for="match_points_team2">Points: </label>' . "\n";
		echo '<span>';

		if ($readonly)
		{
			$site->write_self_closing_tag('input type="text" class="small_input_field" id="match_points_team2"'
										  . ' name="team2_points" value="' . strval(intval($team2_points))
										  . '" readonly="readonly"');
		} else
		{
			$site->write_self_closing_tag('input type="text" class="small_input_field" id="match_points_team2"'
										  . ' name="team2_points" value="' . strval(intval($team2_points)) . '"');
		}
		echo '</span></p>' . "\n\n";

		echo '<p>Current day and time is: ' . date('Y-m-d H:i:s') . ' ' . date('T') . '</p>' . "\n";

		echo '<p><label for="match_day">Day: </label>' . "\n";
		echo '<span>';
		if (!isset($match_day))
		{
			if (isset($_POST['match_day']))
			{
				$match_day = htmlentities($_POST['match_day']);
			} else
			{
				$match_day = date('Y-m-d');
			}
		}
		if ($readonly)
		{
			$site->write_self_closing_tag('input type="text" class="small_input_field" id="match_day"'
										  . ' name="match_day" value="' . htmlent($match_day) . '" readonly="readonly"');
		} else
		{
			$site->write_self_closing_tag('input type="text" class="small_input_field" id="match_day"'
										  . ' name="match_day" value="' . htmlent($match_day) . '"');
		}
		echo '</span></p>' . "\n\n";


		echo '<p><label for="match_time">Time: </label>' . "\n";
		echo '<span>';
		if (!isset($match_time))
		{
			if (isset($_POST['match_time']))
			{
				$match_time = htmlentities($_POST['match_time']);
			} else
			{
				$match_time = date('H:i:s');
			}
		}
		if ($readonly)
		{
			$site->write_self_closing_tag('input type="text" class="small_input_field" id="match_time"'
										  . ' name="match_time" value="' . htmlent($match_time) . '" readonly="readonly"');
		} else
		{
			$site->write_self_closing_tag('input type="text" class="small_input_field" id="match_time"'
										  . ' name="match_time" value="' . htmlent($match_time) . '"');
		}
		echo '</span></p>' . "\n\n";

		echo '<div class="formrow"><label for="match_time">Length: </label>' . "\n";
		echo '<span>';
		if (!isset($duration))
		{
			if (isset($_POST['duration']))
			{
				$duration = intval($_POST['duration']);
			} else
			{
				$duration = 30;
			}
		}
		echo '<select id="duration" name="duration"';
		if ($readonly)
		{
			echo ' disabled="disabled"';
		}
		echo  '>';
		echo '<option value="30" ' . (($duration == 30)? 'selected="selected"' : '') . '>30</option>';
		echo '<option value="20" ' . (($duration == 20)? 'selected="selected"' : '') . '>20</option>';
		echo '</select>';

		echo '</span></div>' . "\n\n";

/*
		echo '</div>';
		echo '</div>';
*/



	}


	function newer_matches_entered()
	{
		global $site;
		global $connection;

		// checked if there are newer matches already entered
		$query = 'SELECT * FROM `matches` WHERE `timestamp`>' . sqlSafeStringQuotes(($_POST['match_day'])  . ' ' . ($_POST['match_time']));
		if (!($result = @$site->execute_query('matches', $query, $connection)))
		{
			unlock_tables();
			$site->dieAndEndPage('Unfortunately there seems to be a database problem and thus comparing timestamps of matches failed.');
		}
	}


	function get_score_at_that_time($teamid, $timestamp)
	{
		global $site;
		global $connection;

		$query = ('SELECT `team1ID`,`team2ID`,`team1_new_score`,`team2_new_score` FROM `matches`'
				  . ' WHERE `timestamp`<'
				  . sqlSafeStringQuotes($timestamp)
				  . ' AND (`team1ID`=' . sqlSafeStringQuotes($teamid)
				  . ' OR `team2ID`=' . sqlSafeStringQuotes($teamid) . ')'
				  . ' ORDER BY `timestamp` DESC LIMIT 0,1');
		if (!($result = $site->execute_query('matches', $query, $connection)))
		{
			$site->dieAndEndPage('Unfortunately there seems to be a database problem'
								 . ' and thus comparing timestamps of matches failed.');
		}

		// write the score of team into variable score
		// default score is 1200
		$score = 1200;
		$rows = mysql_num_rows($result);
		if ($rows > 0)
		{
			while ($row = mysql_fetch_array($result))
			{
				if (((int) $row['team1ID']) === $teamid)
				{
					$score = $row['team1_new_score'];
				} else
				{
					$score = $row['team2_new_score'];
				}
			}
			mysql_free_result($result);
		}

		// return the searched value
		if ($site->debug_sql())
		{
			echo 'get_score_at_that_time for team #' . htmlent($teamid). ' returned ' . $score . '<br>';
		}
		return (int) $score;
	}


	function compute_scores($team_id1, $team_id2, &$score_a, &$score_b, $caps_a, $caps_b, &$diff, &$team_stats_changes, $duration)
	{
		global $site;

		/* A:  Using the old ratings oldA and oldB, the win probability for team A is calculated:
		 prob=1.0 / (1 + 10 ^ ((oldB-oldA)/400.0));
		 score= 1 if A wins, 0.5 for draw, 0 if B wins
		 The change in the ratings is then calculated by:
		 diff=50*(score-prob);
		 After that some rounding magic to integer is applied and the new ratings are calculated:
		 newA=oldA+diff, newB=oldB-diff; */

		if ($site->debug_sql())
		{
			echo 'computed scores before: ' . $score_a . ', ' . $score_b;
			$site->write_self_closing_tag('br');
		}

		if (is_array($team_stats_changes))
		{
			// write down the team id's of the teams in question
			// as the query does not compare the old total and the new total score
			// we can only use this as a mark where to check for changes later
			if (!(isset($team_stats_changes[$team_id1])))
			{
				$team_stats_changes[$team_id1] = '';
			}
			if (!(isset($team_stats_changes[$team_id2])))
			{
				$team_stats_changes[$team_id2] = '';
			}
		}

		if (is_numeric($score_a) && is_numeric($score_b) && is_numeric($duration))
		{
			$score_a = intval($score_a);
			$score_b = intval($score_b);
			$prob = 1.0 / (1 + pow(10, (($score_b-$score_a)/400.0)));
			$score = 0;

			if ($caps_a > $caps_b) // team a wins
			{
				$score = 1;
			} else if ($caps_a == $caps_b) // draw
			{
				$score = 0.5;
			} else // team b wins
			{
				$score = 0;
			}

			// do not compute absolute value of rounded difference
			// as we need a signed integer to track score changes
			$diff=floor(50*($score-$prob));

			$duration = intval($duration);
			if ($duration === 20)
			{
			    $diff = ($diff * 2) / 3;
			}

			// do not forget to round the values to integers
			$score_a = $score_a + $diff;
			$score_b = $score_b - $diff;
		}

		if ($site->debug_sql())
		{
			echo ' values after ' . $score_a . ', ' .  $score_b . ', ' . $caps_a . ', ' . $caps_b . ', ' . $diff;
			$site->write_self_closing_tag('br');
		}
	}


	function show_score_changes($team_stats_changes, $keys, $n_teams=0)
	{
		global $site;
		global $connection;

		if (intval($n_teams) === 0)
		{
			$n_teams=(((int) count($keys)) - 1);
		}


		// old score = new score for some teams?
		$skipped_teams = false;

		for ($i = 0; $i <= $n_teams; $i++)
		{
			// grab old score from team overview
			$query = ('SELECT `score` FROM `teams_overview` WHERE `teamid`='
					  . sqlSafeStringQuotes($keys[$i])
					  . ' LIMIT 1');
			if (!($result = @$site->execute_query('teams_overview', $query, $connection)))
			{
				$site->dieAndEndPageNoBox('Could not load score from team overview for team #'
										  . $keys[$i]
										  . '.');
			}
			while ($row = mysql_fetch_array($result))
			{
				// remember old score
				$team_stats_changes[$keys[$i]]['old_score'] = $row['score'];

				// compute new score
				$team_stats_changes[$keys[$i]]['new_score'] = get_score_at_that_time($keys[$i], date('Y-m-d H:i:s'));

				// save new score in database, if needed
				if ($team_stats_changes[$keys[$i]]['old_score'] - $team_stats_changes[$keys[$i]]['new_score'] === 0)
				{
					// this team has no changed score
					unset($team_stats_changes[$keys[$i]]);

					// -> one team had no changed score
					$skipped_teams = true;
				} else
				{
					$query = ('UPDATE `teams_overview` SET `score`='
							  . $team_stats_changes[$keys[$i]]['new_score']
							  . ' WHERE `teamid`='
							  . sqlSafeStringQuotes($keys[$i])
							  . ' LIMIT 1');
					if (!($result_update = @$site->execute_query('matches', $query, $connection)))
					{
						$site->dieAndEndPageNoBox('Could not load score of team overview for team #'
												  . $keys[$i]
												  . '.');
					}
				}
			}
			mysql_free_result($result);

			if (isset($team_stats_changes[$keys[$i]]))
			{
				// get team name from database
				$query = ('SELECT `name` FROM `teams` WHERE `id`='
						  . sqlSafeStringQuotes($keys[$i])
						  . ' LIMIT 1');
				if (!($result = @$site->execute_query('teams', $query, $connection)))
				{
					$site->dieAndEndPageNoBox('Could not load name of team #'
											  . $keys[$i]
											  . '.');
				}
				while ($row = mysql_fetch_array($result))
				{
					// save name in lookup array
					$team_stats_changes[$keys[$i]]['name'] = $row['name'];
				}
				mysql_free_result($result);
			}
		}


		// at least one team had no changed score
		if ($skipped_teams)
		{
			// re-index the keys
			$keys = array_keys($team_stats_changes);
			// re-compute the number of teams
			$n_teams=(((int) count($keys)) - 1);
		}


		if ($n_teams >= 0)
		{
			// if teams had changed scores show a nice comparison table

			echo '<table id="table_scores_changed_overview" class="nested_table">' . "\n";
			echo '<caption>Changed teams scores</caption>' . "\n";
			echo '<tr>' . "\n";
			echo '	<th>Team</th>' . "\n";
			echo '	<th>Previous score</th>' . "\n";
			echo '	<th>New score</th>' . "\n";
			echo '	<th>Difference</th>' . "\n";
			echo '</tr>' . "\n\n";

			for ($i = 0; $i <= $n_teams; $i++)
			{
				// entries with no changed scores were deleted without re-indexing using unset
				if (isset($keys[$i]))
				{
					echo '<tr class="table_scores_changed_overview">' . "\n";

					// name of team
					echo '	<td class="table_scores_changed_overview_name">';
					echo '<a href="../Teams/?profile=' . htmlspecialchars($keys[$i]) . '">';
					echo strval($team_stats_changes[$keys[$i]]['name']);
					echo '</a>';
					echo '</td>' . "\n";

					// old score
					echo '	<td class="table_scores_changed_overview_score_before">';
					echo strval($team_stats_changes[$keys[$i]]['old_score']);
					echo '</td>' . "\n";

					// new score
					echo '	<td class="table_scores_changed_overview_score_after">';
					echo strval($team_stats_changes[$keys[$i]]['new_score']);
					echo '</td>' . "\n";

					// difference computation
					echo '	<td class="table_scores_changed_overview_difference">';
					$score_change = (strval((intval($team_stats_changes[$keys[$i]]['new_score']))
											-
											(intval($team_stats_changes[$keys[$i]]['old_score']))));

					// prefix
					if ($score_change > 0)
					{
						echo '+';
					} elseif ($score_change === 0)
					{
						// &plusmn; displays a +- symbol
						echo '&plusmn;';
					}

					// display difference
					echo $score_change;
					echo '</td>' . "\n";
					echo '</tr>' . "\n";
				}
			}

			// done
			echo '</table>' . "\n";
		} else
		{
			echo '<p>No team scores were changed.</p>' . "\n";
		}
	}


	function los_show_score_changes($team_stats_changes, $keys, $n_teams=0)
	{
		global $site;
		global $connection;

		if (intval($n_teams) === 0)
		{
			$n_teams=(((int) count($keys)) - 1);
		}


		// old score = new score for some teams?
		$skipped_teams = false;

		for ($i = 0; $i <= $n_teams; $i++)
		{
			// grab old score from team overview
			$query = ('SELECT `score` FROM `teams_overview` WHERE `teamid`='
					  . sqlSafeStringQuotes($keys[$i])
					  . ' LIMIT 1');
			if (!($result = @$site->execute_query('teams_overview', $query, $connection)))
			{
				$site->dieAndEndPageNoBox('Could not load score from team overview for team #'
										  . $keys[$i]
										  . '.');
			}
			while ($row = mysql_fetch_array($result))
			{
				// remember old score
				$team_stats_changes[$keys[$i]]['old_score'] = $row['score'];

				// compute new score
				$team_stats_changes[$keys[$i]]['new_score'] = get_score_at_that_time($keys[$i], date('Y-m-d H:i:s'));

				// save new score in database, if needed
				if ($team_stats_changes[$keys[$i]]['old_score'] - $team_stats_changes[$keys[$i]]['new_score'] === 0)
				{
					// this team has no changed score
					unset($team_stats_changes[$keys[$i]]);

					// -> one team had no changed score
					$skipped_teams = true;
				} else
				{
					$query = ('UPDATE `teams_overview` SET `score`='
							  . $team_stats_changes[$keys[$i]]['new_score']
							  . ' WHERE `teamid`='
							  . sqlSafeStringQuotes($keys[$i])
							  . ' LIMIT 1');
					if (!($result_update = @$site->execute_query('matches', $query, $connection)))
					{
						$site->dieAndEndPageNoBox('Could not load score of team overview for team #'
												  . $keys[$i]
												  . '.');
					}
				}
			}
			mysql_free_result($result);

			if (isset($team_stats_changes[$keys[$i]]))
			{
				// get team name from database
				$query = ('SELECT `name` FROM `teams` WHERE `id`='
						  . sqlSafeStringQuotes($keys[$i])
						  . ' LIMIT 1');
				if (!($result = @$site->execute_query('teams', $query, $connection)))
				{
					$site->dieAndEndPageNoBox('Could not load name of team #'
											  . $keys[$i]
											  . '.');
				}
				while ($row = mysql_fetch_array($result))
				{
					// save name in lookup array
					$team_stats_changes[$keys[$i]]['name'] = $row['name'];
				}
				mysql_free_result($result);
			}
		}


		// at least one team had no changed score
		if ($skipped_teams)
		{
			// re-index the keys
			$keys = array_keys($team_stats_changes);
			// re-compute the number of teams
			$n_teams=(((int) count($keys)) - 1);
		}


		if ($n_teams >= 0)
		{
			if (isset($keys[0]))
			{
				$score_change = (strval((intval($team_stats_changes[$keys[0]]['new_score'])) - (intval($team_stats_changes[$keys[0]]['old_score']))));
				echo '(+/- ' . abs($score_change) . ') ';
			}
		}
		else
		{
			echo '(+/- 0) ';
		}
	}


	function lock_tables()
	{
		global $site;
		global $connection;
		global $tables_locked;

		$tables_locked = true;

		// concurrent access could alter the table while much of the data inside the table is recalculated
		// as most of the data in table depends on each other we must not access it in a concurrent way

		// any call of unlock_tables(...) will unlock the table
		$query = 'LOCK TABLES `matches` WRITE,`teams_overview` WRITE, `teams_profile` WRITE';
		if (isset($_GET['edit']) || isset($_GET['delete']))
		{
			$query .= ', `matches_edit_stats` WRITE';
		}
		$query .= ', `teams` WRITE';

		if (!($result = @$site->execute_query('matches, teams_overview, teams_profile, matches_edit_stats, teams', $query, $connection)))
		{
			unlock_tables();
			$site->dieAndEndPage('Unfortunately locking the matches table failed and thus altering the list of matches was cancelled.');
		}

		// innoDB may neeed autcommit = 0
		$query = 'SET AUTOCOMMIT = 0';
		if (!($result = @$site->execute_query('all!', $query, $connection)))
		{
			unlock_tables();
			$site->dieAndEndPage('Trying to deactivate autocommit failed.');
		}
	}


	function unlock_tables()
	{
		global $site;
		global $connection;

		global $tables_locked;

		if ($tables_locked)
		{
			$query = 'UNLOCK TABLES';
			if (!($site->execute_query('all!', $query, $connection)))
			{
				$site->dieAndEndPage('Unfortunately unlocking tables failed. This likely leads to an access problem to database!');
			}
			$tables_locked = false;
			$query = 'COMMIT';
			if (!($site->execute_query('all!', $query, $connection)))
			{
				$site->dieAndEndPage('Unfortunately committing changes failed!');
			}
			$query = 'SET AUTOCOMMIT = 1';
			if (!($result = @$site->execute_query('all!', $query, $connection)))
			{
				$site->dieAndEndPage('Trying to activate autocommit failed.');
			}
		}
	}

	// all necessary tasks are done
?>
