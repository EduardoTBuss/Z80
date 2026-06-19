#include "macro.hpp"

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

int main (int argc, char **argv)
{
    if (argc < 3)
    {
        std::cerr << "Usage: z80macro <input.asm> <output.asm>\n";
        return 1;
    }
    std::ifstream in (argv[1]);
    if (!in)
    {
        std::cerr << "Cannot open: " << argv[1] << "\n";
        return 1;
    }
    std::ostringstream buf;
    buf << in.rdbuf ();
    MacroProcessor mp;
    try
    {
        std::string result = mp.process (buf.str ());
        std::ofstream out (argv[2]);
        if (!out)
        {
            std::cerr << "Cannot write: " << argv[2] << "\n";
            return 1;
        }
        out << result;
        std::cout << "Macro expansion done: " << argv[2] << "\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "Macro error: " << e.what () << "\n";
        return 1;
    }
    return 0;
}