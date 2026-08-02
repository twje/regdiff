#pragma once

// Standard
#include <stdexcept>

// Reported when RegDiff cannot do what it was asked to do: a snapshot file that
// is missing, malformed, or written to a schema version this build does not
// read, or a capture that found nothing at all.
class RegistryError : public std::runtime_error
{
public:
  using std::runtime_error::runtime_error;
};
