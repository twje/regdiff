#include "Regdiff/SnapshotOperations.h"

// Project
#include "Regdiff/RegistryAccess.h"
#include "Regdiff/RegistryError.h"
#include "Regdiff/RegistrySnapshotter.h"
#include "Regdiff/SnapshotJson.h"

// Standard
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <vector>

namespace
{
  std::string UtcTimestamp()
  {
    const auto now =
      std::chrono::floor<std::chrono::seconds>(
        std::chrono::system_clock::now());

    return std::format("{:%Y-%m-%dT%H:%M:%SZ}", now);
  }
}

RegistrySnapshot CaptureSnapshot(const std::vector<std::string>& roots)
{
  const Win32RegistryAccess registry;

  RegistrySnapshot snapshot =
    RegistrySnapshotter(registry).Capture(roots);

  snapshot.captured_at = UtcTimestamp();
  snapshot.computer_name = LocalComputerName();

  if (snapshot.keys.empty())
  {
    std::string message = "no Registry Keys could be read";

    for (const std::string& diagnostic : snapshot.diagnostics)
    {
      message += "\n  " + diagnostic;
    }

    throw RegistryError(message);
  }

  return snapshot;
}

RegistrySnapshot SaveSnapshot(const std::vector<std::string>& roots, const std::filesystem::path& file)
{
  RegistrySnapshot snapshot = CaptureSnapshot(roots);

  std::ofstream stream(
    file,
    std::ios::binary | std::ios::trunc);

  if (!stream)
  {
    throw RegistryError(
      "unable to write snapshot to " + file.string());
  }

  SnapshotWriter().Write(stream, snapshot);

  stream.close();

  if (!stream)
  {
    throw RegistryError(
      "failed while writing snapshot to " + file.string());
  }

  return snapshot;
}

RegistrySnapshot LoadSnapshot(
  const std::filesystem::path& file)
{
  std::ifstream stream(file, std::ios::binary);

  if (!stream)
  {
    throw RegistryError(
      "unable to open snapshot " + file.string());
  }

  try
  {
    return SnapshotReader().Read(stream);
  }
  catch (const RegistryError& error)
  {
    throw RegistryError(
      file.string() + ": " + error.what());
  }
}