/*
 * Art-Net DMX 发送器 — 实现
 * 来源: QLC+ artnetpacketizer::setupArtNetDmx() + ArtNetController::sendDmx()
 */

#include "ArtNetSender.h"
#include <QDebug>
#include <QNetworkInterface>
#include <QtEndian>

// 根据目标地址找到同一子网的本地 IP，找不到则返回 AnyIPv4
static QHostAddress findLocalIpForTarget(const QHostAddress &target)
{
    // 如果是回环、广播或 Any，走任意接口
    if (target == QHostAddress::LocalHost ||
        target == QHostAddress::Broadcast ||
        target == QHostAddress::AnyIPv4)
        return QHostAddress::AnyIPv4;

    const auto ifaces = QNetworkInterface::allInterfaces();
    for (const auto &iface : ifaces) {
        if (iface.flags() & QNetworkInterface::IsLoopBack)
            continue;
        for (const auto &entry : iface.addressEntries()) {
            QHostAddress ip = entry.ip();
            if (ip.protocol() != QAbstractSocket::IPv4Protocol || ip.isLoopback())
                continue;
            // 检查目标和本地 IP 是否在同一子网
            quint32 targetBits = target.toIPv4Address();
            quint32 localBits  = ip.toIPv4Address();
            quint32 maskBits   = entry.netmask().toIPv4Address();
            if (maskBits == 0) continue;
            if ((targetBits & maskBits) == (localBits & maskBits))
                return ip;
        }
    }
    return QHostAddress::AnyIPv4; // fallback
}

ArtNetSender::ArtNetSender(const QString &address, QObject *parent)
    : QObject(parent)
    , m_socket(new QUdpSocket(this))
    , m_address(address)
    , m_universe(0)
    , m_sequence(0)
    , m_physicalPort(0)
    , m_fullTransmission(false)
{
    m_dmxData.fill(0, DMX_UNIVERSE_SIZE);
    m_lastSentData.fill(0, DMX_UNIVERSE_SIZE);

    // 绑定到目标子网对应的本地网卡 IP，确保广播从正确网卡发出
    QHostAddress target(address);
    QHostAddress local = findLocalIpForTarget(target);
    if (local != QHostAddress::AnyIPv4)
        qInfo() << "ArtNetSender: bind to" << local.toString() << "for target" << address;
    m_socket->bind(local, 0);
}

ArtNetSender::~ArtNetSender()
{
    m_socket->close();
}

void ArtNetSender::setUniverse(uint16_t universe)
{
    m_universe = universe;
}

void ArtNetSender::setChannel(int channel, uint8_t value)
{
    if (channel >= 0 && channel < DMX_UNIVERSE_SIZE)
        m_dmxData[channel] = static_cast<char>(value);
}

void ArtNetSender::setChannels(const QByteArray &data)
{
    int len = qMin(data.size(), DMX_UNIVERSE_SIZE);
    m_dmxData.replace(static_cast<qsizetype>(0), static_cast<qsizetype>(len), data.constData(), static_cast<qsizetype>(len));
}

void ArtNetSender::sendDmx()
{
    // Diff 模式：数据没变就不发
    if (!m_fullTransmission && m_dmxData == m_lastSentData)
        return;

    QByteArray packet = buildArtNetPacket();

    qint64 written = m_socket->writeDatagram(packet, m_address, ARTNET_PORT);
    if (written > 0)
    {
        m_sequence++;
        m_lastSentData = m_dmxData;
        emit dmxSent();
    }
    else
    {
        qWarning() << "ArtNetSender::sendDmx() writeDatagram failed, error:"
                   << m_socket->errorString()
                   << "address:" << m_address.toString();
    }
}

void ArtNetSender::sendPoll()
{
    // ArtPoll 包: 头部 + 0x00 (TalkToMe) + 0x0E (Priority)
    QByteArray packet;
    packet.append(ARTNET_ID, 7);
    packet.append('\0');
    packet.append(static_cast<char>(ARTNET_POLL & 0xFF));
    packet.append(static_cast<char>((ARTNET_POLL >> 8) & 0xFF));
    packet.append(static_cast<char>(ARTNET_VERSION >> 8));
    packet.append(static_cast<char>(ARTNET_VERSION & 0xFF));
    packet.append('\0');  // TalkToMe
    packet.append('\x0E'); // Priority

    m_socket->writeDatagram(packet, m_address, ARTNET_PORT);
}

QByteArray ArtNetSender::buildArtNetPacket() const
{
    /*
     * ArtDmx 包结构:
     * 字节 0-7:   "Art-Net\0"
     * 字节 8-9:   OpCode (0x5000, 小端序)
     * 字节 10-11: 协议版本 (14, 大端序)
     * 字节 12:    序列号
     * 字节 13:    物理端口
     * 字节 14:    宇宙低字节
     * 字节 15:    宇宙高字节
     * 字节 16:    长度高字节
     * 字节 17:    长度低字节 (512 = 0x0200)
     * 字节 18+:  DMX 数据 (512 字节)
     */

    QByteArray packet;
    packet.reserve(ARTNET_HEADER_SIZE + DMX_UNIVERSE_SIZE);

    // 固定头 (12 字节)
    packet.append(ARTNET_ID, 7);
    packet.append('\0');                      // 补齐 "Art-Net\0"
    packet.append(static_cast<char>(ARTNET_DMX & 0xFF));        // OpCode 低
    packet.append(static_cast<char>((ARTNET_DMX >> 8) & 0xFF)); // OpCode 高
    packet.append(static_cast<char>(ARTNET_VERSION >> 8));      // 版本高
    packet.append(static_cast<char>(ARTNET_VERSION & 0xFF));    // 版本低

    // DMX 信息
    packet.append(static_cast<char>(m_sequence));               // 序列号
    packet.append(static_cast<char>(m_physicalPort));           // 物理端口
    packet.append(static_cast<char>(m_universe & 0xFF));        // 宇宙低
    packet.append(static_cast<char>((m_universe >> 8) & 0xFF)); // 宇宙高
    packet.append(static_cast<char>((DMX_UNIVERSE_SIZE >> 8) & 0xFF)); // 长度高
    packet.append(static_cast<char>(DMX_UNIVERSE_SIZE & 0xFF));        // 长度低

    // DMX 数据
    packet.append(m_dmxData);

    return packet;
}
