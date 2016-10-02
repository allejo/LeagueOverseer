# League Overseer
[![GitHub release](https://img.shields.io/github/release/allejo/LeagueOverseer.svg?maxAge=2592000)](https://github.com/allejo/LeagueOverseer/releases/latest)
![Minimum BZFlag Version](https://img.shields.io/badge/BZFlag-v2.4.5+-blue.svg)

A BZFlag plug-in to relay information from BZFlag servers to a website, such as player info or match reports. This plug-in is the core of [Leagues United](http://leaguesunited.org/) servers and has full compatability with [BZiON](https://github.com/allejo/bzion).

## Contributors

### Developers

These individuals have made multiple significant contributions to the project on a sustained basis. They become actively involved on improving and adding new features to the project.

- Vladimir Jimenez ([allejo](https://github.com/allejo))
- Ned Anderson ([mdskpr](https://github.com/mdskpr))
- Konstantinos Kanavouras ([kongr45gpen/alezakos](https://github.com/kongr45gpen))

### Thanks to

These individuals have assisted significantly with guiding the project in its current direction and have contributed several suggestions to continuously improve the project.

- [apeman](https://github.com/achoopic)
- [blast](https://github.com/blast007)
- [BulletCatcher](https://github.com/JMakey)
- [JeffM](https://github.com/JeffM2501)
- [the map](https://github.com/asinck)

## Requirements

- BZFlag 2.4.5+ (*latest version available on GitHub is recommended*)
- C++11
- json-c
    - libjson0-dev (Debian/Ubuntu)
    - json-c-devel (Fedora Linux)

## Documentation

### Configuration File

A sample configuration is available in the repository, [leagueOverSeer.cfg](https://github.com/allejo/LeagueOverseer/blob/v1_1/leagueOverSeer.cfg).

| Config Value | Type | Default | Description |
| :----------- | :--: | :-----: | :---------- |
| ROTATIONAL_LEAGUE | Boolean | false | When this option is set to true, the file name of the map that was used for the match will be reported. This is option is relies directly to **this** version of the [mapchange](https://github.com/macsforme/leaguesunited/tree/mapchange) plug-in. |
| MAPCHANGE_PATH | String | None | The path to the file that is created by mapchange which stores the current configuration being used on the server. |
| LEAGUE\_OVERSEER\_URL | String | None | The API endpoint for the plug-in to make motto requests **and** match reports |
| MATCH\_REPORT\_URL | String | None | The API endpoint for the plug-in to report matches to. See the [POST Requests](#post-requests) section to see what data is sent for match requests. <br> **Warning:** If `LEAGUE_OVERSEER_URL` is set, this value will be ignored. |
| MOTTO\_FETCH\_URL | String | None | The API endpoint for the plug-in to fetch player mottos from. See the [POST Requests](#post-requests) section to see what data is sent for match requests. <br> **Warning:** If `LEAGUE_OVERSEER_URL` is set, this value will be ignored. |
| DEBUG_LEVEL | Integer | 1 | The BZFS debug level the plug-in will display relevant debug information at |
| VERBOSE_LEVEL | Integer | 4 | The BZFS debug level the plug-in will display all plug-in information at. <br> **Warning:** This is a lot of information and should not be used in a production environment. |

### POST Requests

League Overseer makes a number of POST requests to its API endpoints. While this plug-in follows BZiON's API specification, you are welcome to write your own endpoints to return custom data or handle matches in a custom website.

#### Match Reports

This POST request is sent to `MATCH_REPORT_URL` or `LEAGUE_OVERSEER_URL` every time a match finishes. League Overseer will output whatever the API endpoint for this request returns as long as it's not an HTML document; in other words, the API endpoint should return plain text.

| POST Variable | Value | Description |
| :------------ | :---: | :---------- |
| query | reportMatch | The type of request the plug-in submitted |
| apiVersion | 1 | The API version plug-in is using. This value is hardcoded in the plug-in and will require you to recompile League Overseer to change this value |
| matchType | "official", "fm" | Whether the match was an official between two teams or a fun match |
| teamOneColor | "Red", "Green", "Blue", "Purple" | The team color of the first team |
| teamTwoColor | "Red", "Green", "Blue", "Purple" | The team color of the second team |
| teamOneWins | `integer` | The number of caps the first team scored |
| teamTwoWins | `integer` | The number of caps the second team secored |
| duration | `integer` | Match time in **minutes**; e.g. a 1800 second match will be reported as 30 |
| matchTime | `YY-MM-DD HH:MM:SS` | The UTC time the match finished |
| server | `host:port` | The public address of the server |
| port | `integer` | The port of the server |
| replayFile | `string` | The name of the replay file for this match |
| mapPlayed | `string` | The name of the map configuration used for the match; this value is gotten from `MAPCHANGE_PATH` file specified in the configuration. Depending on the configuration of the server, this value could be something like `hix`, `duc`, `babel`. |
| teamOnePlayers | `comma separated BZIDs` | A comma separated list of BZIDs for the members on team one; e.g. `180,31980` |
| teamTwoPlayers | `comma separated BZIDs` | A comma separated list of BZIDs for the members of team two; e.g. `180,31980` |
| teamOneIPs | `comma separated IPs` | A comma separated list of IPs for the members of team one; this follows the same order as the list of BZIDs; e.g. `127.0.0.1,127.0.0.2` |
| teamTwoIPs | `comma separated IPs` | A comma separated list of IPs for the members of team one; this follows the same order as the list of BZIDs; e.g. `127.0.0.1,127.0.0.2` |

**Notes**

- The order of the teams being reported respects BZFlag's team color order (red, green, blue, purple). The first team reported is **not** necessarily the winner of the match.

#### Team Motto Dump

This POST request is sent to `MOTTO_FETCH_URL` or `LEAGUE_OVERSEER_URL` whenever the plug-in is initially loaded.

| POST Variable | Value | Description |
| :------------ | :---: | :---------- |
| query | teamDump | The type of request the plug-in submitted |
| apiVersion | 1 | The API version plug-in is using. This value is hardcoded in the plug-in and will require you to recompile League Overseer to change this value |

League Overseer expects a JSON response in the following structure:

```json
{
  "teamDump": [
    {
      "team": "<team name>",
      "members": "<comma separated BZIDs of team members>"
    },
    {
      "team": "<another team name>",
      "members": "<comma separated BZIDs of team members>"
    }
  ]
}
```

#### Player Motto Request

This POST request is sent to `MOTTO_FETCH_URL` or `LEAGUE_OVERSEER_URL` whenever a player joins the server.

| POST Variable | Value | Description |
| :------------ | :---: | :---------- |
| query | teamDump | The type of request the plug-in submitted |
| apiVersion | 1 | The API version plug-in is using. This value is hardcoded in the plug-in and will require you to recompile League Overseer to change this value |
| teamPlayers | `string` | The BZID of the player we're requesting a motto for |

League Overseer expects a JSON response in the following structure:

```json
{
  "bzid": "<BZID of player>",
  "team": "<team name>"
}
```

## License

[GNU General Public License Version 3.0](https://github.com/allejo/leagueOverSeer/blob/master/LICENSE.markdown)
