# League Over Seer

This is a BZFlag plug-in that allows official league servers to communicate with the league website to automatically transmit match reports and handle team data.

## Authors

Vlad Jimenez (allejo)<br>
Ned Anderson (mdskpr)

## Compiling
### Requirements

* libsqlite3-dev

### How to Compile
1. Check out the BZFlag source code.<br>
```svn co svn://svn.code.sf.net/p/bzflag/code/trunk/bzflag ```
2. Create a plugin in the source.<br>
```sh newplug.sh leagueOverSeer```
3. Replace the blank leagueOverSeer.cpp file with the one downloaded
4. Add '-lsqlite3' to LDFLAGS in the Makefile in order to compile linking to the SQLite3 library
5. Add the plug-in to the build system<br>
```./autogen.sh; make; make install;```

## BZFS Details
### Slash Commands
<pre>
/cancel
/fm
/official
/spawn
/pause
/resume
</pre>

### Parameters
<pre>/path/to/leagueOverSeer.so,/path/to/leagueOverSeer.cfg</pre>

### Configuration File Options
```ROTATIONAL_LEAGUE```

* Default: <em>false</em>
* Description:  If a league uses different maps for its matches (such as Open League) then make sure that this value is set to 'true' and if the league is GU or Ducati, then set the value to 'false'

```MAPCHANGE_PATH```

* Default: <em>N/A</em>
* Description: If this is a rotational league, then be sure to change the path to the mapchange.out file because that is where the name of the map file is stored. Whatever is located in this file is what will be submitted as the map played. If this is not a rotational, then this variable can be left with the current value or commented out.

```SQLITE_DB```

* Default: <em>N/A</em>
* Description: This is the SQLite database that will be used to store all the team names according to BZID

```GAMEOVER_REPORT```

* Default: <em>false</em>
* Description: If you would like the plugin to report any match that was ended by a /gameover, then make sure that this value is set to 'true' and if you don't want the match to be reported, be sure to set it to 'false'

```LEAGUE_OVER_SEER_URL```

* Default: <em>N/A</em>
* Description: The URL of the main leagueOverSeer PHP script

```DEBUG_LEVEL```

* Default: <em>1</em>
* Description: The debug level that will be used by the plugin to report some information on who started a match, who canceled a match, what teams played, etc

## Web Hosting Details
If you are hosting a league website using the <a href="https://code.google.com/p/bz-owl/" target="_blank">bz-owl</a> project, make sure you follow these requirements:

1. Make a backup of the /Matches/match.php file and replace it with the one provided in this repository.
2. Upload leagueOverSeer.php to the root of your league website directory.
3. Add the IPs of the official match servers to the $ips array located on line 3 of leagueOverSeer.php. This is a precaution so only official match servers can report and access data.

## License:
GPL 3.0