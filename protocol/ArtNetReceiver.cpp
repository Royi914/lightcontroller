/*
 * Art-Net DMX 接收器 — 实现
 * 来源: QLC+ artnetplugin::handlePacket() + artnetpacketizer::fillDMXdata()
 */

#include "ArtNetReceiver.h"

ArtNetReceiver::ArtNetReceiver(QObject *parent)
    : QObject(parent)
    , m_socket(new QUdpSocket(this))
    , m_universe(0)
    , m_filterUniverse(true)
{
    m_dmxData.fill(0, DMX_UNIVERSE_SIZE);
    connect(m_socket, &QUdpSocket::readyRead,
            this, &ArtNetReceiver::onReadyRead);
}

ArtNetReceiver::~ArtNetReceiver()
{
    m_socket->close();
}

bool ArtNetReceiver::bind(quint16 port, const QHostAddress &address)
{
    return m_socket->bind(address, port, QUdpSocket::ShareAddress);
}

void ArtNetReceiver::onReadyRead()
{
    while (m_socket->hasPendingDatagrams())
    {
        QByteArray datagram;
        datagram.resize(static_cast<int>(m_socket->pendingDatagramSize()));
        QHostAddress sender;
        quint16 senderPort;
        m_socket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);

        // 至少需要 12 字节头
        if (datagram.size() < ARTNET_HEADER_SIZE)
            continue;

        // 检查 "Art-Net" 标识
        if (!datagram.startsWith(ARTNET_ID))
            continue;

        // 提取 OpCode（小端序）
        uint16_t opCode = static_cast<uint8_t>(datagram[8])
                        | (static_cast<uint8_t>(datagram[9]) << 8);

        switch (opCode)
        {
        case ARTNET_DMX:
            handleArtDmx(datagram, sender);
            break;
        case ARTNET_POLL:
            handleArtPoll(datagram, sender);
            break;
        case ARTNET_POLL_REPLY:
            handleArtPollReply(datagram, sender);
            break;
        // RDM 等其他 OpCode 可按需添加
        }
    }
}

void ArtNetReceiver::handleArtDmx(const QByteArray &data, const QHostAddress &sender)
{
    Q_UNUSED(sender)

    // 解析 ArtDmx
    uint8_t sequence  = static_cast<uint8_t>(data[12]);
    uint8_t physical  = static_cast<uint8_t>(data[13]);
    uint16_t universe = static_cast<uint8_t>(data[14])
                      | (static_cast<uint8_t>(data[15]) << 8);
    uint16_t length   = (static_cast<uint8_t>(data[16]) << 8)
                      | static_cast<uint8_t>(data[17]);

    Q_UNUSED(sequence)
    Q_UNUSED(physical)

    // 宇宙过滤
    if (m_filterUniverse && universe != m_universe)
        return;

    int dataLen = qMin(static_cast<int>(length), DMX_UNIVERSE_SIZE);
    int avail = data.size() - ARTNET_HEADER_SIZE;
    dataLen = qMin(dataLen, avail);

    m_dmxData = data.mid(ARTNET_HEADER_SIZE, dataLen);
    if (m_dmxData.size() < DMX_UNIVERSE_SIZE)
        m_dmxData.append(QByteArray(DMX_UNIVERSE_SIZE - m_dmxData.size(), '\0'));

    emit dmxReceived(universe, m_dmxData);
}

void ArtNetReceiver::handleArtPoll(const QByteArray &data, const QHostAddress &sender)
{
    uint8_t talkToMe = (data.size() > 12) ? static_cast<uint8_t>(data[12]) : 0;
    emit artPollReceived(sender, talkToMe);

    // 可以在这里自动回复 ArtPollReply
}

void ArtNetReceiver::handleArtPollReply(const QByteArray &data, const QHostAddress &sender)
{
    Q_UNUSED(data)

    // 解析短名称和长名称（略）
    emit artPollReplyReceived(sender, QString(), QString());
}
