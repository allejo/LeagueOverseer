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

## Web Hosting Details
If you are hosting a league website using the <a href="https://code.google.com/p/bz-owl/" target="_blank">bz-owl</a> project, make sure you follow these requirements:

1. Make a backup of the /Matches/match.php file and replace it with the one provided in this repository.
2. Upload leagueOverSeer.php to the root of your league website directory.
3. Add the IPs of the official match servers to the $ips array located on line 3 of leagueOverSeer.php. This is a precaution so only official match servers can report and access data.

## License:
GPL 3.0