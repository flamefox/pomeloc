#include "pomeloc/pomeloc.h"
#include "pomeloc/idl.h"
#include "pomeloc/util.h"
#include <limits>

#define POMELOC_VERSION "0.0.1 (" __DATE__ ")"
const char* SERVER_PROTOS = "serverProtos.json";
const char* CLIENT_PROTOS = "clientProtos.json";

static void Error(const std::string &err, bool usage = false,
    bool show_exe_name = true);

// This struct allows us to create a table of all possible output generators
// for the various programming languages and formats we support.
struct Generator
{
    bool(*generate)(const pomeloc::Parser &parser,
        const std::string &path,
        const std::string &file_name);
    const char *generator_opt_short;
    const char *generator_opt_long;
    const char *lang_name;
    pomeloc::IDLOptions::Language lang;
    const char *generator_help;
};

const Generator generators[] =
{
    {
        pomeloc::GenerateGeneral,  "-n", "--csharp", "C#",
        pomeloc::IDLOptions::kCSharp,
        "Generate C# classes for tables/structs"
    },
};

const char *program_name = nullptr;
pomeloc::Parser *parser = nullptr;

static void Error(const std::string &err, bool usage, bool show_exe_name)
{
    if (show_exe_name) printf("%s: ", program_name);
    printf("%s\n", err.c_str());
    if (usage)
    {
        printf("usage: %s [OPTION]... [%s] [%s]\n", program_name, SERVER_PROTOS, CLIENT_PROTOS);
        for (size_t i = 0; i < sizeof(generators) / sizeof(generators[0]); ++i)
            printf("  %-12s %s %s.\n",
                generators[i].generator_opt_long,
                generators[i].generator_opt_short
                ? generators[i].generator_opt_short
                : "  ",
                generators[i].generator_help);
        printf(
            "  -o PATH         Prefix PATH to all generated files.\n"
            "  --version       Print the version number of flatc and exit.\n"
            "  --ns            Use custom namespace or empty\n"
            "Output files are named using the base file name of the input,\n"
            "and written to the current directory or the path given by -o.\n"
            "example: %s -n -o ./out %s %s.\n",
            program_name, SERVER_PROTOS, CLIENT_PROTOS);
    }
    if (parser) delete parser;
    exit(1);
}

void ParseFile(std::string file, pomeloc::Parser* pp)
{
    std::string contents;
    if (!pomeloc::LoadFile(file.c_str(), true, &contents))
        Error("unable to load file: " + file);

    // Check if file contains 0 bytes.
    if (contents.length() != strlen(contents.c_str()))
    {
        Error("input file appears to be binary: " + file, true);
    }

    if (!pp->Parse(contents.c_str(), file.c_str()))
        Error(pp->error_, false, false);
}

int main(int argc, const char *argv[])
{
    program_name = argv[0];
    pomeloc::IDLOptions opts;
    std::string output_path;
    const size_t num_generators = sizeof(generators) / sizeof(generators[0]);
    bool generator_enabled[num_generators] =
    { false };
    bool any_generator = false;
    bool raw_binary = false;
    bool schema_binary = false;
    std::vector<std::string> filenames;
    std::vector<const char *> include_directories;
    for (int argi = 1; argi < argc; argi++)
    {
        std::string arg = argv[argi];
        if (arg[0] == '-')
        {
            if (filenames.size() && arg[1] != '-')
                Error("invalid option location: " + arg, true);
            if (arg == "-o")
            {
                if (++argi >= argc) Error("missing path following: " + arg, true);
                output_path = pomeloc::ConCatPathFileName(argv[argi], "");
            }
            else if (arg == "--version")
            {
                printf("pomeloc version %s\n", POMELOC_VERSION);
                exit(0);
            }
            else if (arg == "--ns")
            {
                if (++argi >= argc) Error("missing namespace following: " + arg, true);
                opts.custom_ns = argv[argi];
            }
            else
            {
                for (size_t i = 0; i < num_generators; ++i)
                {
                    if (arg == generators[i].generator_opt_long ||
                        (generators[i].generator_opt_short &&
                            arg == generators[i].generator_opt_short))
                    {
                        generator_enabled[i] = true;
                        any_generator = true;
                        goto found;
                    }
                }
                Error("unknown commandline argument" + arg, true);
            found:;
            }
        }
        else
        {
            if (filenames.size() >= 2)
            {
                Error("too many input files", true);
            }
            std::string strfile(argv[argi]);
            if (strfile.find(SERVER_PROTOS) != std::string::npos)
            {
                filenames.push_back(strfile);
            }
            if (strfile.find(CLIENT_PROTOS) != std::string::npos)
            {
                filenames.push_back(strfile);
                if(filenames.size() > 1)
                {
                    std::swap(filenames[0], filenames[1]);
                }
            }
        }
    }

    if (!filenames.size()) Error("missing input files", false, true);
    if (!any_generator)
    {
        Error("no options: specify at least one generator.", true);
    }

    // Now process the files:
    pomeloc::Parser* parserClient = new pomeloc::Parser(opts);
    pomeloc::Parser* parserServer = new pomeloc::Parser(opts);
    std::string file;
    if (filenames.size() > 1)
    {
        ParseFile(filenames.at(1), parserServer);
        ParseFile(filenames.at(0), parserClient);
        file = filenames.at(0);
    }
    else
    {
        ParseFile(filenames.at(0), parserClient);
        file = filenames.at(0);
    }

    for (auto& item : parserServer->structs_)
    {
        if (item.is_event_)
        {
            parserClient->event_structs_.push_back(item);
        }
        else
        {
            pomeloc::MetaStruct ms;
            std::string name = item.router_;
            ms.name_ = name.substr(name.find_last_of('.') + 1);
            ms.name_ += "_result";
            ms.structs_ = item.structs_;
            ms.vars_ = item.vars_;
            parserClient->response_maps_[name] = ms;
        }
    }
    
    std::string filebase = pomeloc::StripPath(
        pomeloc::StripExtension(file));
    for (size_t i = 0; i < num_generators; ++i)
    {
        parserClient->opts.lang = generators[i].lang;
        if (generator_enabled[i])
        {
            pomeloc::EnsureDirExists(output_path);
            if (!generators[i].generate(*parserClient, output_path, filebase))
            {
                Error(std::string("Unable to generate ") +
                    generators[i].lang_name +
                    " for " +
                    filebase);
            }
        }
    }

    delete parserClient;
    delete parserServer;
    return 0;
}
