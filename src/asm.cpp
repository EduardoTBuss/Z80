#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

#include "assembler.hpp"
#include "macro.hpp"
#include "objfmt.hpp"

int main (int argc, char **argv)
{
    if (argc < 3)
    {
        std::cerr << "Usage: z80asm <input.asm> <output.obj> [--no-macro]\n";
        return 1;
    }
    std::string infile = argv[1];
    std::string outfile = argv[2];
    bool doMacro = true;
    for (int i = 3; i < argc; ++i)
        if (std::string (argv[i]) == "--no-macro")
            doMacro = false;

    std::ifstream fin (infile);
    if (!fin)
    {
        std::cerr << "Cannot open: " << infile << "\n";
        return 1;
    }
    std::ostringstream buf;
    buf << fin.rdbuf ();
    std::string src = buf.str ();

    if (doMacro)
    {
        MacroProcessor mp;
        try
        {
            src = mp.process (src);
        }
        catch (std::exception &e)
        {
            std::cerr << "Macro error: " << e.what () << "\n";
            return 1;
        }
    }

    Assembler asm_;
    try
    {
        ObjectFile obj = asm_.assemble (src, infile);
        ObjFmt::save (obj, outfile);
        std::cout << "Assembled: " << outfile << "\n";
        std::cout << "Symbols:\n";
        for (auto &s : obj.symbols)
            if (s.defined)
                std::cout << "  " << s.name << " = 0x" << std::hex << s.value << "\n";
        return 0;
    }
    catch (Z80Error &e)
    {
        std::cerr << "Assembly error:\n" << e.what () << "\n";
        return 1;
    }
}