#include <Windows.h>
#include <stb_image.h>
#include <imgui.h>
#include <iostream>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include <glm/glm.hpp>
#include "Application.h"
#include "../Utils/StyleManager.h"
#include "log.h"

static Application *Instance = nullptr;

static void glfw_error_callback(int error, const char *description)
{
    LOG(ERRO, "GLFW Error %d:%s", error, description);
}

Application::Application(const char *appName, int w, int h) :
    m_AppName(appName),
    m_Width(w),
    m_Height(h),
    m_WindowHandler(nullptr)
{
    Instance = this;
    Init();
}

Application::~Application()
{
    Shutdown();
}

void Application::Init()
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        LOG(ERRO, "Could not Initialie GLFW!");
        return;
    }

    const char *glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE); // 启用双缓冲
    m_WindowHandler = glfwCreateWindow(m_Width, m_Height, m_AppName.c_str(), NULL, NULL);

    GLFWimage icon;
    icon.width = 512;
    icon.height = 512;
    int channels = 4;
    icon.pixels = stbi_load("./res/icons/map.png", &icon.width, &icon.height, &channels, 4);
    glfwSetWindowIcon(m_WindowHandler, 1, &icon);
    stbi_image_free(icon.pixels);

    glfwSetCursorPos(m_WindowHandler, (m_Width / 2), (m_Height / 2));

    glfwSetWindowUserPointer(m_WindowHandler, this);
    glfwSetCursorPosCallback(m_WindowHandler, mouse_callback_static);
    glfwSetScrollCallback(m_WindowHandler, scroll_callback_static);
    glfwSetWindowSizeCallback(m_WindowHandler, window_size_callback_static);

    glfwMakeContextCurrent(m_WindowHandler);
    glfwSetWindowUserPointer(m_WindowHandler, this);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        LOG(ERRO, "Failed to initialize GLAD");
        exit(EXIT_FAILURE);
    }

    LOG(NOTICE, "%s\n", glGetString(GL_VERSION));
    ImGui::CreateContext();
    IMGUI_CHECKVERSION();
    ImGuiIO &io = ImGui::GetIO();

    io.IniFilename = NULL;
    ImGui::LoadIniSettingsFromDisk("./configs/imgui.ini");

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // Enable Multi-Viewport / Platform Windows

    // fonts set
    io.Fonts->AddFontFromFileTTF("./res/fonts/YaHei.ttf", 16.0f, nullptr, io.Fonts->GetGlyphRangesChineseFull());

    m_config = new INIReader("./configs/style.ini");
    int color_style = m_config->GetInteger("STYLE", "style", 5);

    StyleManager::SelectTheme((StyleManager::MStyle_t)color_style);

    ImGuiStyle &style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImGui_ImplGlfw_InitForOpenGL(m_WindowHandler, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
}

void Application::Shutdown()
{
    for (auto &layer : m_LayerStack)
        layer->OnDetach();

    m_LayerStack.clear();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(m_WindowHandler);
    glfwTerminate();
}

Application *Application::GetInstance()
{
    return Instance;
}

void Application::Run()
{
    ImVec4 clear_color = ImVec4(0.33f, 0.33f, 0.33f, 1.00f);

    while (!glfwWindowShouldClose(m_WindowHandler)) {
        // 检查 ESC 键
        if (glfwGetKey(m_WindowHandler, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(m_WindowHandler, GLFW_TRUE);
        }

        // ImGui 帧
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 更新 Layers
        for (auto &layer : m_LayerStack)
            layer->OnUpdate(m_TimeStep);

        // 渲染
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(m_WindowHandler, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        ImGuiIO &io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow *backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }
        glfwSwapBuffers(m_WindowHandler);

        glfwWaitEventsTimeout(1);

        float time = GetTime();
        m_FrameTime = time - m_LastFrameTime;
        m_TimeStep = glm::min<float>(m_FrameTime, 1);
        m_LastFrameTime = time;
    }
}

void Application::Close()
{
    glfwSetWindowShouldClose(m_WindowHandler, GLFW_TRUE); // 通知 GLFW 应该关闭窗口
}

float Application::GetTime()
{
    return (float)glfwGetTime();
}

void Application::PushLayer(const std::shared_ptr<Layer> &layer)
{
    m_LayerStack.emplace_back(layer);
    layer->OnAttach();
}

void Application::window_size_callback_static(GLFWwindow *window, int width, int height)
{
    Application *that = static_cast<Application *>(glfwGetWindowUserPointer(window));
    that->window_size_callback(window, width, height);
}

void Application::mouse_callback_static(GLFWwindow *window, double xpos, double ypos)
{
    Application *that = static_cast<Application *>(glfwGetWindowUserPointer(window));
    that->mouse_callback(window, xpos, ypos);
}

void Application::scroll_callback_static(GLFWwindow *window, double xpos, double ypos)
{
    Application *that = static_cast<Application *>(glfwGetWindowUserPointer(window));
    that->scroll_callback(window, xpos, ypos);
}

void Application::mouse_callback(GLFWwindow *window, double xpos, double ypos)
{
}

void Application::scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
}

void Application::window_size_callback(GLFWwindow *window, int width, int height)
{
}
