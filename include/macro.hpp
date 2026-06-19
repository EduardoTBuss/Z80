#pragma once
#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

struct MacroDef
{
    std::string name;
    std::vector< std::string > params;
    std::vector< std::string > body;
};

class MacroProcessor
{
  public:
    std::string process (const std::string &src)
    {
        std::istringstream in (src);
        std::ostringstream out;
        processStream (in, out, {}, {});
        return out.str ();
    }

  private:
    std::unordered_map< std::string, MacroDef > macros_;

    static std::string trim (const std::string &s)
    {
        size_t a = s.find_first_not_of (" \t\r\n");
        if (a == std::string::npos)
            return {};

        size_t b = s.find_last_not_of (" \t\r\n");

        return s.substr (a, b - a + 1);
    }

    static std::string upper (std::string s)
    {
        for (char &c : s)
            c = toupper (c);
        return s;
    }

    static std::string stripComment (const std::string &line)
    {
        bool inStr = false;

        for (size_t i = 0; i < line.size (); ++i)
        {
            if (line[i] == '\'')
                inStr = !inStr;
            if (!inStr && line[i] == ';')
                return line.substr (0, i);
        }

        return line;
    }

    static std::string firstToken (const std::string &line)
    {
        std::istringstream ss (line);
        std::string tok;
        ss >> tok;

        return upper (tok);
    }

    static std::vector< std::string > splitArgs (const std::string &s)
    {
        std::vector< std::string > args;
        std::string cur;
        int depth = 0;

        for (char c : s)
        {
            if (c == '(')
            {
                ++depth;
                cur += c;
            }
            else if (c == ')')
            {
                --depth;
                cur += c;
            }
            else if (c == ',' && depth == 0)
            {
                args.push_back (trim (cur));
                cur.clear ();
            }
            else
            {
                cur += c;
            }
        }

        if (!trim (cur).empty ())
            args.push_back (trim (cur));
        return args;
    }

    static std::string getLabel (const std::string &line)
    {
        if (!line.empty () && (line[0] == '_' || isalpha (line[0])))
        {
            size_t i = 0;

            while (i < line.size () && (isalnum (line[i]) || line[i] == '_'))
                ++i;

            if (i < line.size () && line[i] == ':')
                return line.substr (0, i);
        }
        return {};
    }

    static std::string afterLabel (const std::string &line)
    {
        size_t col = line.find (':');
        if (col == std::string::npos)
            return line;
        return trim (line.substr (col + 1));
    }

    std::string applyParams (const std::string &line,
                             const std::vector< std::string > &pnames,
                             const std::vector< std::string > &pvals)
    {
        std::string result = line;

        for (size_t i = 0; i < pnames.size () && i < pvals.size (); ++i)
        {
            std::string out;

            const std::string &key = pnames[i];
            const std::string &val = pvals[i];

            size_t pos = 0;

            while (pos < result.size ())
            {
                size_t found = result.find (key, pos);

                if (found == std::string::npos)
                {
                    out += result.substr (pos);
                    break;
                }

                bool leftOk = (found == 0 ||
                               !isalnum (result[found - 1]) && result[found - 1] != '_');

                bool rightOk = (found + key.size () >= result.size () ||
                                (!isalnum (result[found + key.size ()]) &&
                                 result[found + key.size ()] != '_'));

                if (leftOk && rightOk)
                {
                    out += result.substr (pos, found - pos);
                    out += val;
                    pos = found + key.size ();
                }
                else
                {
                    out += result.substr (pos, found - pos + 1);
                    pos = found + 1;
                }
            }
            result = out;
        }
        return result;
    }

    bool collectMacroBody (std::istream &in, MacroDef &def, int nestLevel)
    {
        std::string line;

        while (std::getline (in, line))
        {
            std::string clean = trim (stripComment (line));
            std::string lbl = getLabel (clean);

            std::string rest = lbl.empty () ? clean : trim (afterLabel (clean));
            std::string tok = firstToken (rest);

            if (tok == "MACRO")
            {
                def.body.push_back (line);

                MacroDef nested;
                std::string nrest = rest.substr (5);

                size_t sp = nrest.find (' ');

                std::string nname =
                    upper (trim (sp == std::string::npos ? nrest : nrest.substr (0, sp)));
                std::string nargs =
                    sp == std::string::npos ? "" : trim (nrest.substr (sp));

                nested.name = nname;

                if (!nargs.empty ())
                    nested.params = splitArgs (nargs);
                collectMacroBody (in, nested, nestLevel + 1);

                macros_[nname] = nested;
                def.body.push_back (";ENDM_NESTED");

                continue;
            }
            if (tok == "ENDM")
            {
                return true;
            }
            def.body.push_back (line);
        }
        return false;
    }

    void processStream (std::istream &in, std::ostream &out,
                        const std::vector< std::string > &pnames,
                        const std::vector< std::string > &pvals)
    {
        std::string line;

        while (std::getline (in, line))
        {
            std::string subst = applyParams (line, pnames, pvals);
            std::string clean = trim (stripComment (subst));

            std::string lbl = getLabel (clean);
            std::string rest = lbl.empty () ? clean : trim (afterLabel (clean));
            std::string tok = firstToken (rest);

            if (tok == "MACRO")
            {
                MacroDef def;

                std::string mrest = rest.substr (5);
                size_t sp = mrest.find (' ');

                std::string mname =
                    upper (trim (sp == std::string::npos ? mrest : mrest.substr (0, sp)));
                std::string margs =
                    sp == std::string::npos ? "" : trim (mrest.substr (sp));

                def.name = mname;
                if (!margs.empty ())
                    def.params = splitArgs (margs);

                collectMacroBody (in, def, 0);
                macros_[mname] = def;

                continue;
            }

            if (!tok.empty () && macros_.count (tok))
            {
                std::string callArgs =
                    rest.size () > tok.size () ? trim (rest.substr (tok.size ())) : "";
                std::vector< std::string > args = callArgs.empty ()
                                                      ? std::vector< std::string >{}
                                                      : splitArgs (callArgs);

                if (!lbl.empty ())
                    out << lbl << ":\n";
                expandMacro (tok, args, out, pnames, pvals, 0);

                continue;
            }

            out << subst << "\n";
        }
    }

    void expandMacro (const std::string &name, const std::vector< std::string > &args,
                      std::ostream &out, const std::vector< std::string > &outerNames,
                      const std::vector< std::string > &outerVals, int depth)
    {
        if (depth > 64)
            throw std::runtime_error ("Macro recursion too deep: " + name);

        auto it = macros_.find (name);

        if (it == macros_.end ())
            throw std::runtime_error ("Undefined macro: " + name);

        const MacroDef &def = it->second;

        std::string bodyStr;
        for (auto &bl : def.body)
            bodyStr += bl + "\n";

        std::istringstream bodyIn (bodyStr);
        std::ostringstream bodyOut;

        processStream (bodyIn, bodyOut, def.params, args);

        std::string expanded = bodyOut.str ();
        std::istringstream expIn (expanded);
        std::string line;

        while (std::getline (expIn, line))
        {
            std::string clean = trim (stripComment (line));
            std::string lbl = getLabel (clean);

            std::string rest = lbl.empty () ? clean : trim (afterLabel (clean));
            std::string tok = firstToken (rest);

            if (!tok.empty () && macros_.count (tok))
            {
                std::string callArgs =
                    rest.size () > tok.size () ? trim (rest.substr (tok.size ())) : "";
                std::vector< std::string > cargs = callArgs.empty ()
                                                       ? std::vector< std::string >{}
                                                       : splitArgs (callArgs);

                if (!lbl.empty ())
                    out << lbl << ":\n";
                expandMacro (tok, cargs, out, {}, {}, depth + 1);
            }
            else
            {
                out << line << "\n";
            }
        }
    }
};