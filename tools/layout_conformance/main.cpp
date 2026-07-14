#include <xanim/xanim.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

static_assert(sizeof(void *) == 4, "layout generator must be compiled for Win32");
static_assert(sizeof(RawFile) == 12, "RawFile x86 layout drift");
static_assert(sizeof(menuDef_t) == 0x11C, "menuDef_t x86 layout drift");

namespace
{
std::filesystem::path outputDirectory;
std::ofstream output;
std::size_t structureCount;
std::size_t expectedStructureCount;
std::size_t memberCount;
std::size_t expectedMemberCount;

void BeginAsset(const char *filename, const char *asset, const char *assetTag,
                std::size_t structures)
{
    if (output.is_open())
        throw std::runtime_error("previous asset manifest was not closed");
    output.open(outputDirectory / filename, std::ios::binary | std::ios::trunc);
    if (!output)
        throw std::runtime_error(std::string("cannot open output ") + filename);
    structureCount = 0;
    expectedStructureCount = structures;
    output << "bmk4-layout-conformance-v1\n"
           << "source kisak-msvc-x86\n"
           << "game \"IW3\"\n"
           << "word-size 32\n"
           << "pointer-size " << sizeof(void *) << "\n"
           << "asset \"" << asset << "\"\n"
           << "asset-tag \"" << assetTag << "\"\n"
           << "structure-count " << structures << "\n\n";
}

void EndAsset()
{
    if (structureCount != expectedStructureCount)
        throw std::runtime_error("structure registry count drift");
    output.flush();
    if (!output)
        throw std::runtime_error("manifest write failed");
    output.close();
}

void BeginStructure(const char *name, const char *kind, const char *source,
                    std::size_t size, std::size_t alignment,
                    std::size_t members)
{
    ++structureCount;
    memberCount = 0;
    expectedMemberCount = members;
    output << "structure \"" << name << "\"\n"
           << "  kind " << kind << "\n"
           << "  source \"" << source << "\"\n"
           << "  size " << size << "\n"
           << "  align " << alignment << "\n"
           << "  member-count " << members << "\n";
}

void EmitMember(const char *name, std::size_t offset, std::size_t size,
                std::size_t alignment)
{
    output << "  member " << memberCount++ << " \"" << name << "\"\n"
           << "    offset " << offset << "\n"
           << "    size " << size << "\n"
           << "    align " << alignment << "\n"
           << "  end-member\n";
}

void EndStructure()
{
    if (memberCount != expectedMemberCount)
        throw std::runtime_error("member registry count drift");
    output << "end-structure\n\n";
}

void Generate()
{
#define BMK4_ASSET_BEGIN(FILE, ASSET, TAG, COUNT) BeginAsset(FILE, ASSET, TAG, COUNT);
#define BMK4_ASSET_END() EndAsset();
#define BMK4_STRUCTURE_BEGIN(TYPE, KIND, SOURCE, COUNT) \
    BeginStructure(#TYPE, KIND, SOURCE, sizeof(TYPE), alignof(TYPE), COUNT);
#define BMK4_MEMBER(TYPE, MEMBER) \
    EmitMember(#MEMBER, offsetof(TYPE, MEMBER), sizeof(((TYPE *)0)->MEMBER), \
               alignof(decltype(((TYPE *)0)->MEMBER)));
#define BMK4_STRUCTURE_END() EndStructure();
#include "layout_registry.inc"
#undef BMK4_STRUCTURE_END
#undef BMK4_MEMBER
#undef BMK4_STRUCTURE_BEGIN
#undef BMK4_ASSET_END
#undef BMK4_ASSET_BEGIN
}
} // namespace

int main(int argc, char **argv)
{
    if (argc != 3 || std::string(argv[1]) != "--output-dir")
    {
        std::cerr << "usage: bmk4-layout-conformance --output-dir <path>\n";
        return 2;
    }

    try
    {
        outputDirectory = std::filesystem::absolute(argv[2]);
        std::filesystem::create_directories(outputDirectory);
        Generate();
        std::cout << "generated RawFile and menuDef_t KISAK x86 manifests in "
                  << outputDirectory.string() << "\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << "layout generation failed: " << error.what() << "\n";
        return 1;
    }
    return 0;
}
