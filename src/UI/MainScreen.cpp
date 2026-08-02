#include "UI/MainScreen.h"

// Project
#include "Platform/FileDialog.h"
#include "Regdiff/RegistryComparator.h"
#include "Regdiff/RegistryError.h"
#include "Regdiff/RegistryValueFormatter.h"
#include "Regdiff/SnapshotOperations.h"
#include "Regdiff/TextReportWriter.h"

// Third Party
#include <imgui.h>

// Standard
#include <cfloat>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <fstream>

namespace
{
  std::vector<std::string> DefaultRegistryRoots()
  {
    return {
      "HKEY_CLASSES_ROOT",
      "HKEY_CURRENT_USER",
      "HKEY_LOCAL_MACHINE",
      "HKEY_USERS",
      "HKEY_CURRENT_CONFIG"
    };
  }

  std::filesystem::path ShowSaveSnapshotDialog()
  {
    return FileDialog::SaveFile()
      .SetTitle("Save Snapshot")
      .SetDefaultFilename("snapshot.json")
      .SetDefaultExtension("json")
      .AddFilter("RegDiff Snapshot (*.json)", "*.json")
      .AddFilter("JSON Files (*.json)", "*.json")
      .Show()
      .value_or(std::filesystem::path{});
  }

  std::filesystem::path ShowOpenSnapshotDialog()
  {
    return FileDialog::OpenFile()
      .SetTitle("Open Snapshot")
      .AddFilter("RegDiff Snapshot (*.json)", "*.json")
      .AddFilter("JSON Files (*.json)", "*.json")
      .AddFilter("All Files (*.*)", "*.*")
      .Show()
      .value_or(std::filesystem::path{});
  }

  std::string_view DisplayValueName(const std::string& name)
  {
    return name.empty() ? std::string_view("(Default)") : std::string_view(name);
  }

  std::string FormatOptionalValue(const std::optional<RegistryValue>& value)
  {
    return value.has_value() ? FormatRegistryValue(*value) : "-";
  }

  void SummaryRow(const char* label, const char* value)
  {
    ImGui::TableNextRow();

    ImGui::TableNextColumn();
    ImGui::TextDisabled("%s", label);

    ImGui::TableNextColumn();
    ImGui::TextUnformatted(value);
  }

  void RenderSnapshotPanel(const char* title, SnapshotSelection& selection)
  {
    ImGui::PushID(title);

    ImGui::TextUnformatted(title);
    ImGui::Separator();

    ImGui::TextDisabled("Snapshot");

    if (selection.IsLoaded())
    {
      const std::string filename = selection.path.filename().string();
      ImGui::TextWrapped("%s", filename.c_str());

      if (ImGui::IsItemHovered())
      {
        ImGui::SetTooltip("%s", selection.path.string().c_str());
      }
    }
    else
    {
      ImGui::TextDisabled("No snapshot loaded.");
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Status");

    if (selection.status.empty())
    {
      ImGui::TextDisabled("-");
    }
    else
    {
      ImGui::TextWrapped("%s", selection.status.c_str());
    }

    ImGui::Spacing();

    if (ImGui::Button("Open Snapshot...", ImVec2(-FLT_MIN, 0.0f)))
    {
      const std::filesystem::path path = ShowOpenSnapshotDialog();

      if (!path.empty())
      {
        try
        {
          selection.snapshot = LoadSnapshot(path);
          selection.path = path;
          selection.status = "Snapshot loaded successfully.";
        }
        catch (const RegistryError& error)
        {
          selection.snapshot.reset();
          selection.status = error.what();
        }
      }
    }

    if (ImGui::Button("Create Snapshot...", ImVec2(-FLT_MIN, 0.0f)))
    {
      const std::filesystem::path path = ShowSaveSnapshotDialog();

      if (!path.empty())
      {
        try
        {
          selection.snapshot = SaveSnapshot(DefaultRegistryRoots(), path);
          selection.path = path;
          selection.status = "Snapshot captured successfully.";
        }
        catch (const RegistryError& error)
        {
          selection.snapshot.reset();
          selection.status = error.what();
        }
      }
    }

    ImGui::Separator();

    constexpr ImGuiTableFlags kSummaryFlags =
      ImGuiTableFlags_SizingFixedFit
      | ImGuiTableFlags_NoSavedSettings;

    if (ImGui::BeginTable("##Summary", 2, kSummaryFlags))
    {
      if (selection.IsLoaded())
      {
        const RegistrySnapshot& snapshot = *selection.snapshot;
        const std::string key_count = std::to_string(snapshot.keys.size());
        const std::string diagnostic_count = std::to_string(snapshot.diagnostics.size());

        SummaryRow("Computer", snapshot.computer_name.c_str());
        SummaryRow("Captured", snapshot.captured_at.c_str());
        SummaryRow("Registry Keys", key_count.c_str());
        SummaryRow("Diagnostics", diagnostic_count.c_str());
      }
      else
      {
        SummaryRow("Computer", "-");
        SummaryRow("Captured", "-");
        SummaryRow("Registry Keys", "-");
        SummaryRow("Diagnostics", "-");
      }

      ImGui::EndTable();
    }

    ImGui::PopID();
  }

  void CompareSnapshots(
    const SnapshotSelection& before,
    const SnapshotSelection& after,
    std::optional<ComparisonResult>& comparison,
    std::string& status)
  {
    comparison.reset();

    if (!before.IsLoaded() || !after.IsLoaded())
    {
      status = "Load both snapshots before comparing.";
      return;
    }

    comparison = RegistryComparator().Compare(
      *before.snapshot,
      *after.snapshot);

    const ComparisonResult& result = *comparison;

    const std::size_t difference_count =
      result.added_keys.size()
      + result.removed_keys.size()
      + result.value_differences.size();

    status = difference_count == 0
      ? "No differences found."
      : std::to_string(difference_count) + " differences found.";
  }

  void RenderComparisonRow(
    std::string_view change,
    std::string_view key_path,
    std::string_view value_name,
    std::string_view before,
    std::string_view after)
  {
    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(change.data(), change.data() + change.size());

    ImGui::TableSetColumnIndex(1);
    ImGui::TextUnformatted(key_path.data(), key_path.data() + key_path.size());

    ImGui::TableSetColumnIndex(2);
    ImGui::TextUnformatted(value_name.data(), value_name.data() + value_name.size());

    ImGui::TableSetColumnIndex(3);
    ImGui::TextUnformatted(before.data(), before.data() + before.size());

    ImGui::TableSetColumnIndex(4);
    ImGui::TextUnformatted(after.data(), after.data() + after.size());
  }

  void RenderKeyRows(
    std::string_view change,
    const std::vector<std::string>& key_paths)
  {
    for (const std::string& key_path : key_paths)
    {
      RenderComparisonRow(change, key_path, "-", "-", "-");
    }
  }

  void RenderValueRows(const std::vector<ValueDifference>& differences)
  {
    for (const ValueDifference& difference : differences)
    {
      std::string_view change;

      switch (difference.type)
      {
      case DifferenceType::kValueAdded:
        change = "Value Added";
        break;

      case DifferenceType::kValueRemoved:
        change = "Value Removed";
        break;

      case DifferenceType::kValueModified:
        change = "Value Modified";
        break;
      }

      const std::string before = FormatOptionalValue(difference.before);
      const std::string after = FormatOptionalValue(difference.after);

      RenderComparisonRow(
        change,
        difference.key_path,
        DisplayValueName(difference.value_name),
        before,
        after);
    }
  }

  void RenderComparisonResults(
    const ComparisonResult& result,
    std::string_view status)
  {
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextUnformatted("Comparison Results");

    if (!status.empty())
    {
      ImGui::SameLine();
      ImGui::TextDisabled("(%.*s)", static_cast<int>(status.size()), status.data());
    }

    constexpr ImGuiTableFlags kResultFlags =
      ImGuiTableFlags_Borders
      | ImGuiTableFlags_RowBg
      | ImGuiTableFlags_Resizable
      | ImGuiTableFlags_Reorderable
      | ImGuiTableFlags_Hideable
      | ImGuiTableFlags_ScrollX
      | ImGuiTableFlags_ScrollY
      | ImGuiTableFlags_SizingStretchProp
      | ImGuiTableFlags_NoSavedSettings;

    const ImVec2 table_size(0.0f, ImGui::GetContentRegionAvail().y);

    if (!ImGui::BeginTable("ComparisonResults", 5, kResultFlags, table_size))
    {
      return;
    }

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Change", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupColumn("Registry Key", ImGuiTableColumnFlags_WidthStretch, 0.40f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.15f);
    ImGui::TableSetupColumn("Before", ImGuiTableColumnFlags_WidthStretch, 0.225f);
    ImGui::TableSetupColumn("After", ImGuiTableColumnFlags_WidthStretch, 0.225f);
    ImGui::TableHeadersRow();

    RenderKeyRows("Key Added", result.added_keys);
    RenderKeyRows("Key Removed", result.removed_keys);
    RenderValueRows(result.value_differences);

    ImGui::EndTable();
  }

  std::filesystem::path ShowSaveReportDialog()
  {
    return FileDialog::SaveFile()
      .SetTitle("Export Comparison Report")
      .SetDefaultFilename("comparison_report.txt")
      .SetDefaultExtension("txt")
      .AddFilter("Text Report (*.txt)", "*.txt")
      .AddFilter("Text Files (*.txt)", "*.txt")
      .AddFilter("All Files (*.*)", "*.*")
      .Show()
      .value_or(std::filesystem::path{});
  }

  void ExportComparisonReport(
    const ComparisonResult& comparison,
    std::string& status)
  {
    const std::filesystem::path path = ShowSaveReportDialog();

    if (path.empty())
    {
      return;
    }

    try
    {
      std::ofstream stream(path, std::ios::trunc);

      if (!stream)
      {
        throw RegistryError(
          "Unable to create report: " + path.string());
      }

      TextReportWriter().Write(stream, comparison);

      if (!stream)
      {
        throw RegistryError(
          "Failed while writing report.");
      }

      status = "Comparison report exported successfully.";
    }
    catch (const RegistryError& error)
    {
      status = error.what();
    }
  }
}

void MainScreen::Render()
{
  const ImGuiViewport* viewport = ImGui::GetMainViewport();

  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);

  constexpr ImGuiWindowFlags kWindowFlags =
    ImGuiWindowFlags_NoTitleBar
    | ImGuiWindowFlags_NoResize
    | ImGuiWindowFlags_NoMove
    | ImGuiWindowFlags_NoCollapse;

  ImGui::Begin("RegDiff", nullptr, kWindowFlags);

  constexpr ImGuiTableFlags kPanelFlags =
    ImGuiTableFlags_SizingStretchSame |
    ImGuiTableFlags_NoSavedSettings;

  if (ImGui::BeginTable("SnapshotPanels", 2, kPanelFlags))
  {
    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    RenderSnapshotPanel("Before Snapshot", before_);

    ImGui::TableSetColumnIndex(1);
    RenderSnapshotPanel("After Snapshot", after_);

    ImGui::EndTable();
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  //
  // Actions
  //

  constexpr float kButtonWidth = 180.0f;
  constexpr float kButtonHeight = 32.0f;

  const bool can_compare =
    before_.IsLoaded() &&
    after_.IsLoaded();

  const bool can_export =
    comparison_.has_value();

  const float toolbar_width =
    kButtonWidth * 2.0f +
    ImGui::GetStyle().ItemSpacing.x;

  const float x =
    (ImGui::GetContentRegionAvail().x - toolbar_width) * 0.5f;

  ImGui::SetCursorPosX(std::max(0.0f, x));

  if (!can_compare)
  {
    ImGui::BeginDisabled();
  }

  if (ImGui::Button("Compare", ImVec2(kButtonWidth, kButtonHeight)))
  {
    CompareSnapshots(
      before_,
      after_,
      comparison_,
      compare_status_);
  }

  if (!can_compare)
  {
    ImGui::EndDisabled();
  }

  ImGui::SameLine();

  if (!can_export)
  {
    ImGui::BeginDisabled();
  }

  if (ImGui::Button("Export Report", ImVec2(kButtonWidth, kButtonHeight)))
  {
    ExportComparisonReport(
      *comparison_,
      compare_status_);
  }

  if (!can_export)
  {
    ImGui::EndDisabled();
  }

  //
  // Results
  //

  if (comparison_.has_value())
  {
    RenderComparisonResults(
      *comparison_,
      compare_status_);
  }

  ImGui::End();
}