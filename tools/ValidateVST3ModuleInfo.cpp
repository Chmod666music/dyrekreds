#include <public.sdk/source/vst/moduleinfo/moduleinfoparser.h>

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

int main (int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: DyrekredsVST3ManifestValidator <moduleinfo.json>\n";
        return 2;
    }

    std::ifstream input (argv[1], std::ios::binary);

    if (! input)
    {
        std::cerr << "Could not open VST3 manifest: " << argv[1] << '\n';
        return 2;
    }

    const std::string json { std::istreambuf_iterator<char> { input }, {} };

    if (! Steinberg::ModuleInfoLib::parseJson (json, &std::cerr))
    {
        std::cerr << "Invalid VST3 moduleinfo.json: " << argv[1] << '\n';
        return 1;
    }

    return 0;
}
