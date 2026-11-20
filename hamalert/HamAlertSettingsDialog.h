#pragma once

#include <QDialog>

class QLineEdit;
class QPushButton;

/** @brief 编辑并保存 HamAlert 用户名与专用 Telnet 密码。 */
class HamAlertSettingsDialog final : public QDialog
{
    Q_OBJECT

public:
    /** @brief 载入当前凭据；取消不修改配置。 */
    explicit HamAlertSettingsDialog(QWidget *parent = nullptr);

private:
    void saveCredentials();

    QLineEdit *m_username;
    QLineEdit *m_password;
    QPushButton *m_save;
};
