League Overseer
================

This is a BZFlag plug-in that allows official league servers to communicate with the league website to automatically transmit match reports and handle team data.

Authors
------

Vlad Jimenez (allejo)  
Ned Anderson (mdskpr)

Thanks to
------

- [alezakos](https://github.com/kongr45gpen)

- [apeman](https://github.com/achoopic)

- [blast](https://github.com/blast007)

- [BulletCatcher](https://github.com/JMakey)

- [JeffM](https://github.com/JeffM2501)

- ... and all the beta testers


Compiling
---------

### Requirements

- BZFlag 2.4.3+ 

- libjson0

- libjson0-dev

- C++11

### How to Compile

1.  Check out the BZFlag source code.

    `git clone -b v2_4_x https://github.com/BZFlag-Dev/bzflag-import-3.git bzflag`

2.  Go into the newly checked out source code and then the plugins directory.

    `cd bzflag/plugins`

3.  Create a plugin using the `newplug.sh` script.

    `sh newplug.sh leagueOverSeer`

4.  Delete the newly create leagueOverSeer directory.

    `rm -rf leagueOverSeer`

5.  Run a git clone of this repository from within the plugins directory. This should have created a new leagueOverSeer directory within the plugins directory.

    `git clone https://github.com/allejo/leagueOverSeer.git`

6.  Instruct the build system to generate a Makefile and then compile and install the plugin.

    `cd ../..; ./autogen.sh; ./configure; make; make install;`

### Updating the Plugin

1.  Go into the leagueOverSeer folder located in your plugins folder.

2.  Pull the changes from Git.

    `git pull origin master`

3.  Compile the changes.

    `make; make install;`

Server Details
--------------

### How to Use

To use this plugin after it has been compiled, simply load the plugin via the configuration file.

`-loadplugin /path/to/leagueOverSeer.so,/path/to/leagueOverSeer.cfg`

### Custom Slash Commands

    /cancel
    /finish
    /fm
    /official
    /pause
    /resume
    /spawn
    
/cancel

- Permission Requirement: Spawn & Non-observer

- Description: Cancel the current match whether it's official or for fun. If this command is used during an official match, it will cancel the match and will *not* report the match to the league website.

/finish

- Permission Requirement: Spawn & Non-observer

- Description: This command will only functional during an official match and the match *will* be reported to the league website.

/fm

- Permission Requirement: Spawn & Non-observer

- Description: This command will start a fun match.

/official

- Permission Requirement: Spawn & Non-observer

- Description: This command will start an official match

/pause

- Permission Requirement: Spawn

- Description: This command is only functional during a match; it will pause the match.

/resume

- Permission Requirement: Spawn

- Description: This command is only functional during a paused match; it will resume the match after it has been paused.

/spawn

- Permission Requirement: Ban

- Description: This command allows you to grant the spawn permission to any players that are on the server

### Deprecated Features

With this plugin loaded, there is no need for /countdown or /gameover and it is highly recommended that you remove these permissions from the group permissions file in order to avoid confusion. Any matches started with /countdown or ended with /gameover will not be handled by the plugin and therefore should not be allowed for matches.

This plugin also replaces the need for the RecordMatch plugin as this plugin already records all matches automatically and names the replays respectively.

### Configuration File Options

ROTATIONAL_LEAGUE

* Default: *false*
* Description:  If a league uses different maps for its matches (such as Open League) then make sure that this value is set to 'true' and if the league is GU or Ducati, then set the value to 'false'

MAPCHANGE_PATH

* Default: *N/A*
* Description: If this is a rotational league, then be sure to change the path to the mapchange.out file because that is where the name of the map file is stored. Whatever is located in this file is what will be submitted as the map played. If this is not a rotational, then this variable can be left with the current value or commented out.

LEAGUE_OVER_SEER_URL

* Default: *N/A*
* Description: The URL of the main leagueOverSeer PHP script

DEBUG_LEVEL

* Default: *1*
* Description: The debug level that will be used by the plugin to report some information on who started a match, who canceled a match, what teams played, etc

Web Hosting Details
------

If you are hosting a league website using the [bz-owl](https://code.google.com/p/bz-owl/) project, make sure you follow these requirements:

1.  Upload leagueOverSeer.php to the root of your league website directory.

2.  Add the IPs of the official match servers to the $ALLOWED_IPS array located on line 27 of leagueOverSeer.php. This is a precaution so only official match servers can report matches and access data.

3.  Set $AUTOREPORT_UID on line 31 to the user ID of the person who will be reporting the matches. For instance, you may want to create an 'autoreport' account on the league website and use that ID.

4.  If you would like for the report handler to output match information received to a file, leave $LOG_DETAILS (line 35) set to *true* and set the file location with the $LOG_FILE variable on line 39. If you do not wish to log, set $LOG_DETAILS to false and leave $LOG_FILE as is.

5.  Since all leagues have different scoring systems, configure line 49 to use the respective fractions of ELOs with their match durations. By default, GU league uses the 2/3 of the ELO points for 20 minute matches and all of the ELO points for 30 minute matches. This looks like: `array(20 => 2/3, 30 => 1)`

License
------

[GNU General Public License 3.0](https://github.com/allejo/leagueOverSeer/blob/master/LICENSE.markdown)

Appendix
------

The code for this plug-in can be found both on [GitHub](https://github.com/allejo/leagueOverSeer) and [BitBucket](https://bitbucket.org/allejo/leagueoverseer) in order to satisfy any preferences a person may have and to have a backup in the case one service would have downtime; both services are synced and have the same code. GitHub is the official code repository so please direct all issues and pull requests there; anything submitted to BitBucket will be ignored.
