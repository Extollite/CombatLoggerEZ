#pragma once

#include <yaml.h>
#include <string>
#include <map>
#include <base/log.h>
#include "base/playerdb.h"
#include <base/scheduler.h>
#include <Actor/Actor.h>
#include <Actor/Player.h>
#include <mods/CommandSupport.h>
#include <mods/ChatAPI.h>
#include <mods/Blacklist.h>

#include <Packet/TransferPacket.h>
#include <Packet/TextPacket.h>

#include <Core/ServerInstance.h>
#include <Net/ServerNetworkHandler.h>
#include <Command/CommandContext.h>
#include <Core/MCRESULT.h>
#include <Command/MinecraftCommands.h>

#include <Core/json.h>
#include <SQLiteCpp/SQLiteCpp.h>
#include <sqlite3.h>

#include <boost/scope_exit.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>


struct Settings {
  int combatTime = 20;
  std::vector<std::string> bannedCommandsVector;
  std::string inCombatMsg = "You are now in combat. You need to wait 20 seconds to log out!";
  std::string inCombatLeft = "You are in combat for %time% more seconds!";
  std::string combatEnd = "You are no longer in combat";
  std::string blockedCommand = "You cannot use this command during combat!";
  template <typename IO> static inline bool io(IO f, Settings &settings, YAML::Node &node) { 
      return f(settings.combatTime, node["combatTime"]) && f(settings.bannedCommandsVector, node["bannedCommands"]) &&
           f(settings.inCombatMsg, node["inCombatMessage"]) && f(settings.inCombatLeft, node["inCombatLeftMessage"]) &&
           f(settings.combatEnd, node["inCombatEndMessage"]) &&
           f(settings.blockedCommand, node["blockedCommandMessage"]);
  }
};

DEF_LOGGER("CL");

struct Combat {
  uint64_t xuid;
  int time;
};

extern Settings settings;

extern std::unordered_map<uint64_t, Combat> inCombat;

