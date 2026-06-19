#pragma once
#include <algorithm>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "objfmt.hpp"
#include "types.hpp"

enum class LinkMode
{
    Absolute,
    Relocatable
};

struct LinkConfig
{
    u16 loadAddr;
    LinkMode mode;
    std::vector< std::string > inputFiles;
    std::string outputFile;
    std::string mapFile;
    LinkConfig () : loadAddr (0x0000), mode (LinkMode::Absolute)
    {
    }
};

struct ExeFile
{
    u16 origin;
    std::vector< u8 > data;
    std::vector< RelocEntry > relocs;
    std::vector< SymbolEntry > symbols;
};

class Linker
{
  public:
    ExeFile link (const std::vector< ObjectFile > &objects, const LinkConfig &cfg)
    {
        errors_.clear ();
        map_.clear ();

        pass1 (objects, cfg);
        return pass2 (objects, cfg);
    }

    const std::vector< std::string > &errors () const
    {
        return errors_;
    }
    const std::vector< LinkerMapEntry > &map () const
    {
        return map_;
    }

  private:
    std::unordered_map< std::string, u16 > globalSyms_;
    std::vector< std::string > errors_;
    std::vector< LinkerMapEntry > map_;

    struct SegPlacement
    {
        std::string module;
        std::string segment;
        u16 base;
        u32 size;
    };

    void pass1 (const std::vector< ObjectFile > &objs, const LinkConfig &cfg)
    {
        globalSyms_.clear ();
        u16 cursor = cfg.loadAddr;

        std::vector< SegPlacement > placements;
        for (auto &obj : objs)
        {
            for (auto &seg : obj.segments)
            {
                SegPlacement sp;
                sp.module = obj.filename;
                sp.segment = seg.name;
                sp.size = (u32)seg.data.size ();
                if (seg.hasOrigin)
                {
                    cursor = (u16)seg.origin;
                }
                sp.base = cursor;
                cursor += (u16)sp.size;
                placements.push_back (sp);
                LinkerMapEntry me;
                me.module = sp.module;
                me.segment = sp.segment;
                me.base = sp.base;
                me.size = sp.size;
                map_.push_back (me);
            }
        }

        for (size_t oi = 0; oi < objs.size (); ++oi)
        {
            auto &obj = objs[oi];
            for (auto &sym : obj.symbols)
            {
                if (!sym.defined)
                    continue;
                u16 base = segBase (placements, obj.filename, sym.segment);
                u16 resolved = base + sym.value;
                if (globalSyms_.count (sym.name) && sym.global)
                {
                    if (globalSyms_[sym.name] != resolved)
                        errors_.push_back ("Duplicate global: " + sym.name);
                }
                else
                {
                    globalSyms_[sym.name] = resolved;
                }
            }
        }

        for (auto &obj : objs)
        {
            for (auto &sym : obj.symbols)
            {
                if (!sym.defined && !globalSyms_.count (sym.name))
                {
                    errors_.push_back ("Undefined external: " + sym.name);
                }
            }
        }
    }

    ExeFile pass2 (const std::vector< ObjectFile > &objs, const LinkConfig &cfg)
    {
        ExeFile exe;
        exe.origin = cfg.loadAddr;

        std::vector< SegPlacement > placements;
        u16 cursor = cfg.loadAddr;
        for (auto &obj : objs)
        {
            for (auto &seg : obj.segments)
            {
                SegPlacement sp;
                sp.module = obj.filename;
                sp.segment = seg.name;
                sp.size = (u32)seg.data.size ();
                if (seg.hasOrigin)
                    cursor = (u16)seg.origin;
                sp.base = cursor;
                cursor += (u16)sp.size;
                placements.push_back (sp);
            }
        }

        u32 totalSize = (u32)(cursor - cfg.loadAddr);
        exe.data.resize (totalSize, 0);

        for (auto &obj : objs)
        {
            for (auto &seg : obj.segments)
            {
                u16 base = segBase (placements, obj.filename, seg.name);
                u32 off = base - cfg.loadAddr;
                if (off + seg.data.size () > exe.data.size ())
                {
                    errors_.push_back ("Segment overflow: " + seg.name);
                    continue;
                }
                std::copy (seg.data.begin (), seg.data.end (), exe.data.begin () + off);
            }

            for (auto &reloc : obj.relocs)
            {
                u16 segBase_ = segBase (placements, obj.filename, reloc.segment);
                u32 absOff = (segBase_ - cfg.loadAddr) + reloc.offset;

                u16 symVal = 0;
                auto it = globalSyms_.find (reloc.symbol);
                if (it != globalSyms_.end ())
                {
                    symVal = it->second;
                }
                else
                {
                    for (auto &sym : obj.symbols)
                    {
                        if (sym.name == reloc.symbol && sym.defined)
                        {
                            u16 sb = segBase (placements, obj.filename, sym.segment);
                            symVal = sb + sym.value;
                            break;
                        }
                    }
                }
                symVal += reloc.addend;

                if (cfg.mode == LinkMode::Absolute)
                {
                    if (reloc.type == RelocType::ABS16)
                    {
                        if (absOff + 1 < exe.data.size ())
                        {
                            exe.data[absOff] = (u8)(symVal & 0xFF);
                            exe.data[absOff + 1] = (u8)(symVal >> 8);
                        }
                    }
                    else if (reloc.type == RelocType::ABS8)
                    {
                        if (absOff < exe.data.size ())
                            exe.data[absOff] = (u8)(symVal & 0xFF);
                    }
                    else if (reloc.type == RelocType::REL8)
                    {
                        u16 pc = segBase_ + (u16)reloc.offset + 2;
                        i8 rel = (i8)((i16)symVal - (i16)pc);
                        if (absOff < exe.data.size ())
                            exe.data[absOff] = (u8)rel;
                    }
                }
                else
                {
                    RelocEntry re = reloc;
                    re.offset = absOff;
                    re.addend = (i16)symVal;
                    exe.relocs.push_back (re);
                }
            }
        }

        for (auto &[name, val] : globalSyms_)
        {
            SymbolEntry se;
            se.name = name;
            se.value = val;
            se.defined = true;
            se.global = true;
            exe.symbols.push_back (se);
        }

        return exe;
    }

    u16 segBase (const std::vector< SegPlacement > &placements, const std::string &module,
                 const std::string &seg)
    {
        for (auto &p : placements)
            if (p.module == module && p.segment == seg)
                return p.base;
        return 0;
    }
};

namespace ExeFmt
{

static constexpr u32 MAGIC = 0x5A383045;

inline void save (const ExeFile &exe, const std::string &path)
{
    std::ofstream f (path, std::ios::binary);
    if (!f)
        throw Z80Error ("Cannot write exe: " + path);
    ObjFmt::writeU32 (f, MAGIC);
    ObjFmt::writeU16 (f, exe.origin);
    ObjFmt::writeU32 (f, (u32)exe.data.size ());
    f.write ((char *)exe.data.data (), exe.data.size ());
    ObjFmt::writeU16 (f, (u16)exe.relocs.size ());
    for (auto &r : exe.relocs)
    {
        ObjFmt::writeU32 (f, r.offset);
        ObjFmt::writeU8 (f, (u8)r.type);
        ObjFmt::writeStr (f, r.symbol);
        ObjFmt::writeU16 (f, (u16)(i16)r.addend);
    }
    ObjFmt::writeU16 (f, (u16)exe.symbols.size ());
    for (auto &s : exe.symbols)
    {
        ObjFmt::writeStr (f, s.name);
        ObjFmt::writeU16 (f, s.value);
    }
}

inline ExeFile load (const std::string &path)
{
    std::ifstream f (path, std::ios::binary);
    if (!f)
        throw Z80Error ("Cannot read exe: " + path);
    u32 magic = ObjFmt::readU32 (f);
    if (magic != MAGIC)
        throw Z80Error ("Bad exe magic: " + path);
    ExeFile exe;
    exe.origin = ObjFmt::readU16 (f);
    u32 sz = ObjFmt::readU32 (f);
    exe.data.resize (sz);
    f.read ((char *)exe.data.data (), sz);
    u16 nrel = ObjFmt::readU16 (f);
    exe.relocs.resize (nrel);
    for (auto &r : exe.relocs)
    {
        r.offset = ObjFmt::readU32 (f);
        r.type = (RelocType)ObjFmt::readU8 (f);
        r.symbol = ObjFmt::readStr (f);
        r.addend = (i16)ObjFmt::readU16 (f);
    }
    u16 nsym = ObjFmt::readU16 (f);
    exe.symbols.resize (nsym);
    for (auto &s : exe.symbols)
    {
        s.name = ObjFmt::readStr (f);
        s.value = ObjFmt::readU16 (f);
        s.defined = true;
        s.global = true;
    }
    return exe;
}

} // namespace ExeFmt