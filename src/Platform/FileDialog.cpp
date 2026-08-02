#include "Platform/FileDialog.h"

// Windows
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <ShObjIdl.h>

// Standard
#include <cstdlib>

namespace
{
  template<typename T>
  void SafeRelease(T*& object)
  {
    if (object != nullptr)
    {
      object->Release();
      object = nullptr;
    }
  }
}

FileDialog::FileDialog(Mode mode)
  : mode_(mode)
{
  if (const wchar_t* profile = _wgetenv(L"USERPROFILE"))
  {
    initial_directory_ = profile;
  }
}

FileDialog FileDialog::OpenFile()
{
  return FileDialog(Mode::kOpenFile);
}

FileDialog FileDialog::SaveFile()
{
  return FileDialog(Mode::kSaveFile);
}

FileDialog FileDialog::SelectFolder()
{
  return FileDialog(Mode::kSelectFolder);
}

FileDialog& FileDialog::SetTitle(std::string_view title)
{
  title_ = ToWide(title);
  return *this;
}

FileDialog& FileDialog::SetInitialDirectory(std::filesystem::path directory)
{
  initial_directory_ = std::move(directory);
  return *this;
}

FileDialog& FileDialog::SetDefaultFilename(std::string_view filename)
{
  default_filename_ = ToWide(filename);
  return *this;
}

FileDialog& FileDialog::SetDefaultExtension(std::string_view extension)
{
  default_extension_ = ToWide(extension);
  return *this;
}

FileDialog& FileDialog::AddFilter(
  std::string_view description,
  std::string_view pattern)
{
  filter_descriptions_.push_back(ToWide(description));
  filter_patterns_.push_back(ToWide(pattern));

  filters_.push_back({
    filter_descriptions_.back().c_str(),
    filter_patterns_.back().c_str()
    });

  return *this;
}

std::optional<std::filesystem::path> FileDialog::Show() const
{
  IFileDialog* dialog = nullptr;

  const CLSID clsid =
    mode_ == Mode::kSaveFile
    ? CLSID_FileSaveDialog
    : CLSID_FileOpenDialog;

  HRESULT hr = CoCreateInstance(
    clsid,
    nullptr,
    CLSCTX_INPROC_SERVER,
    IID_PPV_ARGS(&dialog));

  if (FAILED(hr))
  {
    return std::nullopt;
  }

  if (!title_.empty())
  {
    dialog->SetTitle(title_.c_str());
  }

  DWORD options = 0;
  dialog->GetOptions(&options);

  options |= FOS_FORCEFILESYSTEM;

  if (mode_ == Mode::kSelectFolder)
  {
    options |= FOS_PICKFOLDERS;
  }

  dialog->SetOptions(options);

  if (!initial_directory_.empty())
  {
    IShellItem* folder = nullptr;

    hr = SHCreateItemFromParsingName(
      initial_directory_.c_str(),
      nullptr,
      IID_PPV_ARGS(&folder));

    if (SUCCEEDED(hr))
    {
      dialog->SetDefaultFolder(folder);
      SafeRelease(folder);
    }
  }

  if (mode_ == Mode::kSaveFile)
  {
    auto* save = static_cast<IFileSaveDialog*>(dialog);

    if (!default_filename_.empty())
    {
      save->SetFileName(default_filename_.c_str());
    }

    if (!default_extension_.empty())
    {
      save->SetDefaultExtension(default_extension_.c_str());
    }
  }

  if (!filters_.empty())
  {
    dialog->SetFileTypes(
      static_cast<UINT>(filters_.size()),
      filters_.data());
  }

  hr = dialog->Show(nullptr);

  if (FAILED(hr))
  {
    SafeRelease(dialog);
    return std::nullopt;
  }

  IShellItem* item = nullptr;

  hr = dialog->GetResult(&item);

  if (FAILED(hr))
  {
    SafeRelease(dialog);
    return std::nullopt;
  }

  PWSTR path = nullptr;

  std::optional<std::filesystem::path> result;

  hr = item->GetDisplayName(
    SIGDN_FILESYSPATH,
    &path);

  if (SUCCEEDED(hr))
  {
    result = path;
    CoTaskMemFree(path);
  }

  SafeRelease(item);
  SafeRelease(dialog);

  return result;
}

std::wstring FileDialog::ToWide(std::string_view text)
{
  return std::wstring(text.begin(), text.end());
}