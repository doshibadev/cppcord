#pragma once

#include <QDialog>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QMediaDevices>
#include <QAudioSource>
#include <QAudioSink>
#include <QTimer>
#include <QIODevice>

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();

    QString getSelectedInputDevice() const;
    QString getSelectedOutputDevice() const;

    void setInputLevel(float level);  // 0.0 to 1.0
    void setOutputLevel(float level); // 0.0 to 1.0

signals:
    void audioDevicesChanged(const QString &inputDevice, const QString &outputDevice);

private slots:
    void onInputDeviceChanged(int index);
    void onOutputDeviceChanged(int index);
    void onTestInputClicked();
    void onTestOutputClicked();
    void refreshDevices();

private:
    void setupUI();
    void loadAudioDevices();

    // Audio device selection
    QComboBox *m_inputDeviceCombo;
    QComboBox *m_outputDeviceCombo;
    QPushButton *m_refreshDevicesBtn;

    // Audio level indicators
    QProgressBar *m_inputLevelBar;
    QProgressBar *m_outputLevelBar;
    QLabel *m_inputStatusLabel;
    QLabel *m_outputStatusLabel;

    // Test buttons
    QPushButton *m_testInputBtn;
    QPushButton *m_testOutputBtn;

    // Save/Cancel
    QPushButton *m_saveBtn;
    QPushButton *m_cancelBtn;

    // Audio testing
    QAudioSource *m_audioSource;
    QAudioSink *m_audioSink;
    QTimer *m_inputLevelTimer;
    QTimer *m_outputLevelTimer;
    QIODevice *m_audioInputDevice;
    QIODevice *m_audioOutputDevice;

    bool m_isTestingInput;
    bool m_isTestingOutput;

    void stopInputTest();
    void stopOutputTest();
    void updateInputLevel();
    void updateOutputLevel();
};

// Sine wave generator for output testing
class ToneGenerator : public QIODevice
{
    Q_OBJECT
public:
    explicit ToneGenerator(QObject *parent = nullptr);

    void start();
    void stop();

    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override;
    qint64 bytesAvailable() const override;

private:
    qint64 m_pos;
    static constexpr int SAMPLE_RATE = 48000;
    static constexpr int FREQUENCY = 440; // A4 note
};
