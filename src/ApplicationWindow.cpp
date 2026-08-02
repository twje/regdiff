#include "ApplicationWindow.h"

#include <cstdint>
#include <cstdio>

#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_opengl3_loader.h"
#include <imgui.h>

#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h>

#if defined(_MSC_VER) && (_MSC_VER >= 1900) && \
    !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

namespace
{
  constexpr auto WINDOW_WIDTH = std::uint32_t{ 1280 };
  constexpr auto WINDOW_HEIGHT = std::uint32_t{ 720 };

  void glfw_error_callback(int error, const char* description)
  {
    std::fprintf(stderr, "Glfw Error %d: %s\n", error, description);
  }
}

ApplicationWindow::~ApplicationWindow()
{
  Shutdown();
}

bool ApplicationWindow::Initialize()
{
  glfwSetErrorCallback(glfw_error_callback);

  if (!glfwInit())
  {
    return false;
  }

#if defined(IMGUI_IMPL_OPENGL_ES2)

  const char* glsl_version = "#version 100";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);

#elif defined(__APPLE__)

  const char* glsl_version = "#version 150";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

#else

  const char* glsl_version = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

#endif

  internal_window_ = glfwCreateWindow(
    static_cast<std::int32_t>(WINDOW_WIDTH),
    static_cast<std::int32_t>(WINDOW_HEIGHT),
    "RegDiff",
    nullptr,
    nullptr);

  if (internal_window_ == nullptr)
  {
    glfwTerminate();
    return false;
  }

  glfwMakeContextCurrent(internal_window_);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForOpenGL(internal_window_, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  auto& style = ImGui::GetStyle();

  style.Colors[ImGuiCol_TableBorderStrong] =
    ImVec4(1.f, 1.f, 1.f, 1.f);

  style.Colors[ImGuiCol_TableBorderLight] =
    ImVec4(1.f, 1.f, 1.f, 1.f);

  return true;
}

bool ApplicationWindow::ShouldClose() const
{
  return glfwWindowShouldClose(internal_window_);
}

void ApplicationWindow::Shutdown()
{
  if (internal_window_ == nullptr)
  {
    return;
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(internal_window_);
  internal_window_ = nullptr;

  glfwTerminate();
}

void ApplicationWindow::BeginFrame()
{
  glfwPollEvents();

  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();

  ImGui::NewFrame();
}

void ApplicationWindow::EndFrame()
{
  ImGui::Render();

  constexpr ImVec4 clear_color(
    30.f / 255.f,
    30.f / 255.f,
    30.f / 255.f,
    1.f);

  int display_width = 0;
  int display_height = 0;

  glfwGetFramebufferSize(
    internal_window_,
    &display_width,
    &display_height
  );

  glViewport(0, 0, display_width, display_height);

  glClearColor(
    clear_color.x * clear_color.w,
    clear_color.y * clear_color.w,
    clear_color.z * clear_color.w,
    clear_color.w);

  glClear(GL_COLOR_BUFFER_BIT);

  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

  glfwSwapBuffers(internal_window_);
}