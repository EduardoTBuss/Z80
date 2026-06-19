#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "types.hpp"

enum class AddrMode
{
    Implied,
    Imm8,
    Imm16,
    RegA,
    RegB,
    RegC,
    RegD,
    RegE,
    RegH,
    RegL,
    RegBC,
    RegDE,
    RegHL,
    RegSP,
    RegAF,
    RegIX,
    RegIY,
    IndHL,
    IndBC,
    IndDE,
    IndSP,
    IndIX,
    IndIY,
    IndAddr,
    Rel8
};

struct RegCode
{
    static int r8 (const std::string &s)
    {
        if (s == "B")
            return 0;
        if (s == "C")
            return 1;
        if (s == "D")
            return 2;
        if (s == "E")
            return 3;
        if (s == "H")
            return 4;
        if (s == "L")
            return 5;
        if (s == "(HL)")
            return 6;
        if (s == "A")
            return 7;
        return -1;
    }
    static int rp (const std::string &s)
    {
        if (s == "BC")
            return 0;
        if (s == "DE")
            return 1;
        if (s == "HL")
            return 2;
        if (s == "SP")
            return 3;
        return -1;
    }
    static int rpAF (const std::string &s)
    {
        if (s == "BC")
            return 0;
        if (s == "DE")
            return 1;
        if (s == "HL")
            return 2;
        if (s == "AF")
            return 3;
        return -1;
    }
    static bool isR8 (const std::string &s)
    {
        return r8 (s) >= 0;
    }
    static bool isRP (const std::string &s)
    {
        return rp (s) >= 0;
    }
};

struct EncResult
{
    std::vector< u8 > bytes;
    std::string relocSym;
    RelocType relocType;
    i16 relocAddend;
    bool hasReloc;
    EncResult () : relocType (RelocType::ABS16), relocAddend (0), hasReloc (false)
    {
    }
};

struct ParsedLine
{
    std::string label;
    std::string op;
    std::vector< std::string > operands;
    int lineNum;
    std::string raw;
};