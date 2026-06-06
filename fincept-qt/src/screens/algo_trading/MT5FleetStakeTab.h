// MT5FleetStakeTab.h/.cpp — verbatim copy of StakeTab for MT5 Fleet
#pragma once
#include <QWidget>
namespace fincept::screens::panels { class LockPanel; class ActiveLocksPanel; class TierPanel; }
namespace fincept::screens {
class MT5FleetStakeTab : public QWidget {
    Q_OBJECT
  public:
    explicit MT5FleetStakeTab(QWidget* parent = nullptr);
    ~MT5FleetStakeTab() override;
  private:
    void build_ui(); void apply_theme();
    panels::LockPanel* lock_panel_ = nullptr;
    panels::ActiveLocksPanel* active_locks_ = nullptr;
    panels::TierPanel* tier_panel_ = nullptr;
};
} // namespace
