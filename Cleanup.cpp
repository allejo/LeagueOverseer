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
#include "plugin_utils.h"

#include "leagueOverSeer.h"

void leagueOverSeer::Cleanup (void)
{
  bz_debugMessagef(0, "Match Over Seer %i.%i.%i (%i) unloaded.", MAJOR, MINOR, REV, BUILD);
  
  Flush();
  
  bz_removeCustomSlashCommand("official");
  bz_removeCustomSlashCommand("fm");
  bz_removeCustomSlashCommand("teamlist");
  bz_removeCustomSlashCommand("cancel");
  bz_removeCustomSlashCommand("spawn");
}