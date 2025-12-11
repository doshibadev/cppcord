#include "AudioManager.h"
#include <QAudioDevice>
#include <QMediaDevices>
#include <QDebug>

AudioManager::AudioManager(QObject *parent)
    : QObject(parent)
{
}

AudioManager::~AudioManager()
{
    stopCapture();
    stopPlayback();
}

QAudioFormat AudioManager::createAudioFormat()
{
    QAudioFormat format;
    format.setSampleRate(OPUS_SAMPLE_RATE);
    format.setChannelCount(OPUS_CHANNELS);
    format.setSampleFormat(QAudioFormat::Int16); // 16-bit PCM
    return format;
}

bool AudioManager::initialize()
{
    // Initialize Opus encoder and decoder
    if (!m_encoder.initialize())
    {
        qWarning() << "Failed to initialize Opus encoder";
        return false;
    }

    if (!m_decoder.initialize())
    {
        qWarning() << "Failed to initialize Opus decoder";
        return false;
    }

    qDebug() << "AudioManager initialized successfully";
    return true;
}

bool AudioManager::startCapture()
{
    if (m_capturing)
    {
        qWarning() << "Already capturing";
        return false;
    }

    QAudioFormat format = createAudioFormat();
    QAudioDevice inputDevice = QMediaDevices::defaultAudioInput();

    if (!inputDevice.isFormatSupported(format))
    {
        qWarning() << "Audio format not supported by input device";
        qWarning() << "Requested:" << format;
        return false;
    }

    m_audioSource = new QAudioSource(inputDevice, format, this);
    m_captureDevice = m_audioSource->start();

    if (!m_captureDevice)
    {
        qWarning() << "Failed to start audio capture";
        delete m_audioSource;
        m_audioSource = nullptr;
        return false;
    }

    connect(m_captureDevice, &QIODevice::readyRead, this, &AudioManager::onCaptureReady);
    m_capturing = true;
    m_captureBuffer.clear();

    qDebug() << "Audio capture started on device:" << inputDevice.description();
    qDebug() << "Audio format:" << format.sampleRate() << "Hz," << format.channelCount() << "channels";
    return true;
}

void AudioManager::stopCapture()
{
    if (!m_capturing)
        return;

    m_audioSource->stop();
    delete m_audioSource;
    m_audioSource = nullptr;
    m_captureDevice = nullptr;
    m_capturing = false;
    m_captureBuffer.clear();

    qDebug() << "Audio capture stopped";
}

void AudioManager::onCaptureReady()
{
    if (!m_captureDevice)
        return;

    // Read available audio data
    QByteArray data = m_captureDevice->readAll();
    m_captureBuffer.append(data);

    // Process complete Opus frames (20ms = 960 samples * 2 channels * 2 bytes)
    int frameSize = OPUS_FRAME_SIZE * OPUS_CHANNELS * sizeof(opus_int16);

    while (m_captureBuffer.size() >= frameSize)
    {
        QByteArray frame = m_captureBuffer.left(frameSize);
        m_captureBuffer.remove(0, frameSize);

        // Emit raw PCM
        emit pcmDataReady(frame);

        // Encode to Opus
        QByteArray opus = m_encoder.encode(frame);
        if (!opus.isEmpty())
        {
            qDebug() << "Emitting Opus data:" << opus.size() << "bytes";
            emit opusDataReady(opus);
        }
        else
        {
            qDebug() << "Warning: Opus encoding returned empty data";
        }
    }
}

bool AudioManager::startPlayback()
{
    if (m_playing)
    {
        qWarning() << "Already playing";
        return false;
    }

    QAudioFormat format = createAudioFormat();
    QAudioDevice outputDevice = QMediaDevices::defaultAudioOutput();

    if (!outputDevice.isFormatSupported(format))
    {
        qWarning() << "Audio format not supported by output device";
        return false;
    }

    m_audioSink = new QAudioSink(outputDevice, format, this);
    m_playbackDevice = m_audioSink->start();

    if (!m_playbackDevice)
    {
        qWarning() << "Failed to start audio playback";
        delete m_audioSink;
        m_audioSink = nullptr;
        return false;
    }

    m_playing = true;
    qDebug() << "Audio playback started on device:" << outputDevice.description();
    return true;
}

void AudioManager::stopPlayback()
{
    if (!m_playing)
        return;

    m_audioSink->stop();
    delete m_audioSink;
    m_audioSink = nullptr;
    m_playbackDevice = nullptr;
    m_playing = false;
    m_playbackQueue.clear();

    qDebug() << "Audio playback stopped";
}

void AudioManager::addOpusData(const QByteArray &opus)
{
    qDebug() << "addOpusData called with" << opus.size() << "bytes";

    if (!m_playing || !m_playbackDevice)
    {
        qDebug() << "Not playing or no playback device";
        return;
    }

    // Decode Opus to PCM
    QByteArray pcm = m_decoder.decode(opus);
    if (pcm.isEmpty())
    {
        qDebug() << "Opus decode returned empty";
        return;
    }

    qDebug() << "Decoded" << opus.size() << "bytes Opus to" << pcm.size() << "bytes PCM";

    // Write directly to audio sink
    qint64 written = m_playbackDevice->write(pcm);
    if (written < pcm.size())
    {
        qWarning() << "Audio playback buffer full, dropped" << (pcm.size() - written) << "bytes";
    }
}

void AudioManager::processPlaybackQueue()
{
    // Reserved for future buffering implementation if needed
}
