/*
 * Art-Net DMX 发送器
 * 完全自包含，仅依赖 Qt Network + Core
 *
 * 用法:
 *   ArtNetSender *sender = new ArtNetSender("192.168.1.255", this);
 *   sender->setUniverse(1);
 *   sender->setChannel(0, 128);  // 通道1 = 50%
 *   sender->sendDmx();
 *
 * 来源: QLC+ artnetcontroller.cpp + artnetpacketizer.cpp 中提取重写
 */

#ifndef ARTNETSENDER_H
#define ARTNETSENDER_H

#include <QObject>
#include <QUdpSocket>
#include <QHostAddress>
#include <QByteArray>
#include <QTimer>

#include "ArtNetCommon.h"

class ArtNetSender : public QObject
{
    Q_OBJECT

public:
    /**
     * @param address 目标 IP（如 "192.168.1.255" 广播，或设备 IP 单播）
     * @param parent  父对象
     */
    explicit ArtNetSender(const QString &address, QObject *parent = nullptr);
    ~ArtNetSender() override;

    /** 设置 Art-Net 宇宙 (0-32767) */
    void setUniverse(uint16_t universe);
    uint16_t universe() const { return m_universe; }

    /** 设置单个通道值 (0-511, 0-255) */
    void setChannel(int channel, uint8_t value);

    /** 批量设置通道值 */
    void setChannels(const QByteArray &data);

    /** 获取通道缓冲区（可写） */
    QByteArray &dmxData() { return m_dmxData; }
    const QByteArray &dmxData() const { return m_dmxData; }

    /** 发送 DMX 数据包 */
    void sendDmx();

    /** 发送 ArtPoll 节点发现 */
    void sendPoll();

    /** 设置物理端口号 (0-3) */
    void setPhysicalPort(uint8_t port) { m_physicalPort = port; }

    /** 设置是否仅发送变更的通道（Full 模式=false） */
    void setFullTransmission(bool full) { m_fullTransmission = full; }

signals:
    void dmxSent();

private:
    QByteArray buildArtNetPacket() const;

    QUdpSocket   *m_socket;
    QHostAddress  m_address;
    uint16_t      m_universe;
    uint8_t       m_sequence;         // 包序列号 (0-255)
    uint8_t       m_physicalPort;     // 物理端口
    bool          m_fullTransmission; // true=始终发满512通道
    QByteArray    m_dmxData;          // 512 通道数据
    QByteArray    m_lastSentData;     // 上次发送的数据（diff 比较用）
};

#endif // ARTNETSENDER_H
