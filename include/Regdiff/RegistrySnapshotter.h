#pragma once

// Project
#include "Regdiff/RegistryAccess.h"
#include "Regdiff/RegistrySnapshot.h"

// Standard
#include <string>
#include <vector>

// Captures a snapshot of everything below a set of Registry Keys.
//
// A Registry Key that cannot be opened is recorded as a diagnostic and skipped,
// so one unreadable Registry Key does not cost the rest of the snapshot. A full
// machine snapshot always contains a few of them.
class RegistrySnapshotter
{
public:
  explicit RegistrySnapshotter(const RegistryAccess& registry);

  // Fills in roots, diagnostics, and keys. The caller fills in captured_at and
  // computer_name, which keeps the capture itself free of any dependency on the
  // clock or the machine and so makes it straightforward to test.
  RegistrySnapshot Capture(const std::vector<std::string>& roots) const;

private:
  const RegistryAccess& registry_;
};
