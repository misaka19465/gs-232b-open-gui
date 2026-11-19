#pragma once

#include <QDialog>

class QComboBox;
class QCheckBox;
class QLabel;
class QLineEdit;
class QWidget;

/** @brief 选择风向数据来源并按需配置和风天气凭据。 */
class WindWeatherSettingsDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit WindWeatherSettingsDialog(QWidget *parent = nullptr);

private:
    void updateProviderDetails();
    void saveSettings();

    QComboBox *m_provider;
    QLabel *m_description;
    QCheckBox *m_termsAccepted;
    QWidget *m_qWeatherFields;
    QLineEdit *m_apiHost;
    QLineEdit *m_apiKey;
};
