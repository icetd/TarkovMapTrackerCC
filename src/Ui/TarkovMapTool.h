#ifndef TARKOV_MAP_TOOL_H
#define TARKOV_MAP_TOOL_H

#include "Core/Layer.h"
#include "Graphics/Renderer/Texture.h"
#include "Utils/INIReader.h"
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>

struct MapBounds {
    std::string name;
    float xmin, xmax, zmin, zmax;
    int imgWidth, imgHeight;
    float rotation;
    std::unique_ptr<Texture> texture;

    struct Extract {
        std::string name;
        float x, z;
        bool isShared;
    };
    std::vector<Extract> extracts;

    MapBounds() : rotation(0.0f) {}
    MapBounds(MapBounds &&other) noexcept
        : name(std::move(other.name)),
          xmin(other.xmin),
          xmax(other.xmax),
          zmin(other.zmin),
          zmax(other.zmax),
          imgWidth(other.imgWidth),
          imgHeight(other.imgHeight),
          rotation(other.rotation),
          texture(std::move(other.texture)),
          extracts(std::move(other.extracts))
    {}

    MapBounds &operator=(MapBounds &&other) noexcept
    {
        if (this != &other) {
            name = std::move(other.name);
            xmin = other.xmin;
            xmax = other.xmax;
            zmin = other.zmin;
            zmax = other.zmax;
            imgWidth = other.imgWidth;
            imgHeight = other.imgHeight;
            rotation = other.rotation;
            texture = std::move(other.texture);
            extracts = std::move(other.extracts);
        }
        return *this;
    }

    MapBounds(const MapBounds &) = delete;
    MapBounds &operator=(const MapBounds &) = delete;

    float GameToPixelX(float x) const;
    float GameToPixelZ(float z) const;
};

struct PlayerData {
    std::string time;
    float x, y, z;
    float qx, qy, qz, qw;
    float yaw;
    ImVec2 pixelPos;
};

class TarkovMapTool : public Layer
{
public:
    enum class Language
    {
        Chinese,
        English
    };

    TarkovMapTool();
    ~TarkovMapTool();

    void SetLanguage(Language lang);
    Language GetLanguage() const { return m_language; }

    void SetScreenshotsDir(const std::string &dir);
    void OpenDirectoryDialog();

protected:
    virtual void OnAttach() override;
    virtual void OnUpdate(float ts) override;
    virtual void OnDetach() override;

private:
    void ShowMapLayout();
    void OnRenderView();
    void ViewMap();
    void DrawPoint();
    void DrawExtracts();
    void DrawDirectionIndicator(ImVec2 center, float yaw);

    void LoadConfig();
    void SaveConfig();
    void LoadMapData(const std::string &jsonPath);
    void LoadLatestScreenshot();
    void UpdatePointForCurrentMap();

    bool ParseFilename(const std::string &filename, PlayerData &out);
    float QuaternionToYaw(float qx, float qy, float qz, float qw);
    void LoadTexture(MapBounds &map, const std::string &path);
    std::string GetLatestPngFile();
    void LoadExtractIcons();

    ImVec2 TransformWithRotation(const MapBounds &map, float x, float z, const ImVec2 &viewStart, const ImVec2 &viewSize) const;

    // 辅助函数：根据当前语言返回对应字符串
    const char *_(const char *ch, const char *en) const
    {
        return m_language == Language::Chinese ? ch : en;
    }

    enum class ExtractFilter
    {
        All,
        PMC,
        Shared
    };
    ExtractFilter m_extractFilter = ExtractFilter::All;

    float m_autoRefreshTimer = 0.0f;
    bool m_autoRefreshEnabled = false;
    float m_autoRefreshInterval = 5.0f;

    std::unique_ptr<INIReader> m_config;
    std::string m_configPath = "./configs/tarkov.ini";

    std::unordered_map<std::string, MapBounds> m_maps;
    std::string m_selectedMap;
    std::string m_screenshotsDir;
    std::string m_mapsDir;
    std::string m_iconsDir;
    std::string m_jsonPath;

    PlayerData m_currentPoint;
    bool m_hasPoint;
    bool m_showMapTool;

    ImVec2 m_viewStartPos;
    ImVec2 m_viewportSize;
    ImVec2 m_imageDisplayPos;
    ImVec2 m_imageDisplaySize;

    std::unique_ptr<Texture> m_iconPmcExtract;
    std::unique_ptr<Texture> m_iconSharedExtract;

    Language m_language = Language::Chinese;

    char m_screenshotsDirBuffer[512];
};

#endif