#pragma once

#include <QDialog>

class QDoubleSpinBox;

/** @brief 编辑仅用于大圆图参考显示的天线增益。 */
class AntennaGainDialog final : public QDialog
{
    Q_OBJECT

public:
    /** @brief 恢复已保存的增益；保存成功才关闭窗口，取消不修改配置。 */
    explicit AntennaGainDialog(QWidget *parent = nullptr);

private:
    void saveGain();

    QDoubleSpinBox *m_gain;
};
