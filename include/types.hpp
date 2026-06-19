#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8 = int8_t;
using i16 = int16_t;

static constexpr u32 MEM_SIZE = 65536;

struct Z80Regs
{
    u8 A, B, C, D, E, H, L, F;
    u8 A2, B2, C2, D2, E2, H2, L2, F2;
    u16 PC, SP, IX, IY;
    u8 I, R;
    bool IFF1, IFF2;
    bool halted;
};

struct SymbolEntry
{
    std::string name;
    u16 value;
    bool defined;
    bool global;
    std::string segment;
};

enum class RelocType
{
    ABS16,
    REL8,
    ABS8
};

struct RelocEntry
{
    u32 offset;
    RelocType type;
    std::string symbol;
    i16 addend;
    std::string segment;
};

struct ObjSegment
{
    std::string name;
    std::vector< u8 > data;
    u32 origin;
    bool hasOrigin;
};

struct ObjectFile
{
    std::string filename;
    std::vector< ObjSegment > segments;
    std::vector< SymbolEntry > symbols;
    std::vector< RelocEntry > relocs;
};

struct LinkerMapEntry
{
    std::string module;
    std::string segment;
    u16 base;
    u32 size;
};

class Z80Error : public std::runtime_error
{
  public:
    explicit Z80Error (const std::string &msg) : std::runtime_error (msg)
    {
    }
};