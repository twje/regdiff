#pragma once

// Project
#include "Regdiff/RegistrySnapshot.h"
#include "Regdiff/RegistryComparator.h"

// Standard
#include <filesystem>
#include <optional>

struct SnapshotSelection
{
  std::filesystem::path path;
  std::optional<RegistrySnapshot> snapshot;

  std::string status;

  bool IsLoaded() const
  {
    return snapshot.has_value();
  }
};

class MainScreen
{
public:
  void Render();

private:
  SnapshotSelection before_;
  SnapshotSelection after_;
  std::optional<ComparisonResult> comparison_;
  std::string compare_status_;
};