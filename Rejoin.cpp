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

#include "Rejoin.h"
#include "../../src/bzfs/GameKeeper.h"

typedef struct RejoinEntry {
   char* IPAddress;
   char callsign[CallSignLen];
   TimeKeeper joinTime;
} RejoinEntry;


RejoinDB::RejoinDB ()
{
  queue.clear(); // call me paranoid
}


RejoinDB::~RejoinDB ()
{
  std::list<struct RejoinEntry*>::iterator it;
  for (it = queue.begin(); it != queue.end(); ++it) {
    RejoinEntry *rn = *it;
    delete rn;
  }
  queue.clear();
}

bool RejoinDB::add (int playerIndex)
{
  GameKeeper::Player *playerData
    = GameKeeper::Player::getPlayerByIndex(playerIndex);
  if (playerData == NULL) {
    return false;
  }
  RejoinEntry* rn = new RejoinEntry;
  strncpy (rn->callsign, playerData->player.getCallSign(), CallSignLen - 1);
  rn->callsign[CallSignLen - 1] = '\0';
  strcpy(rn->IPAddress, bz_getPlayerIPAddress(playerData->player.getPlayerIndex()));
  queue.push_back (rn);
  return true;
}

void RejoinDB::del (void) 
{
    std::list<struct RejoinEntry*>::iterator it;
    TimeKeeper thenTime = TimeKeeper::getCurrent();
    thenTime += -600;

    it = queue.begin();
    while (it != queue.end()) {
      RejoinEntry *rn = *it;
      if (rn->joinTime <= thenTime) {
        delete rn;
        it = queue.erase(it);
        continue;
      }
      ++it;
   }

}

bool RejoinDB::inListAlready(const char* ip)
{
  std::list<struct RejoinEntry*>::iterator it;
  it = queue.begin();
  while(it != queue.end()) {
    RejoinEntry *rn = *it;
    if(rn->IPAddress == ip) {
      return true;
      break;
    } 
  } 
  return false;
}

char* RejoinDB::getCallsignByIP(const char* ip) 
{
  std::list<struct RejoinEntry*>::iterator it;
  it = queue.begin();
  char* callsign2;
  while(it != queue.end()) {
    RejoinEntry *rn = *it;
    if(rn->IPAddress == ip) 
    {
     callsign2 = rn->callsign;
    }
  }
  return callsign2;
}
