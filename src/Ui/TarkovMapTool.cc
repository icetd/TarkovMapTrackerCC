#include "TarkovMapTool.h"
#include "Core/log.h"
#include <regex>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <windows.h>
#include <shlobj.h>
#include <objbase.h>
#include <knownfolders.h>
#include <stb_image.h>
#include <iostream>
#include <nlohmann/json.hpp>

#define M_PI 3.14159265358979323846

using json = nlohmann::json;

std::string GetDefaultTarkovScreenshotsDir()
{
    std::string screenshotsDir;

    // 获取用户的文档目录
    PWSTR documentsPath = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &documentsPath))) {
        char documentsDir[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, documentsPath, -1, documentsDir, MAX_PATH, NULL, NULL);

        // 构建塔科夫截图目录路径
        screenshotsDir = std::string(documentsDir) + "\\Escape from Tarkov\\Screenshots";
        CoTaskMemFree(documentsPath);

        // 检查并创建目录
        DWORD ftyp = GetFileAttributesA(screenshotsDir.c_str());
        if (ftyp == INVALID_FILE_ATTRIBUTES || !(ftyp & FILE_ATTRIBUTE_DIRECTORY)) {
            CreateDirectoryA((std::string(documentsDir) + "\\Escape from Tarkov").c_str(), NULL);
            CreateDirectoryA(screenshotsDir.c_str(), NULL);
        }
    }

    return screenshotsDir;
}

float MapBounds::GameToPixelX(float x) const
{
    return (x - xmin) / (xmax - xmin) * imgWidth;
}

float MapBounds::GameToPixelZ(float z) const
{
    return (z - zmin) / (zmax - zmin) * imgHeight;
}

TarkovMapTool::TarkovMapTool() :
    m_selectedMap("shoreline"),
    m_screenshotsDir(GetDefaultTarkovScreenshotsDir()),
    m_mapsDir("./res/maps"),
    m_iconsDir("./res/icons"),
    m_jsonPath("./res/maps/maps.json"),
    m_hasPoint(false),
    m_showMapTool(true)
{
    memset(m_screenshotsDirBuffer, 0, sizeof(m_screenshotsDirBuffer));

    // 如果默认目录为空，使用备用目录
    if (m_screenshotsDir.empty()) {
        m_screenshotsDir = "./res/screenshots";
        CreateDirectoryA("./res/screenshots", NULL);
    }
}

TarkovMapTool::~TarkovMapTool()
{}

void TarkovMapTool::SetLanguage(Language lang)
{
    m_language = lang;
    LOG(INFO, "Language switched to %s", lang == Language::Chinese ? "Chinese" : "English");
    SaveConfig();
}

void TarkovMapTool::SetScreenshotsDir(const std::string &dir)
{
    if (dir.empty()) return;

    // 检查目录是否存在
    DWORD ftyp = GetFileAttributesA(dir.c_str());
    if (ftyp == INVALID_FILE_ATTRIBUTES || !(ftyp & FILE_ATTRIBUTE_DIRECTORY)) {
        LOG(WARN, "Directory does not exist: %s", dir.c_str());
        // 尝试创建目录
        if (CreateDirectoryA(dir.c_str(), NULL)) {
            LOG(INFO, "Created directory: %s", dir.c_str());
        } else {
            LOG(ERRO, "Failed to create directory: %s", dir.c_str());
            return;
        }
    }

    m_screenshotsDir = dir;

    SaveConfig();
    LoadLatestScreenshot();
}

void TarkovMapTool::OpenDirectoryDialog()
{
#ifdef _WIN32
    BROWSEINFOA bi = {0};
    bi.lpszTitle = "选择截图目录 / Select Screenshots Directory";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (pidl != 0) {
        char path[MAX_PATH];
        if (SHGetPathFromIDListA(pidl, path)) {
            SetScreenshotsDir(path);
            strncpy(m_screenshotsDirBuffer, path, sizeof(m_screenshotsDirBuffer) - 1);
            m_screenshotsDirBuffer[sizeof(m_screenshotsDirBuffer) - 1] = '\0';
        }
        CoTaskMemFree(pidl);
    }
#endif
}

void TarkovMapTool::SaveConfig()
{
    std::ofstream file(m_configPath);
    if (!file.is_open()) {
        LOG(ERRO, "Failed to save config: %s", m_configPath.c_str());
        return;
    }

    file << "[Paths]\n";
    file << "ScreenshotsDir=" << m_screenshotsDir << "\n";
    file << "MapsDir=" << m_mapsDir << "\n";
    file << "IconsDir=" << m_iconsDir << "\n";
    file << "MapDataJson=" << m_jsonPath << "\n\n";

    file << "[UI]\n";
    file << "SelectedMap=" << m_selectedMap << "\n";
    file << "ExtractFilter=" << (int)m_extractFilter << "\n";
    file << "Language=" << (m_language == Language::Chinese ? "Chinese" : "English") << "\n\n";

    file << "[AutoRefresh]\n";
    file << "Enabled=" << (m_autoRefreshEnabled ? "true" : "false") << "\n";
    file << "Interval=" << m_autoRefreshInterval << "\n";

    file.close();
    LOG(INFO, "Config saved to %s", m_configPath.c_str());
}

void TarkovMapTool::LoadConfig()
{
    CreateDirectoryA("./configs", NULL);
    m_config = std::make_unique<INIReader>(m_configPath);

    // 优先使用配置文件中的路径，如果没有则使用默认塔科夫截图目录
    std::string configScreenshotsDir = m_config->Get("Paths", "ScreenshotsDir", "");
    if (!configScreenshotsDir.empty()) {
        m_screenshotsDir = configScreenshotsDir;
    } else {
        m_screenshotsDir = GetDefaultTarkovScreenshotsDir();
        if (m_screenshotsDir.empty()) {
            m_screenshotsDir = "./res/screenshots";
        }
    }

    m_mapsDir = m_config->Get("Paths", "MapsDir", "./res/maps");
    m_iconsDir = m_config->Get("Paths", "IconsDir", "./res/icons");
    m_jsonPath = m_config->Get("Paths", "MapDataJson", "./res/maps/maps.json");
    m_selectedMap = m_config->Get("UI", "SelectedMap", "shoreline");
    m_autoRefreshEnabled = m_config->GetBoolean("AutoRefresh", "Enabled", false);
    m_autoRefreshInterval = (float)m_config->GetReal("AutoRefresh", "Interval", 5.0);
    m_extractFilter = (ExtractFilter)m_config->GetInteger("UI", "ExtractFilter", 0);

    std::string langStr = m_config->Get("UI", "Language", "Chinese");
    m_language = (langStr == "English") ? Language::English : Language::Chinese;

    // 初始化缓冲区
    strncpy(m_screenshotsDirBuffer, m_screenshotsDir.c_str(), sizeof(m_screenshotsDirBuffer) - 1);
    m_screenshotsDirBuffer[sizeof(m_screenshotsDirBuffer) - 1] = '\0';

    LOG(INFO, "Config loaded: ScreenshotsDir=%s, AutoRefresh=%d, Interval=%.1f, Language=%s",
        m_screenshotsDir.c_str(), m_autoRefreshEnabled, m_autoRefreshInterval,
        m_language == Language::Chinese ? "Chinese" : "English");
}

void TarkovMapTool::OnAttach()
{
    LOG(INFO, "TarkovMapTool::OnAttach");
    LoadConfig();
    LoadExtractIcons();
    LoadMapData(m_jsonPath);
    for (auto &pair : m_maps) {
        std::string path = m_mapsDir + "/" + pair.second.name + ".png";
        LoadTexture(pair.second, path);
    }
    LoadLatestScreenshot();
}

void TarkovMapTool::OnUpdate(float ts)
{
    if (m_showMapTool) {
        if (m_autoRefreshEnabled) {
            m_autoRefreshTimer += ts;
            if (m_autoRefreshTimer >= m_autoRefreshInterval) {
                m_autoRefreshTimer = 0.0f;
                LoadLatestScreenshot();
            }
        }
        ShowMapLayout();
    }
}

void TarkovMapTool::OnDetach()
{
    SaveConfig();
}

void TarkovMapTool::LoadExtractIcons()
{
    std::string pmcIconPath = m_iconsDir + "/extract-pmc.png";
    if (GetFileAttributesA(pmcIconPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        m_iconPmcExtract = std::make_unique<Texture>(pmcIconPath, "icon", false);
        LOG(INFO, "Loaded PMC extract icon");
    } else {
        LOG(WARN, "PMC extract icon not found: %s", pmcIconPath.c_str());
    }
    std::string sharedIconPath = m_iconsDir + "/extract-shared.png";
    if (GetFileAttributesA(sharedIconPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        m_iconSharedExtract = std::make_unique<Texture>(sharedIconPath, "icon", false);
        LOG(INFO, "Loaded Shared extract icon");
    } else {
        LOG(WARN, "Shared extract icon not found: %s", sharedIconPath.c_str());
    }
}

void TarkovMapTool::LoadMapData(const std::string &jsonPath)
{
    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        LOG(ERRO, "Failed to open map data file: %s", jsonPath.c_str());
        return;
    }
    json data;
    file >> data;
    for (auto it = data.begin(); it != data.end(); ++it) {
        const std::string &key = it.key();
        json mapData = it.value();
        MapBounds map;
        map.name = mapData["name"].get<std::string>();
        map.xmin = mapData["xmin"].get<float>();
        map.xmax = mapData["xmax"].get<float>();
        map.zmin = mapData["zmin"].get<float>();
        map.zmax = mapData["zmax"].get<float>();
        map.imgWidth = 0;
        map.imgHeight = 0;
        map.texture = nullptr;
        map.rotation = 180.0f;
        if (map.name == "labs") {
            map.rotation = 90.0f;
        } else if (map.name == "factory") {
            map.rotation = -90.0f;
        }
        if (mapData.find("pmc_extracts") != mapData.end()) {
            for (auto &extract : mapData["pmc_extracts"]) {
                MapBounds::Extract e;
                e.name = extract["name"].get<std::string>();
                e.x = extract["location"][0].get<float>();
                e.z = extract["location"][1].get<float>();
                e.isShared = false;
                map.extracts.push_back(e);
            }
        }
        if (mapData.find("shared_extracts") != mapData.end()) {
            for (auto &extract : mapData["shared_extracts"]) {
                MapBounds::Extract e;
                e.name = extract["name"].get<std::string>();
                e.x = extract["location"][0].get<float>();
                e.z = extract["location"][1].get<float>();
                e.isShared = true;
                map.extracts.push_back(e);
            }
        }
        m_maps.emplace(map.name, std::move(map));
        LOG(INFO, "Loaded map: %s with %zu extracts (rotation=%.0f)",
            map.name.c_str(), map.extracts.size(), map.rotation);
    }
}

std::string TarkovMapTool::GetLatestPngFile()
{
    std::string latestFile;
    FILETIME latestTime = {0};
    std::string searchPath = m_screenshotsDir + "\\*.png";
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        LOG(WARN, "No PNG files found in %s", m_screenshotsDir.c_str());
        return "";
    }
    do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            if (CompareFileTime(&findData.ftLastWriteTime, &latestTime) > 0) {
                latestTime = findData.ftLastWriteTime;
                latestFile = findData.cFileName;
            }
        }
    } while (FindNextFileA(hFind, &findData));
    FindClose(hFind);
    if (!latestFile.empty()) {
        LOG(INFO, "Latest screenshot: %s", latestFile.c_str());
    }
    return latestFile;
}

void TarkovMapTool::LoadLatestScreenshot()
{
    m_hasPoint = false;
    std::string latestFile = GetLatestPngFile();
    if (latestFile.empty()) return;
    if (ParseFilename(latestFile, m_currentPoint)) {
        m_hasPoint = true;
        UpdatePointForCurrentMap();
        LOG(INFO, "Loaded point: (%.2f, %.2f, %.2f) yaw=%.1f",
            m_currentPoint.x, m_currentPoint.y, m_currentPoint.z, m_currentPoint.yaw);
    }
}

void TarkovMapTool::UpdatePointForCurrentMap()
{
    if (!m_hasPoint) return;
    auto it = m_maps.find(m_selectedMap);
    if (it == m_maps.end()) return;
    MapBounds &currentMap = it->second;
    m_currentPoint.pixelPos = ImVec2(
        currentMap.GameToPixelX(m_currentPoint.x),
        currentMap.GameToPixelZ(m_currentPoint.z));
}

float TarkovMapTool::QuaternionToYaw(float qx, float qy, float qz, float qw)
{
    float siny = 2.0f * (qw * qy + qx * qz);
    float cosy = 1.0f - 2.0f * (qy * qy + qz * qz);
    float yaw = atan2f(siny, cosy);
    return yaw * 180.0f / M_PI + 180.0f;
}

bool TarkovMapTool::ParseFilename(const std::string &filename, PlayerData &out)
{
    std::regex pattern(R"(\[(\d{2}-\d{2})\]_(-?\d+\.?\d*),\s*(-?\d+\.?\d*),\s*(-?\d+\.?\d*)_(-?\d+\.?\d*),\s*(-?\d+\.?\d*),\s*(-?\d+\.?\d*),\s*(-?\d+\.?\d*))");
    std::smatch match;
    if (std::regex_search(filename, match, pattern)) {
        out.time = match[1].str();
        out.x = std::stof(match[2].str());
        out.y = std::stof(match[3].str());
        out.z = std::stof(match[4].str());
        out.qx = std::stof(match[5].str());
        out.qy = std::stof(match[6].str());
        out.qz = std::stof(match[7].str());
        out.qw = std::stof(match[8].str());
        out.yaw = QuaternionToYaw(out.qx, out.qy, out.qz, out.qw);
        return true;
    }
    return false;
}

void TarkovMapTool::LoadTexture(MapBounds &map, const std::string &path)
{
    map.texture = std::make_unique<Texture>(path, "map", false);
    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char *data = stbi_load(path.c_str(), &width, &height, &channels, 0);
    if (data) {
        map.imgWidth = width;
        map.imgHeight = height;
        stbi_image_free(data);
        LOG(INFO, "Loaded texture: %s (%dx%d) rotation=%.0f", map.name.c_str(), width, height, map.rotation);
    } else {
        LOG(ERRO, "Failed to load texture: %s", path.c_str());
    }
}

ImVec2 TarkovMapTool::TransformWithRotation(const MapBounds &map, float x, float z, const ImVec2 &viewStart, const ImVec2 &viewSize) const
{
    float pixelX = map.GameToPixelX(x);
    float pixelY = map.GameToPixelZ(z);
    float u = pixelX / map.imgWidth;
    float v = pixelY / map.imgHeight;
    float rotatedU = u, rotatedV = v;
    if (map.rotation == 90.0f) {
        rotatedU = v;
        rotatedV = 1.0f - u;
    } else if (map.rotation == 180.0f) {
        rotatedU = 1.0f - u;
        rotatedV = 1.0f - v;
    } else if (map.rotation == -90.0f) {
        rotatedU = 1.0f - v;
        rotatedV = u;
    }
    float screenX = viewStart.x + m_imageDisplayPos.x + rotatedU * m_imageDisplaySize.x;
    float screenY = viewStart.y + m_imageDisplayPos.y + m_imageDisplaySize.y - (rotatedV * m_imageDisplaySize.y);
    return ImVec2(screenX, screenY);
}

void TarkovMapTool::ShowMapLayout()
{
    ImGui::Begin(_(u8"控制", "Controls"), &m_showMapTool);

    ImGui::TextUnformatted(u8"语言 / Language");
    ImGui::Separator();

    bool isCN = (m_language == Language::Chinese);
    if (isCN) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
    }
    if (ImGui::Button(u8"中文")) {
        SetLanguage(Language::Chinese);
    }
    if (isCN) ImGui::PopStyleColor();

    ImGui::SameLine();

    bool isEN = (m_language == Language::English);
    if (isEN) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
    }
    if (ImGui::Button("English")) {
        SetLanguage(Language::English);
    }
    if (isEN) ImGui::PopStyleColor();

    ImGui::Separator();

    ImGui::Text(_(u8"撤离点显示:", "Extract Filter:"));
    ImGui::RadioButton(_(u8"全部", "All"), (int *)&m_extractFilter, (int)ExtractFilter::All);
    ImGui::SameLine();
    ImGui::RadioButton(_(u8"PMC", "PMC"), (int *)&m_extractFilter, (int)ExtractFilter::PMC);
    ImGui::SameLine();
    ImGui::RadioButton(_(u8"合作撤离", "Shared"), (int *)&m_extractFilter, (int)ExtractFilter::Shared);
    ImGui::Separator();

    // ========== 截图目录设置区域 ==========
    ImGui::Text(_(u8"截图目录:", "Screenshots Dir:"));

    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", m_screenshotsDir.c_str());
    if (ImGui::Button(_(u8"浏览...", "Browse..."))) {
        OpenDirectoryDialog();
    }

    ImGui::SameLine();
    if (ImGui::Button(_(u8"塔科夫默认", "Tarkov Default"))) {
        std::string defaultDir = GetDefaultTarkovScreenshotsDir();
        if (!defaultDir.empty()) {
            SetScreenshotsDir(defaultDir);
            strncpy(m_screenshotsDirBuffer, defaultDir.c_str(), sizeof(m_screenshotsDirBuffer) - 1);
            m_screenshotsDirBuffer[sizeof(m_screenshotsDirBuffer) - 1] = '\0';
        }
    }

    ImGui::PushID("screenshots_dir_input");
    ImGui::SetNextItemWidth(300.0f);
    if (ImGui::InputText(_(u8"##ScreenshotsDir", "##ScreenshotsDir"),
                         m_screenshotsDirBuffer, sizeof(m_screenshotsDirBuffer))) {
    }

    if (ImGui::Button(_(u8"应用", "Apply"))) {
        SetScreenshotsDir(m_screenshotsDirBuffer);
    }

    // 重置默认按钮
    ImGui::SameLine();
    if (ImGui::Button(_(u8"重置默认", "Reset Default"))) {
        SetScreenshotsDir("./res/screenshots");
        strncpy(m_screenshotsDirBuffer, "./res/screenshots", sizeof(m_screenshotsDirBuffer) - 1);
        m_screenshotsDirBuffer[sizeof(m_screenshotsDirBuffer) - 1] = '\0';
    }
    ImGui::PopID();
    // ============================================

    ImGui::Text(_(u8"配置文件:", "Config:"));
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", m_configPath.c_str());
    ImGui::Separator();

    ImGui::Text(_(u8"自动刷新:", "Auto Refresh:"));
    ImGui::SameLine();
    ImGui::Checkbox(_(u8"启用", "Enable"), &m_autoRefreshEnabled);
    if (m_autoRefreshEnabled) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.0f);
        ImGui::SliderFloat(_(u8"间隔(秒)", "Interval(s)"), &m_autoRefreshInterval, 1.0f, 30.0f, "%.1f");
    }
    ImGui::Separator();

    if (m_hasPoint) {
        ImGui::Text(_(u8"最新截图信息", "Latest Screenshot Info"));
        ImGui::Separator();
        ImGui::Text(_(u8"时间:", "Time:"));
        ImGui::SameLine();
        ImGui::Text("%s", m_currentPoint.time.c_str());
        ImGui::Text(_(u8"地图:", "Map:"));
        ImGui::SameLine();
        ImGui::Text("%s", m_selectedMap.c_str());
        ImGui::Separator();
        ImGui::Text(_(u8"游戏坐标:", "Game Position:"));
        ImGui::Text(_(u8"  X: %.2f", "  X: %.2f"), m_currentPoint.x);
        ImGui::Text(_(u8"  Y: %.2f", "  Y: %.2f"), m_currentPoint.y);
        ImGui::Text(_(u8"  Z: %.2f", "  Z: %.2f"), m_currentPoint.z);
        ImGui::Separator();
        ImGui::Text(_(u8"朝向: %.1f°", u8"Yaw: %.1f°"), m_currentPoint.yaw);

        // 计算显示用的朝向
        float displayYaw = m_currentPoint.yaw;
        auto it = m_maps.find(m_selectedMap);
        if (it != m_maps.end()) {
            if (it->second.rotation == 90.0f) {
                displayYaw -= 90.0f;
            } else if (it->second.rotation == -90.0f) {
                displayYaw += 90.0f;
            }
        }

        ImVec2 center = ImGui::GetCursorScreenPos();
        center.x += 70;
        center.y += 70;
        DrawDirectionIndicator(center, displayYaw);
        ImGui::Dummy(ImVec2(140, 140));
        ImGui::Separator();

        ImGui::Text(_(u8"切换地图:", "Switch Map:"));
        if (ImGui::BeginCombo(_(u8"##MapSelect", "##MapSelect"), m_selectedMap.c_str())) {
            for (auto &pair : m_maps) {
                if (ImGui::Selectable(pair.second.name.c_str(), m_selectedMap == pair.second.name)) {
                    m_selectedMap = pair.second.name;
                    UpdatePointForCurrentMap();
                    SaveConfig();
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::Button(_(u8"手动刷新截图", "Refresh Screenshot"), ImVec2(-1, 30))) {
            LoadLatestScreenshot();
        }
        if (ImGui::Button(_(u8"重载配置文件", "Reload Config"), ImVec2(-1, 30))) {
            LoadConfig();
            LoadLatestScreenshot();
        }
        if (m_autoRefreshEnabled) {
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f),
                               _(u8"自动刷新中 (%.1f秒)", "Auto refreshing (%.1fs)"), m_autoRefreshInterval);
        }
    } else {
        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), _(u8"未找到截图文件", "No screenshot found"));
        ImGui::Text("");
        ImGui::Text(_(u8"请将截图放入:", "Place screenshots in:"));
        ImGui::Text("  %s", m_screenshotsDir.c_str());
        ImGui::Text("");
        ImGui::Text(_(u8"文件名格式:", "Filename format:"));
        ImGui::Text(_(u8"  [HH-MM]_X, Y, Z_qx, qy, qz, qw_...", "  [HH-MM]_X, Y, Z_qx, qy, qz, qw_..."));
        if (ImGui::Button(_(u8"重载配置文件", "Reload Config"), ImVec2(-1, 30))) {
            LoadConfig();
            LoadLatestScreenshot();
        }
    }
    ImGui::End();

    ImGui::Begin("view");
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
    ImGui::BeginChild(_(u8"##MapView", "##MapView"), ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    OnRenderView();
    ImGui::EndChild();
    ImGui::End();
}

void TarkovMapTool::OnRenderView()
{
    m_viewStartPos = ImGui::GetCursorScreenPos();
    m_viewportSize = ImGui::GetContentRegionAvail();
    ViewMap();
    DrawExtracts();
    DrawPoint();
}

void TarkovMapTool::ViewMap()
{
    auto it = m_maps.find(m_selectedMap);
    if (it == m_maps.end()) return;
    MapBounds &currentMap = it->second;
    if (currentMap.texture && currentMap.texture->getID() > 0) {
        float imgAspect = (float)currentMap.imgWidth / currentMap.imgHeight;
        float viewAspect = m_viewportSize.x / m_viewportSize.y;
        if (imgAspect > viewAspect) {
            m_imageDisplaySize.x = m_viewportSize.x;
            m_imageDisplaySize.y = m_viewportSize.x / imgAspect;
        } else {
            m_imageDisplaySize.y = m_viewportSize.y;
            m_imageDisplaySize.x = m_viewportSize.y * imgAspect;
        }
        m_imageDisplayPos.x = (m_viewportSize.x - m_imageDisplaySize.x) * 0.5f;
        m_imageDisplayPos.y = (m_viewportSize.y - m_imageDisplaySize.y) * 0.5f;
        ImGui::SetCursorPosX(m_imageDisplayPos.x);
        ImGui::SetCursorPosY(m_imageDisplayPos.y);
        ImGui::Image((ImTextureID)(intptr_t)currentMap.texture->getID(), m_imageDisplaySize, ImVec2(0, 1), ImVec2(1, 0));
    } else {
        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), _(u8"未加载地图纹理", "Map texture not loaded"));
        ImGui::Text(_(u8"路径: %s/%s.png", "Path: %s/%s.png"), m_mapsDir.c_str(), m_selectedMap.c_str());
    }
}

void TarkovMapTool::DrawExtracts()
{
    auto it = m_maps.find(m_selectedMap);
    if (it == m_maps.end()) return;
    MapBounds &currentMap = it->second;
    if (currentMap.imgWidth == 0 || currentMap.imgHeight == 0) return;
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    for (const auto &extract : currentMap.extracts) {
        if (m_extractFilter == ExtractFilter::PMC && extract.isShared) continue;
        if (m_extractFilter == ExtractFilter::Shared && !extract.isShared) continue;
        ImVec2 screenPos = TransformWithRotation(currentMap, extract.x, extract.z, m_viewStartPos, m_viewportSize);
        Texture *icon = extract.isShared ? m_iconSharedExtract.get() : m_iconPmcExtract.get();
        float iconSize = 20.0f;
        if (icon && icon->getID() > 0) {
            ImVec2 oldCursorPos = ImGui::GetCursorScreenPos();
            ImGui::SetCursorScreenPos(ImVec2(screenPos.x - iconSize / 2, screenPos.y - iconSize / 2));
            ImGui::Image((ImTextureID)(intptr_t)icon->getID(), ImVec2(iconSize, iconSize), ImVec2(0, 1), ImVec2(1, 0));
            ImGui::SetCursorScreenPos(oldCursorPos);
        } else {
            drawList->AddCircleFilled(screenPos, 6.0f,
                                      extract.isShared ? IM_COL32(100, 100, 255, 255) : IM_COL32(255, 200, 100, 255), 12);
            drawList->AddCircle(screenPos, 6.0f, IM_COL32(255, 255, 255, 255), 12, 1.5f);
        }
        ImVec2 textSize = ImGui::CalcTextSize(extract.name.c_str());
        ImVec2 textPos(screenPos.x - textSize.x / 2, screenPos.y + iconSize / 2 + 2);
        drawList->AddRectFilled(ImVec2(textPos.x - 2, textPos.y - 1),
                                ImVec2(textPos.x + textSize.x + 2, textPos.y + textSize.y + 1),
                                IM_COL32(0, 0, 0, 180), 3.0f);
        ImU32 textColor = extract.isShared ? IM_COL32(100, 200, 255, 255) : IM_COL32(255, 200, 100, 255);
        drawList->AddText(textPos, textColor, extract.name.c_str());
    }
}

void TarkovMapTool::DrawPoint()
{
    if (!m_hasPoint) return;
    auto it = m_maps.find(m_selectedMap);
    if (it == m_maps.end()) return;
    MapBounds &currentMap = it->second;
    if (currentMap.imgWidth == 0 || currentMap.imgHeight == 0) return;
    ImVec2 screenPos = TransformWithRotation(currentMap, m_currentPoint.x, m_currentPoint.z, m_viewStartPos, m_viewportSize);
    float adjustedYaw = m_currentPoint.yaw;
    if (currentMap.rotation == 90.0f) {
        adjustedYaw -= 90.0f;
    } else if (currentMap.rotation == -90.0f) {
        adjustedYaw += 90.0f;
    }
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    float radius = 8.0f;
    drawList->AddCircleFilled(screenPos, radius, IM_COL32(255, 0, 0, 255), 12);
    drawList->AddCircle(screenPos, radius, IM_COL32(255, 255, 255, 255), 12, 2);
    float rad = adjustedYaw * M_PI / 180.0f;
    float length = 40.0f;
    float endX = screenPos.x + length * sinf(rad);
    float endY = screenPos.y - length * cosf(rad);
    drawList->AddLine(screenPos, ImVec2(endX, endY), IM_COL32(0, 255, 0, 255), 3.0f);
    drawList->AddText(ImVec2(screenPos.x + 14, screenPos.y - 12), IM_COL32(255, 255, 0, 255), m_currentPoint.time.c_str());
}

void TarkovMapTool::DrawDirectionIndicator(ImVec2 center, float yaw)
{
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    float radius = 50.0f;
    drawList->AddCircle(center, radius, IM_COL32(255, 255, 255, 200), 0, 2.0f);
    drawList->AddCircleFilled(center, radius, IM_COL32(0, 0, 0, 180), 0);
    for (int i = 0; i < 360; i += 10) {
        float rad = i * M_PI / 180.0f;
        float startRadius = (i % 30 == 0) ? radius - 10 : radius - 6;
        float endRadius = radius - 2;
        ImVec2 start(center.x + startRadius * cosf(rad), center.y + startRadius * sinf(rad));
        ImVec2 end(center.x + endRadius * cosf(rad), center.y + endRadius * sinf(rad));
        if (i % 90 == 0) {
            drawList->AddLine(start, end, IM_COL32(255, 200, 100, 255), 2.0f);
        } else if (i % 30 == 0) {
            drawList->AddLine(start, end, IM_COL32(200, 200, 200, 200), 2.0f);
        } else {
            drawList->AddLine(start, end, IM_COL32(150, 150, 150, 150), 1.0f);
        }
    }
    auto drawDirectionText = [&](const char *text, float angle, ImU32 color) {
        float rad = angle * M_PI / 180.0f;
        float textRadius = radius - 18;
        float x = center.x + textRadius * cosf(rad);
        float y = center.y + textRadius * sinf(rad);
        ImVec2 textSize = ImGui::CalcTextSize(text);
        drawList->AddText(ImVec2(x - textSize.x * 0.5f, y - textSize.y * 0.5f), color, text);
    };
    drawDirectionText("N", -90, IM_COL32(255, 100, 100, 255));
    drawDirectionText("S", 90, IM_COL32(100, 255, 100, 255));
    drawDirectionText("E", 0, IM_COL32(100, 150, 255, 255));
    drawDirectionText("W", 180, IM_COL32(255, 200, 100, 255));
    float rad = (yaw - 90) * M_PI / 180.0f;
    float arrowLength = radius - 8;
    float arrowX = center.x + arrowLength * cosf(rad);
    float arrowY = center.y + arrowLength * sinf(rad);
    drawList->AddLine(center, ImVec2(arrowX, arrowY), IM_COL32(255, 0, 0, 255), 3.0f);
    float arrowAngle = 25.0f * M_PI / 180.0f;
    float tipRadius = 12.0f;
    ImVec2 tip1(
        arrowX + tipRadius * cosf(rad + M_PI - arrowAngle),
        arrowY + tipRadius * sinf(rad + M_PI - arrowAngle));
    ImVec2 tip2(
        arrowX + tipRadius * cosf(rad + M_PI + arrowAngle),
        arrowY + tipRadius * sinf(rad + M_PI + arrowAngle));
    drawList->AddTriangleFilled(ImVec2(arrowX, arrowY), tip1, tip2, IM_COL32(255, 0, 0, 255));
    drawList->AddCircleFilled(center, 6.0f, IM_COL32(255, 255, 255, 255), 0);
    drawList->AddCircleFilled(center, 3.0f, IM_COL32(0, 0, 0, 255), 0);
    char angleStr[32];
    snprintf(angleStr, sizeof(angleStr), u8"%.0f°", yaw);
    ImVec2 textSize = ImGui::CalcTextSize(angleStr);
    drawList->AddText(ImVec2(center.x - textSize.x * 0.5f, center.y + radius + 5),
                      IM_COL32(255, 255, 0, 255), angleStr);
}