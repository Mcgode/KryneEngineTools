/**
 * @file
 * @author Max Godefroy
 * @date 26/03/2026.
 */

#pragma once

#include <KryneEngine/Modules/TextRendering/FontFiles/PreBakedFontFile.hpp>
#include "../../../../../Include/ProjectManager/AssetCooker/IAssetPipeline.hpp"


struct FT_LibraryRec_;

namespace ProjectManager
{
    class FontAssetPipeline final: public IAssetPipeline
    {
    public:
        FontAssetPipeline();
        ~FontAssetPipeline() override;

        [[nodiscard]] eastl::string_view GetName() const override
        {
            return "FontAssetPipeline";
        }

        [[nodiscard]] CookResult CookAsset(
            AssetCooker* _cooker,
            void* _entry,
            eastl::string_view _assetRelativePath,
            eastl::string_view _assetDirectory,
            eastl::string_view _outputDir) override;

        [[nodiscard]] KryneEngine::u64 GetVersion() const override
        {
            return 1;
        }

        [[nodiscard]] eastl::span<const char*> GetHandledAssetFileExtensions() const override
        {
            static const char* extensions[] = { ".ttf", ".otf" };
            return extensions;
        }

        using BakedRenderInfo = KryneEngine::Modules::TextRendering::PreBakedFontFile::BakedRenderInfo;

        void SetRenderInfo(const BakedRenderInfo _renderInfo) { m_renderInfo = _renderInfo; }
        void SetCompress(const bool _compress) { m_compress = _compress; }
        void SetFontSize(const KryneEngine::u16 _fontSize) { m_bakeFontSize = _fontSize; }

        [[nodiscard]] BakedRenderInfo GetRenderInfo() const { return m_renderInfo; }
        [[nodiscard]] bool IsCompress() const { return m_compress; }
        [[nodiscard]] KryneEngine::u16 GetFontSize() const { return m_bakeFontSize; }


    private:
        FT_LibraryRec_* m_ftLibrary = nullptr;
        BakedRenderInfo m_renderInfo = static_cast<BakedRenderInfo>(0);
        KryneEngine::u16 m_bakeFontSize = 48;
        bool m_compress = false;
    };
}
