#include <cstring>
#include <fstream>
#include <iostream>

#include "linker.hpp"

static void printUsage ()
{
    std::cerr << "Usage: z80link [options] file1.obj [file2.obj ...]\n"
              << "  -o <out.exe>    output file (default: out.exe)\n"
              << "  -m <out.map>    generate map file\n"
              << "  -org <addr>     load address in hex (default: 0000)\n"
              << "  -reloc          produce relocatable output (Relocating Loader)\n"
              << "  -abs            produce absolute output (Absolute Loader) "
                 "[default]\n";
}

int main (int argc, char **argv)
{
    if (argc < 2)
    {
        printUsage ();
        return 1;
    }

    LinkConfig cfg;
    cfg.outputFile = "out.exe";
    std::vector< std::string > inputs;

    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        if (a == "-o" && i + 1 < argc)
        {
            cfg.outputFile = argv[++i];
        }
        else if (a == "-m" && i + 1 < argc)
        {
            cfg.mapFile = argv[++i];
        }
        else if (a == "-org" && i + 1 < argc)
        {
            cfg.loadAddr = (u16)strtoul (argv[++i], nullptr, 16);
        }
        else if (a == "-reloc")
        {
            cfg.mode = LinkMode::Relocatable;
        }
        else if (a == "-abs")
        {
            cfg.mode = LinkMode::Absolute;
        }
        else if (a[0] != '-')
        {
            inputs.push_back (a);
        }
        else
        {
            std::cerr << "Unknown option: " << a << "\n";
            printUsage ();
            return 1;
        }
    }

    if (inputs.empty ())
    {
        std::cerr << "No input files.\n";
        return 1;
    }

    std::vector< ObjectFile > objects;
    for (auto &f : inputs)
    {
        try
        {
            objects.push_back (ObjFmt::load (f));
        }
        catch (Z80Error &e)
        {
            std::cerr << "Load error: " << e.what () << "\n";
            return 1;
        }
    }

    Linker lnk;
    ExeFile exe;
    try
    {
        exe = lnk.link (objects, cfg);
    }
    catch (Z80Error &e)
    {
        std::cerr << "Link error: " << e.what () << "\n";
        return 1;
    }
    if (!lnk.errors ().empty ())
    {
        for (auto &e : lnk.errors ())
            std::cerr << e << "\n";
        return 1;
    }

    ExeFmt::save (exe, cfg.outputFile);
    std::cout << "Linked: " << cfg.outputFile << "  origin=0x" << std::hex << exe.origin
              << "  size=" << std::dec << exe.data.size () << "\n";

    if (!cfg.mapFile.empty ())
    {
        std::ofstream mf (cfg.mapFile);
        mf << "Linker Map\n";
        mf << std::left;
        mf << "Module                 Segment    Base   Size\n";
        mf << std::string (55, '-') << "\n";
        for (auto &me : lnk.map ())
        {
            char line[128];
            snprintf (line, sizeof (line), "%-22s %-10s %04X   %u", me.module.c_str (),
                      me.segment.c_str (), me.base, me.size);
            mf << line << "\n";
        }
        mf << "\nSymbols:\n";
        for (auto &s : exe.symbols)
            mf << "  " << s.name << " = " << std::hex << s.value << "\n";
        std::cout << "Map: " << cfg.mapFile << "\n";
    }
    return 0;
}