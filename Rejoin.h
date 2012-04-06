/*
League Over Seer Plug-in
    Copyright (C) 2012 Ned Anderson & Vladimir Jimenez

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "leagueOverSeer.h"
#include "PlayerInfo.h"
#include "TimeKeeper.h"
#include <list>

class RejoinDB : public leagueOverSeer {
  public:
    RejoinDB ();
    ~RejoinDB ();

    bool add (int playerIndex);
    void del (void);
    bool inListAlready( const char* ip);
    char* getCallsignByIP(const char* ip);

  private:
    std::list<struct RejoinEntry*> queue;
};

