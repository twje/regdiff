#pragma once

struct GLFWwindow;

class ApplicationWindow
{
public:
  ApplicationWindow() = default;
  ~ApplicationWindow();

  bool Initialize();

  bool ShouldClose() const;

  void BeginFrame();
  void EndFrame();

private:
  void Shutdown();

private:
  GLFWwindow* internal_window_ = nullptr;
};