#ifndef KILL_LIST_FORMAT_H
#define KILL_LIST_FORMAT_H
#include <string>
#include <sstream>
#include <iomanip>

inline bool IsKillListNumber(const char *text) {
    if(!text || !text[0]) return false;
    for(const char *p = text; *p; ++p) if(*p < '0' || *p > '9') return false;
    return true;
}
inline bool IsKillListPlayerRow(const char *name, const char *kills, const char *assists, const char *deaths, const char *damage) {
    return name && name[0] && IsKillListNumber(kills) && IsKillListNumber(assists) && IsKillListNumber(deaths) && IsKillListNumber(damage);
}

inline std::string BuildKillListBox(const std::string &rows, int kills, int assists, int deaths, int damage, bool turkish = false) {
    const char *dash = "--------------------------------------";
    std::ostringstream out;
    out << std::left << std::setw(16) << (turkish ? "Isim" : "Name") << std::right
        << ' ' << std::setw(4) << "K" << ' ' << std::setw(4) << "A"
        << ' ' << std::setw(4) << "D" << ' ' << std::setw(6) << "DMG"
        << '\n' << dash << '\n' << (rows.empty() ? (turkish ? "(bos)" : "(empty)") : rows) << '\n' << dash << '\n'
        << std::left << std::setw(16) << (turkish ? "Toplam" : "Total") << std::right
        << ' ' << std::setw(4) << kills << ' ' << std::setw(4) << assists
        << ' ' << std::setw(4) << deaths << ' ' << std::setw(6) << damage;
    return out.str();
}
#endif
