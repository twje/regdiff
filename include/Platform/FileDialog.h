#pragma once

// Standard
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// System
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <Windows.h>
#include <ShObjIdl.h>

class FileDialog
{
public:
  static FileDialog OpenFile();
  static FileDialog SaveFile();
  static FileDialog SelectFolder();

  FileDialog& SetTitle(std::string_view title);

  FileDialog& SetInitialDirectory(std::filesystem::path directory);

  FileDialog& SetDefaultFilename(std::string_view filename);

  FileDialog& SetDefaultExtension(std::string_view extension);

  FileDialog& AddFilter(
    std::string_view description,
    std::string_view pattern);

  std::optional<std::filesystem::path> Show() const;

private:
  enum class Mode
  {
    kOpenFile,
    kSaveFile,
    kSelectFolder
  };

  explicit FileDialog(Mode mode);

  static std::wstring ToWide(std::string_view text);

private:
  Mode mode_;

  std::wstring title_;

  std::filesystem::path initial_directory_;

  std::wstring default_filename_;
  std::wstring default_extension_;

  std::vector<std::wstring> filter_descriptions_;
  std::vector<std::wstring> filter_patterns_;

  std::vector<COMDLG_FILTERSPEC> filters_;
};