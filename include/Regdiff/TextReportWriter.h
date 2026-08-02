#pragma once

// Project
#include "Regdiff/RegistryComparator.h"

// Standard
#include <iosfwd>

// Writes a comparison as plain text.
class TextReportWriter
{
public:
  void Write(std::ostream& stream, const ComparisonResult& result) const;
};
