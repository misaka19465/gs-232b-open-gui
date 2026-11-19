#include "WindWeatherSettingsDialog.h"

#include "AppSettings.h"

#include <QComboBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

WindWeatherSettingsDialog::WindWeatherSettingsDialog(QWidget *parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("windWeatherSettingsDialog"));
    setWindowTitle(tr("Wind data source"));
    setWindowModality(Qt::WindowModal);
    resize(560, 340);

    const AppSettings settings;
    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    m_provider = new QComboBox(this);
    m_provider->setObjectName(QStringLiteral("windWeatherProvider"));
    m_provider->addItem(tr("Open-Meteo (no API key)"),
                        static_cast<int>(WindWeather::Provider::OpenMeteo));
    m_provider->addItem(tr("MET Norway (no API key; slower updates)"),
                        static_cast<int>(WindWeather::Provider::MetNorway));
    m_provider->addItem(tr("QWeather (API Host and API Key)"),
                        static_cast<int>(WindWeather::Provider::QWeather));
    m_provider->setCurrentIndex(m_provider->findData(
        static_cast<int>(settings.windWeatherProvider())));
    form->addRow(tr("Provider"), m_provider);
    layout->addLayout(form);

    m_description = new QLabel(this);
    m_description->setObjectName(QStringLiteral("windWeatherProviderDescription"));
    m_description->setWordWrap(true);
    m_description->setTextFormat(Qt::RichText);
    m_description->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_description->setOpenExternalLinks(true);
    layout->addWidget(m_description);

    m_termsAccepted = new QCheckBox(this);
    m_termsAccepted->setObjectName(QStringLiteral("windWeatherTermsAccepted"));
    m_termsAccepted->setChecked(settings.windWeatherTermsAccepted());
    layout->addWidget(m_termsAccepted);

    m_qWeatherFields = new QWidget(this);
    m_qWeatherFields->setObjectName(QStringLiteral("qWeatherCredentials"));
    auto *qWeatherForm = new QFormLayout(m_qWeatherFields);
    m_apiHost = new QLineEdit(m_qWeatherFields);
    m_apiHost->setObjectName(QStringLiteral("qWeatherApiHost"));
    m_apiHost->setPlaceholderText(QStringLiteral("h2a9cf3mhs.xy.qweatherapi.com"));
    m_apiHost->setText(settings.qWeatherApiHost());
    qWeatherForm->addRow(tr("API Host"), m_apiHost);

    m_apiKey = new QLineEdit(m_qWeatherFields);
    m_apiKey->setObjectName(QStringLiteral("qWeatherApiKey"));
    m_apiKey->setEchoMode(QLineEdit::Password);
    m_apiKey->setText(settings.qWeatherApiKey());
    qWeatherForm->addRow(tr("API Key"), m_apiKey);
    layout->addWidget(m_qWeatherFields);
    layout->addStretch(1);

    auto *buttons = new QDialogButtonBox(this);
    buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    auto *save = buttons->addButton(tr("Save"), QDialogButtonBox::AcceptRole);
    save->setObjectName(QStringLiteral("saveWindWeatherSettings"));
    save->setDefault(true);
    layout->addWidget(buttons);

    connect(m_provider, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        m_termsAccepted->setChecked(false);
        updateProviderDetails();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted,
            this, &WindWeatherSettingsDialog::saveSettings);
    updateProviderDetails();
}

void WindWeatherSettingsDialog::updateProviderDetails()
{
    const auto provider = static_cast<WindWeather::Provider>(m_provider->currentData().toInt());
    const bool qWeather = provider == WindWeather::Provider::QWeather;
    const bool openMeteo = provider == WindWeather::Provider::OpenMeteo;
    m_qWeatherFields->setVisible(qWeather);
    m_termsAccepted->setVisible(!qWeather);
    if (qWeather) {
        m_description->setText(
            tr("Global current-weather data. An API Host and API Key are required. Your Maidenhead grid center is sent over HTTPS; the key is stored unencrypted in local settings."));
        return;
    }
    if (provider == WindWeather::Provider::MetNorway) {
        m_description->setText(
            tr("Global forecast data; no API key is needed. Your Maidenhead grid center and public IP are sent to MET Norway. The app checks at least every three hours; data outside the Nordic region can update about every six hours. A writable local cache is required to honor response expiry and validation headers. A recognizable User-Agent and attribution are required. <a href=\"https://api.met.no/doc/TermsOfService\">Terms</a>."));
        m_termsAccepted->setText(tr("I have read and agree to the MET Weather API terms and attribution requirements."));
        return;
    }
    m_description->setText(
        tr("Global model-based wind data; no API key is needed. Your Maidenhead grid center and public IP are sent to Open-Meteo over HTTPS. The free API is for non-commercial use and requires attribution. <a href=\"https://open-meteo.com/en/terms\">Terms</a>. This is model output, not a nearby weather-station reading."));
    if (openMeteo)
        m_termsAccepted->setText(tr("I agree to Open-Meteo's free non-commercial use and attribution requirements."));
}

void WindWeatherSettingsDialog::saveSettings()
{
    const auto provider = static_cast<WindWeather::Provider>(m_provider->currentData().toInt());
    AppSettings settings;
    if (provider != WindWeather::Provider::QWeather && !m_termsAccepted->isChecked()) {
        QMessageBox::warning(this, tr("Wind data source"),
                             tr("Review and accept this provider's terms before using it."));
        return;
    }
    if (provider == WindWeather::Provider::QWeather &&
        !settings.setQWeatherCredentials(m_apiHost->text(), m_apiKey->text())) {
        QMessageBox::warning(this, tr("Wind data source"),
                             tr("Enter both a valid API Host and API Key, or clear both fields."));
        return;
    }
    if (!settings.setWindWeatherProvider(provider,
                                         provider == WindWeather::Provider::QWeather ||
                                             m_termsAccepted->isChecked())) {
        QMessageBox::warning(this, tr("Application settings"),
                             tr("Could not save application settings."));
        return;
    }
    accept();
}
