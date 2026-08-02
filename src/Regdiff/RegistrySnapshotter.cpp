#include "Regdiff/RegistrySnapshotter.h"

// Standard
#include <utility>

RegistrySnapshotter::RegistrySnapshotter(const RegistryAccess& registry)
  : registry_(registry)
{
}

RegistrySnapshot RegistrySnapshotter::Capture(const std::vector<std::string>& roots) const
{
  RegistrySnapshot snapshot;
  snapshot.roots = roots;

  // The Registry Keys still to be read. Working through a list rather than
  // recursing keeps the depth of the Registry tree off the call stack, and the
  // depth of the Registry is not something RegDiff controls.
  std::vector<std::string> pending(roots.rbegin(), roots.rend());

  while (!pending.empty())
  {
    const std::string path = std::move(pending.back());
    pending.pop_back();

    RegistryReadResult contents = registry_.Read(path);
    if (!contents.opened)
    {
      snapshot.diagnostics.push_back(path + ": " + contents.error);
      continue;
    }

    // Pushed in reverse so that the subkeys come back off the list in the order
    // Windows listed them.
    for (auto name = contents.subkey_names.rbegin(); name != contents.subkey_names.rend(); ++name)
    {
      pending.push_back(path + '\\' + *name);
    }

    snapshot.keys.push_back({ .path = path, .values = std::move(contents.values) });
  }

  Canonicalise(snapshot);
  return snapshot;
}
