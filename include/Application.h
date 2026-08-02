#pragma once

// Application
#include "ApplicationWindow.h"
#include "UI/MainScreen.h"

// System
#include <cstdint>

enum class AppScreen
{
  Main,
};

class Application
{
public:
  bool Initialize();
  void Run();

private:
  void Render();

private:
  // Windowing
  ApplicationWindow window_;

  // Screens
  MainScreen main_screen_;

  // UI state
  AppScreen active_screen_ = AppScreen::Main;
};