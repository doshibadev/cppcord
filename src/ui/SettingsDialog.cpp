#include "SettingsDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QAudioFormat>
#include <QDebug>
#include <QtMath>
#include <cmath>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent), m_audioSource(nullptr), m_audioSink(nullptr), m_audioInputDevice(nullptr), m_audioOutputDevice(nullptr), m_isTestingInput(false), m_isTestingOutput(false)
{
    setWindowTitle("Settings");
    setMinimumSize(600, 400);

    m_inputLevelTimer = new QTimer(this);
    m_inputLevelTimer->setInterval(50);
    connect(m_inputLevelTimer, &QTimer::timeout, this, &SettingsDialog::updateInputLevel);

    m_outputLevelTimer = new QTimer(this);
    m_outputLevelTimer->setInterval(50);
    connect(m_outputLevelTimer, &QTimer::timeout, this, &SettingsDialog::updateOutputLevel);

    setupUI();
    loadAudioDevices();
}

SettingsDialog::~SettingsDialog()
{
    stopInputTest();
    stopOutputTest();
}

void SettingsDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Audio Settings Group
    QGroupBox *audioGroup = new QGroupBox("Audio Settings", this);
    QVBoxLayout *audioLayout = new QVBoxLayout(audioGroup);

    // Input Device Section
    QLabel *inputLabel = new QLabel("Input Device (Microphone):", audioGroup);
    inputLabel->setStyleSheet("font-weight: bold;");
    audioLayout->addWidget(inputLabel);

    QHBoxLayout *inputDeviceLayout = new QHBoxLayout();
    m_inputDeviceCombo = new QComboBox(audioGroup);
    m_inputDeviceCombo->setMinimumWidth(300);
    inputDeviceLayout->addWidget(m_inputDeviceCombo);

    m_testInputBtn = new QPushButton("Test", audioGroup);
    m_testInputBtn->setFixedWidth(80);
    inputDeviceLayout->addWidget(m_testInputBtn);
    inputDeviceLayout->addStretch();

    audioLayout->addLayout(inputDeviceLayout);

    // Input status and level
    m_inputStatusLabel = new QLabel("Status: Not detected", audioGroup);
    m_inputStatusLabel->setStyleSheet("color: #ED4245;");
    audioLayout->addWidget(m_inputStatusLabel);

    QLabel *inputLevelLabel = new QLabel("Input Level:", audioGroup);
    audioLayout->addWidget(inputLevelLabel);

    m_inputLevelBar = new QProgressBar(audioGroup);
    m_inputLevelBar->setRange(0, 100);
    m_inputLevelBar->setValue(0);
    m_inputLevelBar->setTextVisible(false);
    m_inputLevelBar->setStyleSheet(
        "QProgressBar { border: 1px solid #555; border-radius: 3px; background-color: #2C2F33; }"
        "QProgressBar::chunk { background-color: #43B581; }");
    audioLayout->addWidget(m_inputLevelBar);

    audioLayout->addSpacing(20);

    // Output Device Section
    QLabel *outputLabel = new QLabel("Output Device (Speakers):", audioGroup);
    outputLabel->setStyleSheet("font-weight: bold;");
    audioLayout->addWidget(outputLabel);

    QHBoxLayout *outputDeviceLayout = new QHBoxLayout();
    m_outputDeviceCombo = new QComboBox(audioGroup);
    m_outputDeviceCombo->setMinimumWidth(300);
    outputDeviceLayout->addWidget(m_outputDeviceCombo);

    m_testOutputBtn = new QPushButton("Test", audioGroup);
    m_testOutputBtn->setFixedWidth(80);
    outputDeviceLayout->addWidget(m_testOutputBtn);
    outputDeviceLayout->addStretch();

    audioLayout->addLayout(outputDeviceLayout);

    // Output status and level
    m_outputStatusLabel = new QLabel("Status: Not detected", audioGroup);
    m_outputStatusLabel->setStyleSheet("color: #ED4245;");
    audioLayout->addWidget(m_outputStatusLabel);

    QLabel *outputLevelLabel = new QLabel("Output Level:", audioGroup);
    audioLayout->addWidget(outputLevelLabel);

    m_outputLevelBar = new QProgressBar(audioGroup);
    m_outputLevelBar->setRange(0, 100);
    m_outputLevelBar->setValue(0);
    m_outputLevelBar->setTextVisible(false);
    m_outputLevelBar->setStyleSheet(
        "QProgressBar { border: 1px solid #555; border-radius: 3px; background-color: #2C2F33; }"
        "QProgressBar::chunk { background-color: #5865F2; }");
    audioLayout->addWidget(m_outputLevelBar);

    audioLayout->addSpacing(20);

    // Refresh button
    m_refreshDevicesBtn = new QPushButton("Refresh Devices", audioGroup);
    audioLayout->addWidget(m_refreshDevicesBtn);

    audioLayout->addStretch();

    mainLayout->addWidget(audioGroup);

    // Button box
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_saveBtn = new QPushButton("Save", this);
    m_saveBtn->setStyleSheet(
        "QPushButton { background-color: #5865F2; color: white; padding: 8px 20px; border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background-color: #4752C4; }");
    buttonLayout->addWidget(m_saveBtn);

    m_cancelBtn = new QPushButton("Cancel", this);
    m_cancelBtn->setStyleSheet(
        "QPushButton { background-color: #4E5058; color: white; padding: 8px 20px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #5D6269; }");
    buttonLayout->addWidget(m_cancelBtn);

    mainLayout->addLayout(buttonLayout);

    // Connect signals
    connect(m_inputDeviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::onInputDeviceChanged);
    connect(m_outputDeviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::onOutputDeviceChanged);
    connect(m_testInputBtn, &QPushButton::clicked, this, &SettingsDialog::onTestInputClicked);
    connect(m_testOutputBtn, &QPushButton::clicked, this, &SettingsDialog::onTestOutputClicked);
    connect(m_refreshDevicesBtn, &QPushButton::clicked, this, &SettingsDialog::refreshDevices);
    connect(m_saveBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void SettingsDialog::loadAudioDevices()
{
    m_inputDeviceCombo->clear();
    m_outputDeviceCombo->clear();

    // Load input devices
    QList<QAudioDevice> inputDevices = QMediaDevices::audioInputs();
    qDebug() << "Found" << inputDevices.size() << "input devices";

    if (inputDevices.isEmpty())
    {
        m_inputDeviceCombo->addItem("No input devices found");
        m_inputStatusLabel->setText("Status: No devices detected");
        m_inputStatusLabel->setStyleSheet("color: #ED4245;");
    }
    else
    {
        QAudioDevice defaultInput = QMediaDevices::defaultAudioInput();
        int defaultIndex = 0;

        for (int i = 0; i < inputDevices.size(); ++i)
        {
            const QAudioDevice &device = inputDevices[i];
            QString deviceName = device.description();
            m_inputDeviceCombo->addItem(deviceName, device.id());

            if (device.id() == defaultInput.id())
            {
                defaultIndex = i;
                deviceName += " (Default)";
                m_inputDeviceCombo->setItemText(i, deviceName);
            }

            qDebug() << "Input device:" << deviceName << "ID:" << device.id();
        }

        m_inputDeviceCombo->setCurrentIndex(defaultIndex);
        m_inputStatusLabel->setText(QString("Status: Ready (%1 devices found)").arg(inputDevices.size()));
        m_inputStatusLabel->setStyleSheet("color: #43B581;");
    }

    // Load output devices
    QList<QAudioDevice> outputDevices = QMediaDevices::audioOutputs();
    qDebug() << "Found" << outputDevices.size() << "output devices";

    if (outputDevices.isEmpty())
    {
        m_outputDeviceCombo->addItem("No output devices found");
        m_outputStatusLabel->setText("Status: No devices detected");
        m_outputStatusLabel->setStyleSheet("color: #ED4245;");
    }
    else
    {
        QAudioDevice defaultOutput = QMediaDevices::defaultAudioOutput();
        int defaultIndex = 0;

        for (int i = 0; i < outputDevices.size(); ++i)
        {
            const QAudioDevice &device = outputDevices[i];
            QString deviceName = device.description();
            m_outputDeviceCombo->addItem(deviceName, device.id());

            if (device.id() == defaultOutput.id())
            {
                defaultIndex = i;
                deviceName += " (Default)";
                m_outputDeviceCombo->setItemText(i, deviceName);
            }

            qDebug() << "Output device:" << deviceName << "ID:" << device.id();
        }

        m_outputDeviceCombo->setCurrentIndex(defaultIndex);
        m_outputStatusLabel->setText(QString("Status: Ready (%1 devices found)").arg(outputDevices.size()));
        m_outputStatusLabel->setStyleSheet("color: #43B581;");
    }
}

QString SettingsDialog::getSelectedInputDevice() const
{
    return m_inputDeviceCombo->currentData().toByteArray();
}

QString SettingsDialog::getSelectedOutputDevice() const
{
    return m_outputDeviceCombo->currentData().toByteArray();
}

void SettingsDialog::setInputLevel(float level)
{
    int percentage = static_cast<int>(level * 100);
    m_inputLevelBar->setValue(percentage);
}

void SettingsDialog::setOutputLevel(float level)
{
    int percentage = static_cast<int>(level * 100);
    m_outputLevelBar->setValue(percentage);
}

void SettingsDialog::onInputDeviceChanged(int index)
{
    Q_UNUSED(index);
}

void SettingsDialog::onOutputDeviceChanged(int index)
{
    Q_UNUSED(index);
}

void SettingsDialog::onTestInputClicked()
{
    if (m_isTestingInput)
    {
        stopInputTest();
        m_testInputBtn->setText("Test");
        m_inputStatusLabel->setText("Status: Test stopped");
        m_inputStatusLabel->setStyleSheet("color: #72767D;");
        m_inputLevelBar->setValue(0);
        return;
    }

    int deviceIndex = m_inputDeviceCombo->currentIndex();
    if (deviceIndex < 0)
    {
        qDebug() << "No input device selected";
        return;
    }

    QByteArray deviceId = m_inputDeviceCombo->itemData(deviceIndex).toByteArray();
    QAudioDevice inputDevice;

    for (const QAudioDevice &device : QMediaDevices::audioInputs())
    {
        if (device.id() == deviceId)
        {
            inputDevice = device;
            break;
        }
    }

    if (inputDevice.isNull())
    {
        qDebug() << "Input device not found";
        return;
    }

    QAudioFormat format;
    format.setSampleRate(48000);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Int16);

    if (!inputDevice.isFormatSupported(format))
    {
        qDebug() << "Audio format not supported, trying default format";
        format = inputDevice.preferredFormat();
    }

    m_audioSource = new QAudioSource(inputDevice, format, this);
    m_audioInputDevice = m_audioSource->start();

    if (m_audioInputDevice)
    {
        m_isTestingInput = true;
        m_testInputBtn->setText("Stop");
        m_inputStatusLabel->setText("Status: Testing - speak into microphone");
        m_inputStatusLabel->setStyleSheet("color: #43B581;");
        m_inputLevelTimer->start();
        qDebug() << "Started input test";
    }
    else
    {
        qDebug() << "Failed to start audio source";
        delete m_audioSource;
        m_audioSource = nullptr;
    }
}

void SettingsDialog::onTestOutputClicked()
{
    if (m_isTestingOutput)
    {
        stopOutputTest();
        m_testOutputBtn->setText("Test");
        m_outputStatusLabel->setText("Status: Test stopped");
        m_outputStatusLabel->setStyleSheet("color: #72767D;");
        m_outputLevelBar->setValue(0);
        return;
    }

    int deviceIndex = m_outputDeviceCombo->currentIndex();
    if (deviceIndex < 0)
    {
        qDebug() << "No output device selected";
        return;
    }

    QByteArray deviceId = m_outputDeviceCombo->itemData(deviceIndex).toByteArray();
    QAudioDevice outputDevice;

    for (const QAudioDevice &device : QMediaDevices::audioOutputs())
    {
        if (device.id() == deviceId)
        {
            outputDevice = device;
            break;
        }
    }

    if (outputDevice.isNull())
    {
        qDebug() << "Output device not found";
        return;
    }

    QAudioFormat format;
    format.setSampleRate(48000);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Int16);

    if (!outputDevice.isFormatSupported(format))
    {
        qDebug() << "Audio format not supported, trying default format";
        format = outputDevice.preferredFormat();
    }

    m_audioSink = new QAudioSink(outputDevice, format, this);

    ToneGenerator *generator = new ToneGenerator(this);
    generator->start();
    m_audioOutputDevice = generator;

    m_audioSink->start(generator);

    m_isTestingOutput = true;
    m_testOutputBtn->setText("Stop");
    m_outputStatusLabel->setText("Status: Playing test tone (440 Hz)");
    m_outputStatusLabel->setStyleSheet("color: #43B581;");
    m_outputLevelTimer->start();
    qDebug() << "Started output test";
}

void SettingsDialog::refreshDevices()
{
    qDebug() << "Refreshing audio devices...";
    stopInputTest();
    stopOutputTest();
    loadAudioDevices();
}

void SettingsDialog::stopInputTest()
{
    if (m_audioSource)
    {
        m_inputLevelTimer->stop();
        m_audioSource->stop();
        delete m_audioSource;
        m_audioSource = nullptr;
        m_audioInputDevice = nullptr;
        m_isTestingInput = false;
    }
}

void SettingsDialog::stopOutputTest()
{
    if (m_audioSink)
    {
        m_outputLevelTimer->stop();
        m_audioSink->stop();
        delete m_audioSink;
        m_audioSink = nullptr;

        if (m_audioOutputDevice)
        {
            delete m_audioOutputDevice;
            m_audioOutputDevice = nullptr;
        }

        m_isTestingOutput = false;
    }
}

void SettingsDialog::updateInputLevel()
{
    if (!m_audioInputDevice)
        return;

    qint64 len = m_audioSource->bytesAvailable();
    if (len <= 0)
        return;

    QByteArray buffer = m_audioInputDevice->read(qMin(len, qint64(4096)));

    const qint16 *samples = reinterpret_cast<const qint16 *>(buffer.constData());
    int sampleCount = buffer.size() / 2;

    qint64 sum = 0;
    for (int i = 0; i < sampleCount; ++i)
    {
        sum += samples[i] * samples[i];
    }

    float rms = std::sqrt(static_cast<float>(sum) / sampleCount);
    float level = qMin(rms / 32768.0f * 10.0f, 1.0f);

    setInputLevel(level);
}

void SettingsDialog::updateOutputLevel()
{
    static float phase = 0.0f;
    phase += 0.1f;
    if (phase > 1.0f)
        phase = 0.0f;

    float level = 0.5f + 0.3f * std::sin(phase * 2.0f * M_PI);
    setOutputLevel(level);
}

// ToneGenerator implementation
ToneGenerator::ToneGenerator(QObject *parent)
    : QIODevice(parent), m_pos(0)
{
}

void ToneGenerator::start()
{
    open(QIODevice::ReadOnly);
}

void ToneGenerator::stop()
{
    close();
}

qint64 ToneGenerator::readData(char *data, qint64 maxlen)
{
    qint16 *out = reinterpret_cast<qint16 *>(data);
    qint64 samples = maxlen / 4;

    for (qint64 i = 0; i < samples; ++i)
    {
        float t = static_cast<float>(m_pos) / SAMPLE_RATE;
        float sample = std::sin(2.0f * M_PI * FREQUENCY * t) * 0.3f;
        qint16 value = static_cast<qint16>(sample * 32767.0f);

        out[i * 2] = value;
        out[i * 2 + 1] = value;

        m_pos++;
    }

    return maxlen;
}

qint64 ToneGenerator::writeData(const char *data, qint64 len)
{
    Q_UNUSED(data)
    Q_UNUSED(len)
    return 0;
}

qint64 ToneGenerator::bytesAvailable() const
{
    return std::numeric_limits<qint64>::max();
}
