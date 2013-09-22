__Notice:__ *This branch has been abandoned in favor of using C++11.*

# League Over Seer

This is a BZFlag plug-in that allows official league servers to communicate with the league website to automatically transmit match reports and handle team data.

## Authors

Vlad Jimenez (allejo)<br>
Ned Anderson (mdskpr)

## Thanks to

* <a href="https://github.com/kongr45gpen" target="_blank">alezakos</a>
* <a href="https://github.com/blast007" target="_blank">blast</a>
* BulletCatcher
* <a href="https://github.com/JeffM2501" target="_blank">JeffM</a>
* ... and all the beta testers

## Compiling
### Requirements

* BZFS r22769 or higher (Bug fixes to API needed by this plugin)
* libsqlite3-dev

### How to Compile
1. Check out the BZFlag source code:<br>
```svn co svn://svn.code.sf.net/p/bzflag/code/trunk/bzflag ```
2. Create a plugin in the source:<br>
```sh newplug.sh leagueOverSeer```
3. Delete the newly created leagueOverSeer directory
4. Run a git clone of this repository from within the plugins/ directory. This should have created a new leagueOverSeer directory within the plugins directory.
5. Instruct the build system to generate a Makefile and then compile and install the plugin:<br>
```./autogen.sh; ./configure; make; make install;```

## BZFS Details
### Slash Commands
<pre>
/cancel
/finish
/fm
/official
/spawn
/pause
/resume
</pre>

### Parameters
<pre>/path/to/leagueOverSeer.so,/path/to/leagueOverSeer.cfg</pre>

### Redundant Permissions

With this plugin loaded, there is no need for /countdown or /gameover and it is highly recommended that you remove these permissions from the group permissions file in order to avoid confusion. Any matches started with /countdown or ended with /gameover will not be handled by the plugin and therefore should not be allowed for matches.

### Configuration File Options

ROTATIONAL_LEAGUE

* Default: <em>false</em>
* Description:  If a league uses different maps for its matches (such as Open League) then make sure that this value is set to 'true' and if the league is GU or Ducati, then set the value to 'false'

MAPCHANGE_PATH

* Default: <em>N/A</em>
* Description: If this is a rotational league, then be sure to change the path to the mapchange.out file because that is where the name of the map file is stored. Whatever is located in this file is what will be submitted as the map played. If this is not a rotational, then this variable can be left with the current value or commented out.

SQLITE_DB

* Default: <em>N/A</em>
* Description: This is the SQLite database that will be used to store all the team names according to BZID

LEAGUE_OVER_SEER_URL

* Default: <em>N/A</em>
* Description: The URL of the main leagueOverSeer PHP script

DEBUG_LEVEL

* Default: <em>1</em>
* Description: The debug level that will be used by the plugin to report some information on who started a match, who canceled a match, what teams played, etc

## Web Hosting Details
If you are hosting a league website using the <a href="https://code.google.com/p/bz-owl/" target="_blank">bz-owl</a> project, make sure you follow these requirements:

1. Upload leagueOverSeer.php to the root of your league website directory.
2. Add the IPs of the official match servers to the $ips array located on line 26 of leagueOverSeer.php. This is a precaution so only official match servers can report and access data.
3. Set $autoReportID on line 28 to the user ID of the person who will be reporting the matches. For instance, you may want to create an 'autoreport' account on the league website and use that ID.
4. If you would like for the report handler to output match information received to a file, leave $keepLog (line 29) set to <em>true</em> and set the file location with the $pathToLogFile variable on line 30. If you do not wish to log, set $keepLog to false and leave $pathToLogFile as is.
5. Since all leagues have different scoring systems, lines 124-137 have GU and Duc points for the ELO fractions. Uncomment and comment which fractions you'd use respectively. By default GU's ```2/3*ELO``` for 20 minute matches and ```1*ELO``` for 30 minute matches is set.

## License
<a href="https://github.com/allejo/leagueOverSeer/blob/master/LICENSE.markdown">GNU General Public License 3.0</a>
