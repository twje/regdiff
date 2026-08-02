#include "Application.h"

bool Application::Initialize()
{
  if (!window_.Initialize())
  {
    return false;
  }

  return true;
}

void Application::Run()
{
  while (!window_.ShouldClose())
  {
    window_.BeginFrame();

    Render();

    window_.EndFrame();
  }
}

void Application::Render()
{
  switch (active_screen_)
  {
    case AppScreen::Main:
    {
      main_screen_.Render();
      break;
    }
  }
}