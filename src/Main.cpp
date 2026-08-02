// Project
#include "CommandLine.h"
#include "Application.h"

// Standard
#include <iostream>

int main(int argc, char* argv[])
{
  if (argc == 1)
  {
    Application application;

    if (!application.Initialize())
    {
      std::cerr << "Unable to initialize RegDiff" << std::endl;
      return -1;
    }

    application.Run();

    return 0;
  }

  return RunCommandLine(argc, argv, std::cout, std::cerr);
}
