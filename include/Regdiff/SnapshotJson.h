#pragma once

// Project
#include "Regdiff/RegistrySnapshot.h"

// Standard
#include <iosfwd>

// Reading and writing the JSON snapshot format.
//
// The two classes are declared together because they are the two halves of one
// file format: a change to the schema has to be made in both, and keeping them
// in one place is what stops them drifting apart.
//
// The format itself is described in docs/JSON_FORMAT.md.

// Writes a Registry snapshot as JSON.
class SnapshotWriter
{
public:
  void Write(std::ostream& stream, const RegistrySnapshot& snapshot) const;
};

// Reads a Registry snapshot from JSON.
//
// Throws RegistryError when the text is not JSON, when a field is missing or
// has the wrong type, or when the snapshot was written to a schema version this
// build does not understand. The snapshot it returns is in canonical order even
// if the file was not.
class SnapshotReader
{
public:
  RegistrySnapshot Read(std::istream& stream) const;
};
