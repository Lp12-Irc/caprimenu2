#pragma once
#include <cstdint>
#include "generated_offsets.hpp"

#include "skeleton.hpp"

namespace weapons {
    inline const char* name(uint16_t id) {
        switch (id) {
            case 1:  return "DEagle";   case 2:  return "Duals";
            case 3:  return "FiveSeveN";case 4:  return "Glock";
            case 7:  return "AK-47";    case 8:  return "AUG";
            case 9:  return "AWP";      case 10: return "FAMAS";
            case 11: return "G3SG1";    case 13: return "Galil";
            case 14: return "M249";     case 16: return "M4A4";
            case 17: return "MAC-10";   case 19: return "P90";
            case 23: return "MP5-SD";   case 24: return "UMP-45";
            case 25: return "XM1014";   case 26: return "Bizon";
            case 27: return "MAG-7";    case 28: return "Negev";
            case 29: return "Sawed-Off";case 30: return "Tec-9";
            case 31: return "Zeus";     case 32: return "P2000";
            case 33: return "MP7";      case 34: return "MP9";
            case 35: return "Nova";     case 36: return "P250";
            case 38: return "SCAR-20";  case 39: return "SG553";
            case 40: return "SSG08";    case 60: return "M4A1-S";
            case 61: return "USP-S";    case 63: return "CZ75";
            case 64: return "R8";
            // Utilities
            case 43: return "Flash";    case 44: return "HE";
            case 45: return "Smoke";    case 46: return "Molotov";
            case 47: return "Decoy";    case 48: return "Incend.";
            case 49: return "C4";
            default: return "???";
        }
    }

    inline const char* util_tag(uint16_t id) {
        switch (id) {
            case 43: return "FL"; case 44: return "HE";
            case 45: return "SM"; case 46: return "MOL";
            case 47: return "DEC"; case 48: return "INC";
            default: return nullptr;
        }
    }

    inline bool is_utility(uint16_t id) {
        return id == 43 || id == 44 || id == 45 ||
               id == 46 || id == 47 || id == 48;
    }
}
