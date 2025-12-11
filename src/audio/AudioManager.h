#pragma once

#include <QObject>
#include <QAudioSource>
#include <QAudioSink>
#include <QIODevice>
#include <QBuffer>
#include <QAudioFormat>
#include "OpusCodec.h"

class AudioManager : public QObject
{
    Q_OBJECT

public:
    explicit AudioManager(QObject *parent = nullptr);
    ~AudioManager();

    bool initialize();

    // Microphone capture
    bool startCapture();
    void stopCapture();
    bool isCapturing() const { return m_capturing; }

    // Speaker playback
    bool startPlayback();
    void stopPlayback();
    bool isPlaying() const { return m_playing; }

    // Add incoming opus data for playback
    void addOpusData(const QByteArray &opus);

signals:
    void pcmDataReady(const QByteArray &pcm);   // Raw PCM from microphone
    void opusDataReady(const QByteArray &opus); // Encoded Opus from microphone

private slots:
    void onCaptureReady();
    void processPlaybackQueue();

private:
    QAudioFormat createAudioFormat();

    // Capture (microphone)
    QAudioSource *m_audioSource = nullptr;
    QIODevice *m_captureDevice = nullptr;
    OpusEncoder m_encoder;
    QByteArray m_captureBuffer;
    bool m_capturing = false;

    // Playback (speakers)
    QAudioSink *m_audioSink = nullptr;
    QIODevice *m_playbackDevice = nullptr;
    OpusDecoder m_decoder;
    QList<QByteArray> m_playbackQueue;
    bool m_playing = false;
};
