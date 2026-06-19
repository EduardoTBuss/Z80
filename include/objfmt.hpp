#pragma once
#include <cstring>
#include <fstream>

#include "types.hpp"

namespace ObjFmt
{

static constexpr u32 MAGIC = 0x5A38304F;
static constexpr u8 VERSION = 1;

inline void writeU8 (std::ostream &o, u8 v)
{
    o.write ((char *)&v, 1);
}
inline void writeU16 (std::ostream &o, u16 v)
{
    u16 le = v;
    o.write ((char *)&le, 2);
}
inline void writeU32 (std::ostream &o, u32 v)
{
    u32 le = v;
    o.write ((char *)&le, 4);
}

inline void writeStr (std::ostream &o, const std::string &s)
{
    u16 len = (u16)s.size ();
    writeU16 (o, len);
    o.write (s.data (), len);
}

inline u8 readU8 (std::istream &i)
{
    u8 v = 0;
    i.read ((char *)&v, 1);
    return v;
}
inline u16 readU16 (std::istream &i)
{
    u16 v = 0;
    i.read ((char *)&v, 2);
    return v;
}
inline u32 readU32 (std::istream &i)
{
    u32 v = 0;
    i.read ((char *)&v, 4);
    return v;
}

inline std::string readStr (std::istream &i)
{
    u16 len = readU16 (i);
    std::string s (len, '\0');

    i.read (&s[0], len);

    return s;
}

inline void save (const ObjectFile &obj, const std::string &path)
{
    std::ofstream f (path, std::ios::binary);

    if (!f)
        throw Z80Error ("Cannot write: " + path);

    writeU32 (f, MAGIC);
    writeU8 (f, VERSION);
    writeStr (f, obj.filename);

    writeU16 (f, (u16)obj.segments.size ());

    for (auto &seg : obj.segments)
    {
        writeStr (f, seg.name);
        writeU8 (f, seg.hasOrigin ? 1 : 0);

        writeU16 (f, (u16)seg.origin);
        writeU32 (f, (u32)seg.data.size ());

        f.write ((char *)seg.data.data (), seg.data.size ());
    }

    writeU16 (f, (u16)obj.symbols.size ());
    for (auto &sym : obj.symbols)
    {
        writeStr (f, sym.name);
        writeU16 (f, sym.value);

        writeU8 (f, sym.defined ? 1 : 0);
        writeU8 (f, sym.global ? 1 : 0);

        writeStr (f, sym.segment);
    }

    writeU16 (f, (u16)obj.relocs.size ());
    for (auto &r : obj.relocs)
    {
        writeU32 (f, r.offset);
        writeU8 (f, (u8)r.type);

        writeStr (f, r.symbol);
        writeU16 (f, (u16)(i16)r.addend);

        writeStr (f, r.segment);
    }
}

inline ObjectFile load (const std::string &path)
{
    std::ifstream f (path, std::ios::binary);
    if (!f)
        throw Z80Error ("Cannot read: " + path);

    u32 magic = readU32 (f);
    if (magic != MAGIC)
        throw Z80Error ("Bad object magic: " + path);

    u8 ver = readU8 (f);
    if (ver != VERSION)
        throw Z80Error ("Unsupported object version");

    ObjectFile obj;
    obj.filename = readStr (f);

    u16 nseg = readU16 (f);
    obj.segments.resize (nseg);

    for (auto &seg : obj.segments)
    {
        seg.name = readStr (f);
        seg.hasOrigin = readU8 (f) != 0;
        seg.origin = readU16 (f);
        u32 sz = readU32 (f);

        seg.data.resize (sz);
        f.read ((char *)seg.data.data (), sz);
    }

    u16 nsym = readU16 (f);
    obj.symbols.resize (nsym);

    for (auto &sym : obj.symbols)
    {
        sym.name = readStr (f);
        sym.value = readU16 (f);

        sym.defined = readU8 (f) != 0;
        sym.global = readU8 (f) != 0;

        sym.segment = readStr (f);
    }

    u16 nrel = readU16 (f);
    obj.relocs.resize (nrel);

    for (auto &r : obj.relocs)
    {
        r.offset = readU32 (f);
        r.type = (RelocType)readU8 (f);

        r.symbol = readStr (f);
        r.addend = (i16)readU16 (f);

        r.segment = readStr (f);
    }
    return obj;
}

} // namespace ObjFmt