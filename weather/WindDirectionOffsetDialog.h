#pragma once

#include <QDialog>

class QSpinBox;

/** @brief 编辑风向到天线目标方位的偏移角并保存为应用偏好。 */
class WindDirectionOffsetDialog final : public QDialog
{
    Q_OBJECT

public:
    /** @brief 创建并载入已保存的风向偏移角。 */
    explicit WindDirectionOffsetDialog(QWidget *parent = nullptr);

private:
    void saveOffset();

    QSpinBox *m_offset;
};
