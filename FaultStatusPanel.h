#pragma once

#include "FaultStatusMonitor.h"

#include <QToolButton>

class QDialog;
class QLabel;
class QPushButton;

/** @brief 在状态栏显示故障摘要，点击后打开包含故障详情及清除操作的非模态窗口。 */
class FaultStatusPanel final : public QToolButton
{
    Q_OBJECT

public:
    /** @brief 绑定宿主管理的故障监视器；监视器须覆盖控件及详情窗口的生命周期。 */
    explicit FaultStatusPanel(FaultStatusMonitor *monitor, QWidget *parent = nullptr);

private:
    void updateStatus();
    void updateIndicator();
    void showClearResult(FaultStatusMonitor::ClearResult result);
    QString faultDescription() const;

    FaultStatusMonitor *m_monitor;
    QDialog *m_details;
    QLabel *m_status;
    QLabel *m_result;
    QPushButton *m_clear;
};
