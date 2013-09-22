Notice: This branch has been abandoned in favor of a large rewrite for better organization and different features.


-- Match Over Seer --

### Author ###
Vlad Jimenez (allejo)
email: allejo.bzflag@gmail.com
irc:   allejo

### License ###
BSD

### Description ###
This plugin is designed to take over all league match functionality by creating an
/official and a /fm slash command. For all the matches started with the /official
command, the match results will be sent to a PHP script on the league website, which
will take the data and enter the match. The PHP script on the website will obviously
need some form check that the data is being sent from an official league server by
checking the server's IP.

### Slash Commands ###
/official <number of seconds>
/fm <number of seconds>
/teamlist
/cancel

The number seconds following the slash command is the same as /countdown <seconds>
If there is no number of seconds, 10 seconds is the default number of seconds

### How To Use ###
-loadplugin /path/to/matchOverSeer.so,/path/to/matchOverSeer.cfg

### More Info ###
For more information, please visit: https://github.com/allejo/matchOverSeer
