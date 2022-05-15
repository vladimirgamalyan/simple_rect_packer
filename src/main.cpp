#include <iostream>
#include <fstream>
#include <vector>
#include "external/CLI11.hpp"
#include "external/json.hpp"
#include "external/maxRectsBinPack/MaxRectsBinPack.h"

struct Size
{
    Size() = default;
    Size(uint32_t w, uint32_t h) : w(w), h(h) {}
    uint32_t w = 0;
    uint32_t h = 0;
};

int main(int argc, char* argv[])
{
    try
    {
        CLI::App app{ "Simple Rect Packer" };

        std::string inputFile;
        std::string outputFile;
        app.add_option("-i,--input_file", inputFile, "input file")->required()->check(CLI::ExistingFile);
        app.add_option("-o,--output_file", outputFile, "output file")->required();

        CLI11_PARSE(app, argc, argv);

        nlohmann::json j;
        {
            std::ifstream i(inputFile);
            i >> j;
        }

        auto config = j["config"];
        std::uint32_t spacing_hor = config["spacingX"];
        std::uint32_t spacing_ver = config["spacingY"];
        std::uint32_t borderX = config["borderX"];
        std::uint32_t borderY = config["borderY"];
        bool cropX = config["cropX"];
        bool cropY = config["cropY"];

        std::vector<rbp::RectSize> glyphRectangles;
        for (uint32_t i = 0; i < j["rects"].size(); ++i)
            glyphRectangles.emplace_back(j["rects"][i]["w"] + spacing_hor, j["rects"][i]["h"] + spacing_ver, i);

        std::vector<Size> textureSizeList = {
            {16, 16},
            {32, 16},
            {16, 32},
            {32, 32},
            {64, 32},
            {32, 64},
            {64, 64},
            {128, 64},
            {64, 128},
            {128, 128},
            {256, 128},
            {128, 256},
            {256, 256},
            {512, 256},
            {256, 512},
            {512, 512},
            {1024, 512},
            {512, 1024},
            {1024, 1024},
            {2048, 1024},
            {1024, 2048},
            {2048, 2048}
        };

        std::vector<Size> textures;

        rbp::MaxRectsBinPack mrbp;

        for (;;)
        {
            std::vector<rbp::Rect> arrangedRectangles;
            auto glyphRectanglesCopy = glyphRectangles;
            Size lastSize;

            uint64_t allGlyphSquare = 0;
            for (const auto& i : glyphRectangles)
                allGlyphSquare += static_cast<uint64_t>(i.width) * i.height;

            for (size_t i = 0; i < textureSizeList.size(); ++i)
            {
                const auto& ss = textureSizeList[i];
                if (ss.w + spacing_hor <= borderX * 2)
                    continue;
                if (ss.h + spacing_ver <= borderY * 2)
                    continue;

                const auto workAreaW = ss.w + spacing_hor - borderX * 2;
                const auto workAreaH = ss.h + spacing_ver - borderY * 2;

                uint64_t textureSquare = static_cast<uint64_t>(workAreaW) * workAreaH;
                if (textureSquare < allGlyphSquare && i + 1 < textureSizeList.size())
                    continue;

                lastSize = ss;
                glyphRectangles = glyphRectanglesCopy;

                mrbp.Init(workAreaW, workAreaH, false);
                mrbp.Insert(glyphRectangles, arrangedRectangles, rbp::MaxRectsBinPack::RectBestAreaFit);

                if (glyphRectangles.empty())
                    break;
            }

            if (arrangedRectangles.empty())
            {
                if (!glyphRectangles.empty())
                    throw std::runtime_error("can not fit glyphs into texture");
                break;
            }

            std::uint32_t maxX = 0;
            std::uint32_t maxY = 0;
            for (const auto& r : arrangedRectangles)
            {
                std::uint32_t x = r.x + borderX;
                std::uint32_t y = r.y + borderY;

                j["rects"][r.tag]["x"] = x;
                j["rects"][r.tag]["y"] = y;
                j["rects"][r.tag]["p"] = textures.size();

                uint32_t right = x + r.width - spacing_hor + borderX;
                uint32_t bottom = y + r.height - spacing_ver + borderY;

                if (maxX < right)
                    maxX = right;
                if (maxY < bottom)
                    maxY = bottom;
            }
            if (cropX)
                lastSize.w = maxX;
            if (cropY)
                lastSize.h = maxY;

            textures.push_back(lastSize);
        }

        j["pages"] = nlohmann::json::array();
        for (const auto& i : textures)
            j["pages"].push_back({ {"w", i.w}, {"h", i.h}});

        std::ofstream of(outputFile);
        of << std::setw(4) << j;
    }
    catch (std::exception& e)
    {
        std::cout << "exception " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
