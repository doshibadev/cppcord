#pragma once

#include <QByteArray>
#include <opus.h>

// Discord voice uses Opus at 48kHz stereo with 20ms frames
constexpr int OPUS_SAMPLE_RATE = 48000;
constexpr int OPUS_CHANNELS = 2;
constexpr int OPUS_FRAME_SIZE = 960; // 20ms at 48kHz
constexpr int OPUS_BITRATE = 64000;  // 64kbps

class OpusEncoder
{
public:
    OpusEncoder();
    ~OpusEncoder();

    bool initialize();
    QByteArray encode(const QByteArray &pcm);
    bool isValid() const { return m_encoder != nullptr; }

private:
    ::OpusEncoder *m_encoder = nullptr;
};

class OpusDecoder
{
public:
    OpusDecoder();
    ~OpusDecoder();

    bool initialize();
    QByteArray decode(const QByteArray &opus);
    bool isValid() const { return m_decoder != nullptr; }

private:
    ::OpusDecoder *m_decoder = nullptr;
};
