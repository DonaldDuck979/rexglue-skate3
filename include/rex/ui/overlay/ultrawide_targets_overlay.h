#pragma once

#include <filesystem>
#include <functional>

#include <rex/ui/imgui_dialog.h>

namespace rex::ui {

class UltrawideTargetsDialog final : public ImGuiDialog {
 public:
  using CloseCallback = std::function<void()>;

  UltrawideTargetsDialog(ImGuiDrawer* drawer, std::filesystem::path export_path,
                         CloseCallback close_callback = {});
  ~UltrawideTargetsDialog();

  void Show();
  void Toggle();
  void Hide();
  bool visible() const { return visible_; }

 protected:
  void OnDraw(ImGuiIO& io) override;

 private:
  std::filesystem::path export_path_;
  CloseCallback close_callback_;
  bool visible_ = false;
  bool show_only_seen_ = true;
  bool draw_section_expanded_ = false;
  bool draw_show_exact_fingerprints_ = false;
  int draw_group_mode_ = 1;
};

}  // namespace rex::ui
