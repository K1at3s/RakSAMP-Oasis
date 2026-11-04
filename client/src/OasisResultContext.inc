static std::string OasisTrim(std::string value) {
    size_t first = value.find_first_not_of(" \t\r\n");
    if(first == std::string::npos) return "";
    value = value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
    if(!value.empty() && value.back() == '.') value.pop_back();
    return value;
}
static std::string OasisResolveSide(const std::string &value) {
    if(PendingGroupCommand::equal(value, g_myGroupName)) return g_myGroupName;
    std::string match;
    for(int i = 0; i < g_groupColorCount; ++i) {
        if(PendingGroupCommand::equal(value, g_groupColors[i].szName) || PendingGroupCommand::equal(value, g_groupColors[i].szTag)) {
            if(!match.empty() && !PendingGroupCommand::equal(match, g_groupColors[i].szName)) return "";
            match = g_groupColors[i].szName;
        }
    }
    return match.empty() ? value : match;
}
static std::string OasisTagFor(const std::string &group) {
    for(int i = 0; i < g_groupColorCount; ++i)
        if(PendingGroupCommand::equal(group, g_groupColors[i].szName) && g_groupColors[i].szTag[0]) return g_groupColors[i].szTag;
    return group;
}
static void ObserveOasisWarNotice(const char *raw) {
    const std::string message = raw ? raw : "";
    const size_t start = message.find("Started at "), vs = message.find(" vs. ");
    if(start == std::string::npos || vs == std::string::npos || g_myGroupName.empty()) return;
    const size_t separator = message.rfind(" - ", vs);
    if(separator == std::string::npos || separator <= start) return;
    const std::string red = OasisResolveSide(OasisTrim(message.substr(separator + 3, vs - separator - 3)));
    const std::string blue = OasisResolveSide(OasisTrim(message.substr(vs + 5)));
    if(red.empty() || blue.empty() || (!PendingGroupCommand::equal(red, g_myGroupName) && !PendingGroupCommand::equal(blue, g_myGroupName))) return;
    if(s_warResultContext.active() && PendingGroupCommand::equal(s_warResultContext.red, red) &&
        PendingGroupCommand::equal(s_warResultContext.blue, blue) && g_inZoneWar) return;
    if(!g_inZoneWar) {
        g_zoneWarStartTick = ClientTick();
        g_zwKillListSent = false; g_zwCapturingKillList = false; g_zwKillTdCount = 0;
        g_zwFirstBlood[0] = 0; g_zwWarKillCount = 0;
        g_zwAutoJoinSent = g_zwCheckJoinSent = g_zwKillListStay = false;
        g_gztCountdownAnnounced = 999; g_gztLastRem = -1;
        g_warRemainingSec = -1; g_warTimerTick = 0;
    }
    g_inZoneWar = true; s_contextWarTick = g_zoneWarStartTick;
    const std::string zone = OasisTrim(message.substr(start + 11, separator - start - 11));
    g_zwWarTeamLeft = red; g_zwWarTeamRight = blue;
    s_warResultContext.begin(g_myGroupName, red, blue, zone,
        std::string("oasis:") + g_szNickName + ":" + std::to_string((long long)time(NULL)) + ":" + std::to_string(s_contextWarTick));
    g_zwWeAreBlue = PendingGroupCommand::equal(blue, g_myGroupName); g_zwWeAreBlueKnown = true;
}
static bool PrepareOasisResult(const char *tagA, const char *tagB, int scoreA, int scoreB,
    std::string &nameA, std::string &nameB, bool &won) {
    if(!s_warResultContext.active() || !PendingGroupCommand::equal(s_warResultContext.ownGroup, g_myGroupName)) return false;
    if(ClientTick() - s_contextWarTick < 15000 || scoreA < 0 || scoreB < 0) return false;
    const std::string redTag = OasisTagFor(s_warResultContext.red), blueTag = OasisTagFor(s_warResultContext.blue);
    if(PendingGroupCommand::equal(tagA, redTag) && PendingGroupCommand::equal(tagB, blueTag)) {
        nameA = s_warResultContext.red; nameB = s_warResultContext.blue;
    } else if(PendingGroupCommand::equal(tagA, blueTag) && PendingGroupCommand::equal(tagB, redTag)) {
        nameA = s_warResultContext.blue; nameB = s_warResultContext.red;
    } else return false;
    if(scoreA == scoreB) {
        if(!s_warResultContext.wonKnown) return false;
        won = s_warResultContext.won;
    } else {
        won = PendingGroupCommand::equal(nameA, s_warResultContext.ownGroup) ? scoreA > scoreB : scoreB > scoreA;
        if(s_warResultContext.wonKnown && won != s_warResultContext.won) return false;
        s_warResultContext.outcome(won);
    }
    return true;
}
