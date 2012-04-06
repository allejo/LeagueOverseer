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

#include "bzfsAPI.h"
#include "PlayerInfo.h"
#include "../../src/bzfs/GameKeeper.h"
#include "../../src/bzfs/RejoinList.h"

class mottofilter : public bz_Plugin, bz_CustomSlashCommandHandler
{
  virtual const char* Name (){return "Motto Replacer";}
  virtual void Init ( const char* config);
  virtual void Cleanup (void);
  virtual bool SlashCommand ( int , bz_ApiString, bz_ApiString, bz_APIStringList* );
  virtual void Event ( bz_EventData * /* eventData */ );
  const char* getGuTeamFromBzId(std::string bzid);
};

BZ_PLUGIN(mottofilter)

void mottofilter::Init ( const char* /*commandLine*/ )
{
  bz_debugMessage(4,"mottofilter plugin loaded");
  Register(bz_eAllowPlayer);
  Register(bz_ePlayerJoinEvent);
  bz_registerCustomSlashCommand("spawn", this);
  // init events here with Register();
}
const char* mottofilter::getGuTeamFromBzId(std::string bzid)
{
  //get team name from GU website.
  const char* playerTeam = "allejo needs to do this:p";
  return playerTeam;
}

bool mottofilter::SlashCommand ( int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList* cmdParams )
{
  bz_BasePlayerRecord *player = bz_getPlayerByIndex(playerID);
  if (!player)
     return true;

  if(command == "spawn" && player->admin) {

    if(cmdParams->size() == 1) {
      const char* msg = message.c_str() + 6;
      while ((*msg != '\0') && isspace(*msg)) msg++;

      if(isdigit(msg[0])) {
       int grantee = (int) atoi(cmdParams->get(0).c_str());
       bz_grantPerm(grantee, "spawn");
      } else if(!isdigit(msg[0])) {
       int grantee = GameKeeper::Player::getPlayerIDByName(cmdParams->get(0).c_str());
       bz_grantPerm(grantee, "spawn");
    }
   }
  } else if(!player->admin){
     bz_sendTextMessage(BZ_SERVER,playerID,"You do not have permission to use that command.");
     bz_freePlayerRecord(player);
     return true;
  }
 bz_freePlayerRecord(player);
 return true;
}

void mottofilter::Event(bz_EventData * eventData) {
    //substitute motto with team name/abbv.  
    if(eventData->eventType == bz_eAllowPlayer) {
       bz_AllowPlayerEventData_V1 *allowData = (bz_AllowPlayerEventData_V1*)eventData;
       GameKeeper::Player *playerData = GameKeeper::Player::getPlayerByIndex(allowData->playerID);
       bz_BasePlayerRecord *record = bz_getPlayerByIndex(allowData->playerID);

       std::string mottoBefore = playerData->player.getMotto();
       const char* newMotto = getGuTeamFromBzId(playerData->getBzIdentifier());
       bz_debugMessagef(4, "Player Joined: MottoFilter: BzId value == %s", playerData->getBzIdentifier().c_str());

       playerData->player.PlayerInfo::setMotto(newMotto);

       std::string mottoAfter = playerData->player.getMotto();
       bz_debugMessagef(4, "Player Joined: MottoFilter: Replaced Motto %s with %s", mottoBefore.c_str(), mottoAfter.c_str());
       bz_freePlayerRecord(record);
    }
}
void mottofilter::Cleanup(void)
{
  Flush();
  bz_removeCustomSlashCommand("spawn");
}

// Local Variables: ***
// mode:C++ ***
// tab-width: 8 ***i
// c-basic-offset: 2 ***
// indent-tabs-mode: t ***
// End: ***
// ex: shiftwidth=2 tabstop=8

