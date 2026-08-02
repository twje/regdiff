#include "CommandLine.h"

// Project
#include "Regdiff/RegistryAccess.h"
#include "Regdiff/RegistryComparator.h"
#include "Regdiff/RegistryError.h"
#include "Regdiff/RegistrySnapshot.h"
#include "Regdiff/RegistrySnapshotter.h"
#include "Regdiff/SnapshotJson.h"
#include "Regdiff/TextReportWriter.h"

// Third party
#include <CLI/CLI.hpp>

// Standard
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace
{
  constexpr int kSuccess = 0;
  constexpr int kDifferencesFound = 1;
  constexpr int kError = 2;

  std::string UpperAscii(std::string text)
  {
    for (char& character : text)
    {
      if (character >= 'a' && character <= 'z')
      {
        character = static_cast<char>(character - 'a' + 'A');
      }
    }

    return text;
  }

  std::vector<std::string> SplitLines(const std::string& text)
  {
    std::vector<std::string> lines;

    std::size_t start = 0;
    while (true)
    {
      const std::size_t end = text.find('\n', start);
      if (end == std::string::npos)
      {
        lines.push_back(text.substr(start));
        return lines;
      }

      lines.push_back(text.substr(start, end - start));
      start = end + 1;
    }
  }

  // CLI11 lays each description out in a column beside its option, which leaves
  // no room for the examples --root needs. This puts every option on a line of
  // its own with its description indented underneath.
  class HelpFormatter : public CLI::Formatter
  {
  public:
    std::string make_group(std::string group,
                           bool is_positional,
                           std::vector<const CLI::Option*> opts) const override
    {
      std::string text = "\n" + UpperAscii(std::move(group)) + ":\n\n";

      for (const CLI::Option* option : opts)
      {
        text += make_option(option, is_positional);
      }

      return text;
    }

    std::string make_option(const CLI::Option* option, bool is_positional) const override
    {
      // all_options, so that an option is listed under every name it answers
      // to: get_name on its own would show only the long one.
      std::string text = "  " + option->get_name(is_positional, true);

      if (!option->get_type_name().empty())
      {
        text += ' ' + option->get_type_name();
      }

      text += '\n';

      for (const std::string& line : SplitLines(option->get_description()))
      {
        text += line.empty() ? std::string("\n") : "      " + line + "\n";
      }

      return text + "\n";
    }
  };

  std::string UtcTimestamp()
  {
    const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
    return std::format("{:%Y-%m-%dT%H:%M:%SZ}", now);
  }

  int RunSnapshot(std::vector<std::string> roots, const std::filesystem::path& output, std::ostream& err)
  {
    if (roots.empty())
    {
      throw RegistryError("nothing to capture: name a Registry root with --root, or use --all");
    }

    // Abbreviations become full names before anything is captured, so that a
    // snapshot taken with --root HKLM matches one taken with the full name.
    for (std::string& root : roots)
    {
      root = ExpandRegistryRoot(root);
    }

    const Win32RegistryAccess registry;

    RegistrySnapshot snapshot = RegistrySnapshotter(registry).Capture(roots);
    snapshot.captured_at = UtcTimestamp();
    snapshot.computer_name = LocalComputerName();

    // Reading nothing at all is almost always a mistyped root rather than a
    // machine with nothing on it, and silently writing an empty snapshot would
    // only be discovered later, when it was compared.
    if (snapshot.keys.empty())
    {
      std::string message = "no Registry Keys could be read";
      for (const std::string& diagnostic : snapshot.diagnostics)
      {
        message += "\n  " + diagnostic;
      }

      throw RegistryError(message);
    }

    std::ofstream file(output, std::ios::binary | std::ios::trunc);
    if (!file)
    {
      throw RegistryError("unable to write the snapshot to " + output.string());
    }

    SnapshotWriter().Write(file, snapshot);
    file.close();

    if (!file)
    {
      throw RegistryError("failed while writing the snapshot to " + output.string());
    }

    err << "Captured " << snapshot.keys.size() << " Registry Keys to " << output.string() << '\n';

    if (!snapshot.diagnostics.empty())
    {
      err << snapshot.diagnostics.size()
          << " Registry Keys could not be read; they are listed under \"diagnostics\" in the snapshot\n";
    }

    return kSuccess;
  }

  RegistrySnapshot LoadSnapshot(const std::filesystem::path& file)
  {
    std::ifstream stream(file, std::ios::binary);
    if (!stream)
    {
      throw RegistryError("unable to open the snapshot " + file.string());
    }

    try
    {
      return SnapshotReader().Read(stream);
    }
    catch (const RegistryError& error)
    {
      throw RegistryError(file.string() + ": " + error.what());
    }
  }

  int RunCompare(const std::filesystem::path& before_file,
                 const std::filesystem::path& after_file,
                 std::ostream& out)
  {
    const RegistrySnapshot before = LoadSnapshot(before_file);
    const RegistrySnapshot after = LoadSnapshot(after_file);

    const ComparisonResult result = RegistryComparator().Compare(before, after);
    TextReportWriter().Write(out, result);

    return result.HasDifferences() ? kDifferencesFound : kSuccess;
  }
}

int RunCommandLine(int argc, const char* const* argv, std::ostream& out, std::ostream& err)
{
  CLI::App app{ "Snapshot and compare the Windows Registry. RegDiff never modifies the Registry.",
                "RegDiff" };
  app.footer("Exit codes: 0 success or no differences, 1 differences found, 2 error.");
  app.require_subcommand(1);
  app.formatter(std::make_shared<HelpFormatter>());

  bool capture_all = false;
  std::vector<std::string> roots;
  std::filesystem::path output;

  CLI::App* snapshot = app.add_subcommand("snapshot", "Capture a JSON snapshot of the Windows Registry");

  CLI::Option* all =
    snapshot->add_flag("-a,--all", capture_all, "Capture the standard Windows Registry hives.");

  CLI::Option* root = snapshot->add_option("-r,--root",
                                           roots,
                                           "Registry root to capture. May be specified multiple times.\n"
                                           "\n"
                                           "Examples:\n"
                                           "  HKLM\n"
                                           "  HKCU\n"
                                           "  HKEY_LOCAL_MACHINE\\SOFTWARE\n"
                                           "  HKEY_CURRENT_USER\\Software")
                        ->type_name("TEXT");

  all->excludes(root);

  snapshot->add_option("-o,--output", output, "JSON snapshot to write.")->required()->type_name("FILE");

  std::filesystem::path before_file;
  std::filesystem::path after_file;

  CLI::App* compare = app.add_subcommand("compare", "Compare two JSON snapshots");
  compare->add_option("before", before_file, "Snapshot describing the earlier state")
    ->required()
    ->check(CLI::ExistingFile);
  compare->add_option("after", after_file, "Snapshot describing the later state")
    ->required()
    ->check(CLI::ExistingFile);

  try
  {
    app.parse(argc, argv);
  }
  catch (const CLI::ParseError& error)
  {
    // CLI11 writes the help or the error itself, and reports 0 for --help.
    return app.exit(error, out, err) == 0 ? kSuccess : kError;
  }

  try
  {
    if (snapshot->parsed())
    {
      return RunSnapshot(capture_all ? StandardRegistryRoots() : roots, output, err);
    }

    return RunCompare(before_file, after_file, out);
  }
  catch (const RegistryError& error)
  {
    err << "RegDiff: " << error.what() << '\n';
    return kError;
  }
}
