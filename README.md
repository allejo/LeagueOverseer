League Overseer [![Current Release](https://img.shields.io/badge/release-v1.1.1-orange.svg)](https://github.com/allejo/LeagueOverseer/releases/tag/v1.1.1.275) ![Minimum BZFlag Version](https://img.shields.io/badge/BZFlag-v2.4.3+-blue.svg)
================

League Overseer is a BZFlag plug-in that allows league servers to communicate with league websites to track player information, stats, match reports, and handle team data.

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

## Features

- Full compatability with [BZiON](http://github.com/allejo/bzion)
- Automatic match reporting
- Track player stats
- Global ban and mute list
- Match events
- Makes server configuration "brad-proof"

## Compiling

Regardless of the size, complexity, and size of this plugin, the plug-in can still be easily compiled with a simple `make`.

### Requirements

- BZFlag 2.4.3+ (*latest version available on GitHub is recommended*)
- C++11
- JSON Library
    - libjson0-dev (Debian/Ubuntu)
    - json-c-devel (Fedora Linux)

### How to Compile

1. Check out the 2.4.x BZFlag source code from GitHub, if you do not already have it on your server. If you are still using SVN, it is recommended you switch to using Git because all future development of BZFlag will use Git.

        git clone -b v2_4_x https://github.com/BZFlag-Dev/bzflag-import-3.git bzflag

2. Go into the newly checked out source code and then the plugins directory.
        
        cd bzflag/plugins

3. Run a git clone of this repository from within the plugins directory. This should have created a new `LeagueOverseer` directory within the plugins directory. Notice, you will be checking out the 'release' branch will always contain the latest release of the plugin to allow for easy updates. If you are running a test port and would like the latest development build, use the 'master' branch instead of 'release.'

        git clone -b release https://github.com/allejo/LeagueOverseer.git

4. The latest BZFlag trunk will contain a script called 'addToBuild.sh' and it will allow you to add the plugin to the build system.

        sh addToBuild.sh LeagueOverseer

5. Instruct the build system to generate a Makefile and then compile and install the plugin.

        cd ..; ./autogen.sh; ./configure; make; make install;

Documentation
-------------

For full documentation of this plug-in, please visit the dedicated [website](http://allejo.github.io/LeagueOverseer/) which contains an excessive amount of detail of the plug-in's configuration and inner-workings.

License
-------

[GNU General Public License Version 3.0](https://github.com/allejo/LeagueOverseer/blob/master/LICENSE.md)
