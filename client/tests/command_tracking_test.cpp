#include "../src/SettingsArguments.h"
#include "../src/CommandTracking.h"
#include "../src/GroupMembership.h"
#include "../src/MuteNotice.h"
#include "../src/KillListFormat.h"
#include "../src/WarResultContext.h"
#include "../src/GameMessages.h"
#include "../src/GroupLeadership.h"
#undef NDEBUG
#include <cassert>
#include <iostream>

int main() {
    struct TestServer { char szAddr[128], szNickname[32], szPassword[128]; int iPort; };
    TestServer server = { "play.uifserver.net", "Tester", "", 7776 };
    ApplyServerArguments("\"C:\\Bot Folder\\RakSAMPClient-postfix.exe\"", server);
    assert(server.iPort == 7776);
    ApplyServerArguments("RakSAMPClient-postfix.exe -p 7776 -n \"Test Name\"", server);
    assert(server.iPort == 7776 && std::string(server.szNickname) == "Test Name");
    ApplyServerArguments("bot.exe /p 8888 -z \"secret-p 0\"", server);
    assert(server.iPort == 8888 && std::string(server.szPassword) == "secret-p 0");
    const char *invalidPorts[] = { "0", "65536", "-1", "7776oops", "ostfix.exe", "99999999999999999999", "" };
    for(size_t n = 0; n < sizeof(invalidPorts)/sizeof(invalidPorts[0]); ++n) {
        int port = 7776;
        assert(!ParseServerPort(invalidPorts[n], port) && port == 7776);
    }
    ApplyServerArguments("bot.exe -p 0 -n Other -p", server);
    assert(server.iPort == 8888);
    ApplyServerArguments("bot.exe -p -n Final", server);
    assert(server.iPort == 8888 && std::string(server.szNickname) == "Final");
    int validPort = 0;
    assert(ParseServerPort("1", validPort) && validPort == 1);
    assert(ParseServerPort("65535", validPort) && validPort == 65535);

    assert(FindMemberLeadership("Name\tRank\tRole\tLast Active\nwashe\tBoss\tLeader\tNow", "washe") == MEMBER_ALLOWED);
    assert(FindMemberLeadership("Name\tRank\tRole\tLast Active\nmt07\tUnderboss\tCo-Leader\tNow", "MT07") == MEMBER_ALLOWED);
    assert(FindMemberLeadership("Name\tRank\tRole\tLast Active\nmt07\tLeader\tMember\tNow", "mt07") == MEMBER_DENIED);
    assert(FindMemberLeadership("Name\tRank\tRole\tLast Active\nwashe_fake\tBoss\tLeader\tNow", "washe") == MEMBER_NOT_FOUND);
    assert(FindMemberLeadership("{00FF00}Name\tRank\tRole\tLast Active\n1  {FF0000}washe\tBoss\t{FFFFFF}Leader\tNow", "washe") == MEMBER_ALLOWED);
    assert(FindMemberLeadership("ID\tName\tRank\tRole\tLast Active\n27\twashe\tBoss\tCo Leader\tNow", "washe") == MEMBER_ALLOWED);
    assert(FindMemberLeadership("Name\tRank\tRole\tLast Active\nwashe\tBoss\tNot Leader\tNow", "washe") == MEMBER_DENIED);
    assert(FindMemberLeadership("washe\tLeader\tMember\tNow", "washe") == MEMBER_DENIED);
    assert(FindMemberLeadership("washe\tCustom Rank\tCo-Leader\tNow", "washe") == MEMBER_ALLOWED);
    assert(FindMemberLeadership("Name\tRank\tRole\tLast Active\nwashe\tBoss\tOwner\tNow", "washe") == MEMBER_ALLOWED);
    WarResultContext firstWar;
    firstWar.begin("CROMM", "CROMM", "Seamless Gate", "69", "bot-cromm:war-1");
    assert(firstWar.active() && !firstWar.wonKnown && !firstWar.ownBlue());
    firstWar.observe("ZONE WAR: The Youngsters Family takes over zone '# 69'");
    assert(!firstWar.wonKnown);
    firstWar.observe("ZONE WAR: CROMM loses to Seamless Gate");
    assert(firstWar.wonKnown && !firstWar.won);
    const WarResultContext frozen = firstWar;
    firstWar.begin("CROMM", "CROMM", "New Opponent", "70", "bot-cromm:war-2");
    assert(!firstWar.wonKnown && frozen.blue == "Seamless Gate" && frozen.zone == "69" && !frozen.won);
    WarResultContext otherBot;
    otherBot.begin("The Youngsters Family", "ehales hells", "The Youngsters Family", "19", "bot-hakixld:war-1");
    assert(otherBot.ownBlue());
    otherBot.observe("ZONE WAR: The Youngsters Family takes over zone '# 24'");
    assert(!otherBot.wonKnown);
    otherBot.observe("ZONE WAR: The Youngsters Family takes over zone '# 19'");
    assert(otherBot.wonKnown && otherBot.won && !firstWar.wonKnown);
    otherBot.begin("CROMM", "The Youngsters Family", "ehales hells", "19", "wrong-bot-war");
    assert(!otherBot.active());
    assert(IsKillListPlayerRow("Alice", "0", "0", "0", "0"));
    assert(!IsKillListPlayerRow("uifserver.net", "", "", "", ""));
    assert(!IsKillListPlayerRow("Alice", "0", "0", "", "0"));
    assert(std::string(PlayerStatsMessage(true)).find("paket kaybi") != std::string::npos);
    assert(std::string(PlayerStatsMessage(false)).find("'s PL") != std::string::npos);
    std::string largeRoster;
    for(int i = 0; i < 30; ++i) largeRoster += "Player" + std::to_string(i) + "          10   20   30   4000\n";
    const std::string largeBox = BuildKillListBox(largeRoster, 300, 600, 900, 120000);
    assert(BuildKillListBox("", 0, 0, 0, 0, true).find("Toplam") != std::string::npos);
    assert(BuildKillListBox("", 0, 0, 0, 0, false).find("(empty)") != std::string::npos);
    assert(largeBox.size() > 512);
    assert(largeBox.find("Player29") != std::string::npos && largeBox.find("120000") != std::string::npos);
    // Exact public chat line which caused a reconnect three seconds before GZ ended.
    assert(!IsOwnMuteNotice("_Ermes: (100) DONT SAY YALL MUTED ME"));
    assert(!IsOwnMuteNotice("Player(100): YOU ARE COMMAND MUTED FOR 60 SECONDS"));
    assert(!IsOwnMuteNotice("Player: (100) YOU ARE MUTED FOR 60 SECONDS"));
    assert(!IsOwnMuteNotice("(GROUP) [20] Player(100): YOU ARE MUTED"));
    assert(!IsOwnMuteNotice("ADMIN: X muted Player(100) for 60 minutes"));
    assert(!IsOwnMuteNotice("YOU ARE NOT MUTED"));
    assert(!IsOwnMuteNotice("YOU ARE MUTEDNESS"));
    assert(IsOwnMuteNotice("~g~YOU ARE COMMAND MUTED FOR 60 SECONDS"));
    assert(IsOwnMuteNotice("{FF0000}You are muted for 60 seconds."));
    assert(IsOwnMuteNotice("ERROR: You have been muted for 10 seconds."));
    assert(IsOwnMuteNotice("  ~r~YOU ARE~n~COMMAND MUTED FOR 60 SECONDS  "));
    assert(IsOwnGroupExit("GROUP: (Bot)(12) left the group.", "bot"));
    assert(IsOwnGroupExit("GROUP: Admin(5) kicked Bot(12) from the group.", "bot"));
    assert(IsOwnGroupExit("You have left the group.", "bot"));
    assert(IsOwnGroupExit("GROUP: You have left the group.", "bot"));
    assert(IsOwnGroupExit("Your group has been disbanded.", "bot"));
    assert(!IsOwnGroupExit("GROUP: Alice(2) left the group.", "bot"));
    assert(!IsOwnGroupExit("GROUP: Admin(5) kicked Alice(2) from the group.", "bot"));
    assert(!IsOwnGroupExit("Alice: You have left the group.", "bot"));
    assert(!IsOwnGroupExit("Alice: GROUP: Bot(12) left the group.", "bot"));
    assert(IsGroupLeaveDialog(true, 100, "UIF", "Are you sure you want to leave your group?"));
    assert(!IsGroupLeaveDialog(false, 100, "Leave Group", "Confirm"));
    assert(!IsGroupLeaveDialog(true, 20001, "Leave Group", "Confirm"));
    assert(!IsGroupLeaveDialog(true, 100, "Kick Group Member", "Confirm"));
    assert(GROUP_LEAVE_YES == 0 && GROUP_LEAVE_NO == 1);
    GroupCommandEvent event;
    // Actual DM invite from the log must not trigger group logging or ?post.
    assert(!GroupCommandEvent::parse(">> jugginpeace(58) invites you to his Deathmatch world! To join type '/dm 4'", event));
    assert(!GroupCommandEvent::parse("(GROUP) [20] Player(58): GROUP: washe(2845359) invited DarkLights.CRMM(3756473)", event));
    assert(!GroupCommandEvent::parse("GROUP: jugginpeace(58) invites you to his Deathmatch world!", event));
    assert(!GroupCommandEvent::parse("GROUP: washe(2845359) invited Unknown(0)", event));
    assert(GroupCommandEvent::parse("GROUP: washe(2845359) invited DarkLights.CRMM(3756473)", event));
    assert(event.type == "ginvite" && event.actor == "washe" && event.target == "DarkLights.CRMM");
    assert(GroupCommandEvent::parse("GROUP: (washe)(2845359) invited (DarkLights.CRMM)(3756473)", event));
    assert(GroupCommandEvent::parse("GROUP: (bot)(42) changed the role of Alice(5) from 'Member' to 'Co-Leader'", event));
    assert(event.type == "grole" && event.actor == "bot" && event.target == "Alice" && event.value == "Co-Leader");
    assert(GroupCommandEvent::parse("GROUP: bot(42) changed the rank of Alice(5) from 'Recruit' to 'Veteran'", event));
    assert(event.type == "grank" && event.value == "Veteran");
    assert(GroupCommandEvent::parse("GROUP: (bot)(42) kicked Alice(5) from the group", event));
    assert(event.type == "gkick" && event.actor == "bot" && event.target == "Alice");
    assert(GroupCommandEvent::parse("GROUP: bot(42) invited Alice(5) to the group", event));
    assert(event.type == "ginvite" && event.target == "Alice");
    assert(!GroupCommandEvent::parse("Someone: GROUP: bot(42) kicked Alice(5)", event));
    assert(!GroupCommandEvent::parse("ADMIN: bot(42) kicked Alice(5)", event));
    assert(!GroupCommandEvent::parse("GROUP: bot(42) changed the role of Alice(5)", event));
    PendingGroupCommand pending;
    assert(!pending.active());
    pending.requesterId = "request-1";
    pending.type = "grole";
    pending.target = "Alice";
    pending.value = "Co-Leader";
    pending.started = 100;
    assert(pending.matches("grole", "BOT", "bot", "alice", "co-leader"));
    assert(!pending.matches("grank", "BOT", "bot", "alice", "co-leader"));
    assert(!pending.matches("grole", "OTHER", "bot", "alice", "co-leader"));
    assert(!pending.matches("grole", "BOT", "bot", "bob", "co-leader"));
    assert(!pending.matches("grole", "BOT", "bot", "alice", "Member"));
    assert(!pending.matches("grole", "BOT", "", "alice", "co-leader"));
    assert(!pending.expired(60100));
    assert(pending.expired(60101));
    // Windows GetTickCount wraps every ~49 days.
    pending.started = 0xFFFFFF00UL;
    assert(!pending.expired(0x00000100UL));
    assert(pending.expired(0x00010000UL));
    pending.type = "ginvite";
    pending.value.clear();
    assert(pending.matches("ginvite", "BOT", "bot", "Alice", ""));
    pending = PendingGroupCommand();
    assert(!pending.matches("ginvite", "BOT", "bot", "Alice", ""));
    std::cout << "Command correlation: OK\n";
}
