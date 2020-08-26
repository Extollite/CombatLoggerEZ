#include "global.h"

#include <dllentry.h>

Settings settings;
std::unordered_map<uint64_t, Combat> inCombat;
std::set<std::string> bannedCommands;
bool running                = false;
bool first   = true;
Mod::Scheduler::Token token;
DEFAULT_SETTINGS(settings);

Mod::Scheduler::Token getToken() { return token; }
std::unordered_map<uint64_t, Combat>& getInCombat() {
  return inCombat;
}

void dllenter() { /*Mod::CommandSupport::GetInstance().AddListener(SIG("loaded"), initCommand);*/
}
void dllexit() {}

void PreInit() {
  Mod::PlayerDatabase::GetInstance().AddListener(SIG("initialized"), [](Mod::PlayerEntry const &entry) {});
  Mod::PlayerDatabase::GetInstance().AddListener(SIG("left"), [](Mod::PlayerEntry const &entry) {
    auto &db = Mod::PlayerDatabase::GetInstance();
    if (getInCombat().count(entry.xuid)) {
      auto packet    = TextPacket::createTextPacket<TextPacketType::JukeboxPopup>(settings.combatEnd);
      uint64_t xuid = getInCombat()[entry.xuid].xuid;
      getInCombat().erase(entry.xuid);
      Inventory *invSource =
          CallServerClassMethod<PlayerInventory *>("?getSupplies@Player@@QEAAAEAVPlayerInventory@@XZ", entry.player)
              ->invectory.get();
      CallServerClassMethod<void>("?dropAll@Inventory@@UEAAX_N@Z", invSource, false);
      CallServerClassMethod<void>("?dropEquipment@Player@@UEAAXXZ", entry.player);
      //int exp = CallServerClassMethod<int>("?getOnDeathExperience@Actor@@UEAAHXZ", entry.player);
      //std::cout << exp << std::endl;
      //CallServerFunction<void>(
      //    "?spawnOrbs@ExperienceOrb@@SAXAEAVBlockSource@@AEBVVec3@@HW4DropType@1@PEAVPlayer@@@Z",
      //    direct_access<BlockSource *>(entry.player, 0x320), entry.player->getPos(), exp, 3, nullptr);
      //direct_access<char *>(entry.player, 0x328)[1072] = 1;
      std::string annouce = boost::replace_all_copy(settings.logout, "%name%", entry.name);
      auto packetAnnouce = TextPacket::createTextPacket<TextPacketType::SystemMessage>(annouce);
      LocateService<Level>()->forEachPlayer([&](Player const &p) -> bool {
        p.sendNetworkPacket(packetAnnouce);
        return true;
      });
      entry.player->kill();
      if (getInCombat().count(xuid)) {
        if (getInCombat()[xuid].xuid == entry.xuid) {
          auto entry = db.Find(xuid);
          if (entry) { 
              entry->player->sendNetworkPacket(packet); 
          }
          getInCombat().erase(xuid);
        }
      }
      if (getInCombat().empty() && running) {
        Mod::Scheduler::SetTimeOut(
            Mod::Scheduler::GameTick(1), [=](auto) { Mod::Scheduler::ClearInterval(getToken()); });
        running = false;
      }
    }
  });
}
void PostInit() {
  for (std::string &str : settings.bannedCommandsVector) { 
      bannedCommands.emplace(str);
  }
  settings.bannedCommandsVector.clear();
}

class ActorDamageSource {
  char filler[0x10];

public:
  __declspec(dllimport) virtual void destruct1(unsigned int)     = 0;
  __declspec(dllimport) virtual bool isEntitySource() const      = 0;
  __declspec(dllimport) virtual bool isChildEntitySource() const = 0;

private:
  __declspec(dllimport) virtual void *unk0() = 0;
  __declspec(dllimport) virtual void *unk1() = 0;
  __declspec(dllimport) virtual void *unk2() = 0;
  __declspec(dllimport) virtual void *unk3() = 0;
  __declspec(dllimport) virtual void *unk4() = 0;

public:
  __declspec(dllimport) virtual ActorUniqueID getEntityUniqueID() const = 0;
  __declspec(dllimport) virtual int getEntityType() const               = 0;

private:
  __declspec(dllimport) virtual int getEntityCategories() const = 0;
};

THook(void *, "?_hurt@Mob@@MEAA_NAEBVActorDamageSource@@H_N1@Z", Mob &mob, ActorDamageSource *src, int i1, bool b1, bool b2) {
  if (mob.getEntityTypeId() != ActorType::Player) return original(mob, src, i1, b1, b2);
  auto &db = Mod::PlayerDatabase::GetInstance();
  auto it  = db.Find((Player *) &mob);
  if (!it) return original(mob, src, i1, b1, b2);
  if (it->player->getCommandPermissionLevel() > CommandPermissionLevel::Any) return original(mob, src, i1, b1, b2);
  if (src->isChildEntitySource() || src->isEntitySource()) {
    Actor *ac = LocateService<Level>()->fetchEntity(src->getEntityUniqueID(), false);
    if (ac && ac->getEntityTypeId() == ActorType::Player) {
      auto &db   = Mod::PlayerDatabase::GetInstance();
      auto entry = db.Find((Player *) ac);
      if (!entry) return original(mob, src, i1, b1, b2);
      if (entry->player->getCommandPermissionLevel() > CommandPermissionLevel::Any)
        return original(mob, src, i1, b1, b2);
      auto packet = TextPacket::createTextPacket<TextPacketType::JukeboxPopup>(settings.inCombatMsg);
      if (!getInCombat().count(entry->xuid)) { 
          entry->player->sendNetworkPacket(packet);
      }
      getInCombat()[entry->xuid].xuid = it->xuid;
      getInCombat()[entry->xuid].time = settings.combatTime;
      if (!getInCombat().count(it->xuid)) { 
          it->player->sendNetworkPacket(packet);
      }
      getInCombat()[it->xuid].xuid = entry->xuid;
      getInCombat()[it->xuid].time = settings.combatTime;
      if (!running) {
        running = true;
        token = Mod::Scheduler::SetInterval(Mod::Scheduler::GameTick(20), [=](auto) {
          if (running) {
            auto &db = Mod::PlayerDatabase::GetInstance();
            for (auto it = getInCombat().begin(); it != getInCombat().end();) {
              auto player = db.Find(it->first);
              if (!player) {
                it->second.time--;
                continue;
              }
              if (--it->second.time > 0) {
                std::string annouce =
                    boost::replace_all_copy(settings.inCombatLeft, "%time%", std::to_string(it->second.time));
                auto packet = TextPacket::createTextPacket<TextPacketType::JukeboxPopup>(annouce);
                player->player->sendNetworkPacket(packet);
                ++it;
              } else {
                auto packet = TextPacket::createTextPacket<TextPacketType::JukeboxPopup>(settings.combatEnd);
                player->player->sendNetworkPacket(packet);
                it = getInCombat().erase(it);
              }
            }
            if (getInCombat().empty() && running) {
              Mod::Scheduler::SetTimeOut(Mod::Scheduler::GameTick(1), [=](auto) { Mod::Scheduler::ClearInterval(getToken()); });
              running = false;
            }
          }
        });
      }
    }
  }
  return original(mob, src, i1, b1, b2);
}

THook(void *, "?die@Player@@UEAAXAEBVActorDamageSource@@@Z", Player &thi, void *src) {
  auto &db = Mod::PlayerDatabase::GetInstance();
  auto it  = db.Find((Player *) &thi);
  if (!it) return original(thi, src);
  if (it->player->getCommandPermissionLevel() > CommandPermissionLevel::Any) return original(thi, src);
  if (getInCombat().count(it->xuid)) {
    auto packet = TextPacket::createTextPacket<TextPacketType::JukeboxPopup>(settings.combatEnd);
    Combat &combat = getInCombat()[it->xuid];
    getInCombat().erase(it->xuid);
    it->player->sendNetworkPacket(packet);
    if (getInCombat().count(combat.xuid)) {
      if (getInCombat()[combat.xuid].xuid == it->xuid) { 
          auto entry = db.Find(combat.xuid);
          if (entry) { 
            entry->player->sendNetworkPacket(packet);
          }
          getInCombat().erase(combat.xuid); 
      }
    }
    if (getInCombat().empty() && running) {
      Mod::Scheduler::SetTimeOut(Mod::Scheduler::GameTick(1), [=](auto) { Mod::Scheduler::ClearInterval(getToken()); });
      running = false;
    }
    ItemStack is;
    is.set(10);

  }
  return original(thi, src);
}

THook(
    void *, "?handle@ServerNetworkHandler@@UEAAXAEBVNetworkIdentifier@@AEBVCommandRequestPacket@@@Z",
    ServerNetworkHandler &snh, NetworkIdentifier const &netid, void *pk) {
  auto &db = Mod::PlayerDatabase::GetInstance();
  auto it  = db.Find(netid);
  if (!it || it->player->getCommandPermissionLevel() > CommandPermissionLevel::Any) { return original(snh, netid, pk); }
  std::string command(direct_access<std::string>(pk, 0x28));
  command = command.substr(1);
  std::vector<std::string> results;
  boost::split(results, command, [](char c) { return c == ' '; });
  if (bannedCommands.count(results[0]) && getInCombat().count(it->xuid)) {
      auto packet = TextPacket::createTextPacket<TextPacketType::SystemMessage>(settings.blockedCommand);
      it->player->sendNetworkPacket(packet);
      return nullptr;
  }
  return original(snh, netid, pk);
}