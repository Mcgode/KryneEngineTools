/**
 * @file
 * @author Max Godefroy
 * @date 26/03/2026.
 */

#include "ProjectManager/FontAssetPipeline/FontAssetPipeline.hpp"

#include <KryneEngine/Modules/TextRendering/Utils/FreetypeFunctionHelpers.hpp>

#include "EASTL/sort.h"
#include "ProjectManager/AssetCooker/AssetCooker.hpp"
#include "ProjectManager/Logger/Logger.hpp"


using namespace KryneEngine;
using namespace KryneEngine::Modules::TextRendering;

namespace ProjectManager
{
    FontAssetPipeline::FontAssetPipeline()
    {
        const FT_Error error = FT_Init_FreeType(&m_ftLibrary);
        KE_ASSERT_MSG(error == FT_Err_Ok, FT_Error_String(error));
    }

    FontAssetPipeline::~FontAssetPipeline()
    {
        const FT_Error error = FT_Done_FreeType(m_ftLibrary);
        KE_ASSERT_MSG(error == FT_Err_Ok, FT_Error_String(error));
    }

    IAssetPipeline::CookResult FontAssetPipeline::CookAsset(
        const eastl::string_view _assetRelativePath,
        const eastl::string_view _assetDirectory,
        const eastl::string_view _outputDir)
    {
        const auto input = std::filesystem::path(_assetDirectory.data()) / _assetRelativePath.data();
        auto output = std::filesystem::path(_outputDir.data()) / _assetRelativePath.data();
        output.replace_extension(".ke_pbf");

        FT_Face face;
        {
            const FT_Error error = FT_New_Face(m_ftLibrary, input.c_str(), 0, &face);
            if (error != FT_Err_Ok)
            {
                Logger::GetInstance()->LogFormatted(LogSeverity::Error, AssetCooker::kLogCategory,
                    "[FontAssetPipline] [Freetype] Failed to load file '%s': %s",
                    _assetRelativePath.data(),
                    FT_Error_String(error));
                return CookResult { .success = false };
            }
        }

        if ((face->face_flags & FT_FACE_FLAG_SCALABLE) == 0)
        {
            Logger::GetInstance()->Log(LogSeverity::Error, AssetCooker::kLogCategory,
                "[FontAssetPipeline] The API only supports scalable/vector fonts");
            KE_VERIFY(FT_Done_Face(face) == FT_Err_Ok);
            return CookResult { .success = false };
        }

        // Select best charmap
        {
            const s32 bestCharMap = Freetype::SelectBestUnicodeCharmap(face);

            if (bestCharMap < 0) [[unlikely]]
            {
                Logger::GetInstance()->LogFormatted(LogSeverity::Error, AssetCooker::kLogCategory,
                    "[FontAssetPipeline] No available unicode char map for '%s'", _assetRelativePath.data());
                KE_VERIFY(FT_Done_Face(face) == FT_Err_Ok);
                return { .success = false };
            }
            const FT_Error error = FT_Set_Charmap(face, face->charmaps[bestCharMap]);
            if (error != FT_Err_Ok) [[unlikely]]
            {
                Logger::GetInstance()->LogFormatted(LogSeverity::Error, AssetCooker::kLogCategory,
                    "[FontAssetPipeline] Failed to set charmap for '%s': %s", _assetRelativePath.data(), FT_Error_String(error));
                KE_VERIFY(FT_Done_Face(face) == FT_Err_Ok);
                return { .success = false };
            }
        }

        eastl::vector<PreBakedFontFile::GlyphBakeInfo> glyphs {};
        eastl::vector<float2> points;
        eastl::vector<OutlineTag> tags;
        {
            u32 glyphIndex;
            u32 unicodeCodepoint = FT_Get_First_Char(face, &glyphIndex);
            while (glyphIndex != 0)
            {
                const FT_Error error = FT_Load_Glyph(face, glyphIndex, FT_LOAD_NO_BITMAP);
                if (error != FT_Err_Ok) [[unlikely]]
                {
                    Logger::GetInstance()->LogFormatted(LogSeverity::Error, AssetCooker::kLogCategory,
                        "[FontAssetPipeline] Failed to load glyph (%d, %d) for '%s': %s",
                        glyphIndex,
                        unicodeCodepoint,
                        _assetRelativePath.data(),
                        FT_Error_String(error));
                    KE_VERIFY(FT_Done_Face(face) == FT_Err_Ok);
                    return { .success = false };
                }

                PreBakedFontFile::GlyphBakeInfo& glyphInfo = glyphs.emplace_back();
                const float scale = 1.0f / static_cast<float>(face->units_per_EM);
                glyphInfo.m_glyph = {
                    .m_codePoint = unicodeCodepoint,
                    .m_advanceX = static_cast<float>(face->glyph->metrics.horiAdvance) * scale,
                    .m_bearingX = static_cast<float>(face->glyph->metrics.horiBearingX) * scale,
                    .m_bearingY = static_cast<float>(face->glyph->metrics.horiBearingY) * scale,
                    .m_width = static_cast<float>(face->glyph->metrics.width) * scale,
                    .m_height = static_cast<float>(face->glyph->metrics.height) * scale,
                };

                if (BitUtils::EnumHasAny(m_renderInfo, BakedRenderInfo::Outlines))
                {
                    glyphInfo.m_outlineStartPoint = points.size();
                    glyphInfo.m_outlineFirstTag = tags.size();

                    Freetype::LoadOutline(face, points, tags);

                    glyphInfo.m_outlineTagCount = points.size() - glyphInfo.m_outlineFirstTag;
                }

                unicodeCodepoint = FT_Get_Next_Char(face, unicodeCodepoint, &glyphIndex);
            }
        }
        eastl::sort(glyphs.begin(), glyphs.end(), [](const auto& _a, const auto& _b) { return _a.m_glyph.m_codePoint < _b.m_glyph.m_codePoint; });

        const eastl::span file = m_compress
            ? PreBakedFontFile::BakeCompressed(m_renderInfo, {}, glyphs, points, tags, {})
            : PreBakedFontFile::Bake(m_renderInfo, {}, glyphs, points, tags, {});

        if (std::filesystem::exists(output))
            std::filesystem::remove(output);

        if (!std::filesystem::exists(output.parent_path()))
            std::filesystem::create_directories(output.parent_path());

        std::ofstream outputFileStream(output, std::ios::binary | std::ios::out);
        outputFileStream.write(reinterpret_cast<const char*>(file.data()), static_cast<std::streamsize>(file.size()));
        outputFileStream.close();

        AllocatorInstance().deallocate(file.data(), file.size());

        return {
            .success = true,
            .m_resultingFiles = { output },
        };
    }
} // ProjectManager