#include "OpusCodec.h"
#include <QDebug>

OpusEncoder::OpusEncoder()
{
}

OpusEncoder::~OpusEncoder()
{
    if (m_encoder)
    {
        opus_encoder_destroy(m_encoder);
        m_encoder = nullptr;
    }
}

bool OpusEncoder::initialize()
{
    int error;
    m_encoder = opus_encoder_create(OPUS_SAMPLE_RATE, OPUS_CHANNELS, OPUS_APPLICATION_VOIP, &error);

    if (error != OPUS_OK || !m_encoder)
    {
        qWarning() << "Failed to create Opus encoder:" << opus_strerror(error);
        return false;
    }

    // Configure encoder for Discord voice
    opus_encoder_ctl(m_encoder, OPUS_SET_BITRATE(OPUS_BITRATE));
    opus_encoder_ctl(m_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    opus_encoder_ctl(m_encoder, OPUS_SET_COMPLEXITY(10));       // Max quality
    opus_encoder_ctl(m_encoder, OPUS_SET_PACKET_LOSS_PERC(15)); // Expect some packet loss

    qDebug() << "Opus encoder initialized:" << OPUS_SAMPLE_RATE << "Hz," << OPUS_CHANNELS << "channels," << OPUS_BITRATE << "bps";
    return true;
}

QByteArray OpusEncoder::encode(const QByteArray &pcm)
{
    if (!m_encoder)
    {
        qWarning() << "Opus encoder not initialized";
        return QByteArray();
    }

    // PCM data should be 16-bit signed integers (2 bytes per sample)
    int expectedSize = OPUS_FRAME_SIZE * OPUS_CHANNELS * sizeof(opus_int16);
    if (pcm.size() != expectedSize)
    {
        qWarning() << "Invalid PCM size:" << pcm.size() << "expected:" << expectedSize;
        return QByteArray();
    }

    // Allocate output buffer (max Opus packet size)
    QByteArray output(4000, 0);

    int encodedBytes = opus_encode(
        m_encoder,
        reinterpret_cast<const opus_int16 *>(pcm.constData()),
        OPUS_FRAME_SIZE,
        reinterpret_cast<unsigned char *>(output.data()),
        output.size());

    if (encodedBytes < 0)
    {
        qWarning() << "Opus encoding failed:" << opus_strerror(encodedBytes);
        return QByteArray();
    }

    output.resize(encodedBytes);
    return output;
}

OpusDecoder::OpusDecoder()
{
}

OpusDecoder::~OpusDecoder()
{
    if (m_decoder)
    {
        opus_decoder_destroy(m_decoder);
        m_decoder = nullptr;
    }
}

bool OpusDecoder::initialize()
{
    int error;
    m_decoder = opus_decoder_create(OPUS_SAMPLE_RATE, OPUS_CHANNELS, &error);

    if (error != OPUS_OK || !m_decoder)
    {
        qWarning() << "Failed to create Opus decoder:" << opus_strerror(error);
        return false;
    }

    qDebug() << "Opus decoder initialized:" << OPUS_SAMPLE_RATE << "Hz," << OPUS_CHANNELS << "channels";
    return true;
}

QByteArray OpusDecoder::decode(const QByteArray &opus)
{
    if (!m_decoder)
    {
        qWarning() << "Opus decoder not initialized";
        return QByteArray();
    }

    // Allocate output buffer for PCM data
    int maxFrameSize = OPUS_FRAME_SIZE * OPUS_CHANNELS * sizeof(opus_int16);
    QByteArray output(maxFrameSize, 0);

    int decodedSamples = opus_decode(
        m_decoder,
        reinterpret_cast<const unsigned char *>(opus.constData()),
        opus.size(),
        reinterpret_cast<opus_int16 *>(output.data()),
        OPUS_FRAME_SIZE,
        0 // FEC disabled
    );

    if (decodedSamples < 0)
    {
        qWarning() << "Opus decoding failed:" << opus_strerror(decodedSamples);
        return QByteArray();
    }

    // Resize to actual decoded size
    output.resize(decodedSamples * OPUS_CHANNELS * sizeof(opus_int16));
    return output;
}
