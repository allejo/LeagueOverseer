# League Over Seer

This is a BZFlag plug-in that allows official league servers to communicate with the league website to automatically transmit match reports and handle team data.

## Authors

Vlad Jimenez (allejo)  
Ned Anderson (mdskpr)

## Thanks to

* [alezakos][1]
* [apeman][2]
* [blast][3]
* [BulletCatcher][4]
* [JeffM][5]
* ... and all the beta testers

## Compiling
### Requirements

* BZFS r22769 or higher (Bug fixes to API needed by this plugin)
* libsqlite3-dev
* C++11

### How to Compile
1. Check out the BZFlag source code:  
```svn co svn://svn.code.sf.net/p/bzflag/code/trunk/bzflag```
2. Go into the newly checked out source code and then the plugins directory.  
```cd plugins```
3. Create a plugin in the source:  
```sh newplug.sh leagueOverSeer```
4. Delete the newly created leagueOverSeer directory.  
```rm -rf leagueOverSeer```
5. Run a git clone of this repository from within the plugins/ directory. This should have created a new leagueOverSeer directory within the plugins directory.  
```git clone https://github.com/allejo/leagueOverSeer.git```
6. Instruct the build system to generate a Makefile and then compile and install the plugin:  
```./autogen.sh; ./configure; make; make install;```

### Updating the Plugin
1. Go into the leagueOverSeer folder located in your plugins folder.
2. Pull the changes from Git.  
```git pull origin master```
3. Compile the changes.  
```make; make install;```

## BZFS Details
### Slash Commands

    /cancel
    /finish
    /fm
    /official
    /spawn
    /pause
    /resume

### Parameters
    /path/to/leagueOverSeer.so,/path/to/leagueOverSeer.cfg

### Redundant Permissions

With this plugin loaded, there is no need for /countdown or /gameover and it is highly recommended that you remove these permissions from the group permissions file in order to avoid confusion. Any matches started with /countdown or ended with /gameover will not be handled by the plugin and therefore should not be allowed for matches.

### Configuration File Options

ROTATIONAL_LEAGUE

* Default: *false*
* Description:  If a league uses different maps for its matches (such as Open League) then make sure that this value is set to 'true' and if the league is GU or Ducati, then set the value to 'false'

MAPCHANGE_PATH

* Default: *N/A*
* Description: If this is a rotational league, then be sure to change the path to the mapchange.out file because that is where the name of the map file is stored. Whatever is located in this file is what will be submitted as the map played. If this is not a rotational, then this variable can be left with the current value or commented out.

SQLITE_DB

* Default: *N/A*
* Description: This is the SQLite database that will be used to store all the team names according to BZID

LEAGUE_OVER_SEER_URL

* Default: *N/A*
* Description: The URL of the main leagueOverSeer PHP script

DEBUG_LEVEL

* Default: *1*
* Description: The debug level that will be used by the plugin to report some information on who started a match, who canceled a match, what teams played, etc

## Web Hosting Details
If you are hosting a league website using the [bz-owl][6] project, make sure you follow these requirements:

1. Upload leagueOverSeer.php to the root of your league website directory.
2. Add the IPs of the official match servers to the $ips array located on line 26 of leagueOverSeer.php. This is a precaution so only official match servers can report and access data.
3. Set $autoReportID on line 28 to the user ID of the person who will be reporting the matches. For instance, you may want to create an 'autoreport' account on the league website and use that ID.
4. If you would like for the report handler to output match information received to a file, leave $keepLog (line 29) set to *true* and set the file location with the $pathToLogFile variable on line 30. If you do not wish to log, set $keepLog to false and leave $pathToLogFile as is.
5. Since all leagues have different scoring systems, lines 124-137 have GU and Duc points for the ELO fractions. Uncomment and comment which fractions you'd use respectively. By default GU's ```2/3*ELO``` for 20 minute matches and ```1*ELO``` for 30 minute matches is set.

## License
[GNU General Public License 3.0][7]

## Appendix

The code for this plug-in can be found both on [GitHub][8] and [BitBucket][9] in order to satisfy any preferences a person may have and to have a backup in the case one service would have downtime; both services are synced and have the same code. GitHub is the official code repository so please direct all issues and pull requests there; anything submitted to BitBucket will be ignored.

[1]:https://github.com/kongr45gpen
[2]:https://github.com/achoopic
[3]:https://github.com/blast007
[4]:https://github.com/JMakey
[5]:https://github.com/JeffM2501
[6]:https://code.google.com/p/bz-owl/
[7]:https://github.com/allejo/leagueOverSeer/blob/master/LICENSE.markdown
[8]:https://github.com/allejo/leagueOverSeer
[9]:https://bitbucket.org/allejo/leagueoverseer