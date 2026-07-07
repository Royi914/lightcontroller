/*
 * Art-Net DMX 接收器
 * 完全自包含，仅依赖 Qt Network
 *
 * 用法:
 *   ArtNetReceiver *receiver = new ArtNetReceiver(this);
 *   receiver->setUniverse(1);
 *   connect(receiver, &ArtNetReceiver::dmxReceived,
 *           this, &MyWidget::onDmxReceived);
 *
 * 来源: QLC+ artnetplugin::handlePacket() + artnetpacketizer::fillDMXdata() 中提取重写
 */

#ifndef ARTNETRECEIVER_H
#define ARTNETRECEIVER_H

#include <QObject>
#include <QUdpSocket>
#include <QHostAddress>
#include <QByteArray>

#include "ArtNetCommon.h"
#include "lightprotocol_global.h"

class LIGHTPROTOCOL_EXPORT ArtNetReceiver : public QObject
{
    Q_OBJECT

public:
    explicit ArtNetReceiver(QObject *parent = nullptr);
    ~ArtNetReceiver() override;

    /** 绑定端口（默认 6454），必须调用才能开始接收 */
    bool bind(quint16 port = ARTNET_PORT,
              const QHostAddress &address = QHostAddress::AnyIPv4);

    /** 设置要监听的宇宙 */
    void setUniverse(uint16_t universe) { m_universe = universe; }
    uint16_t universe() const { return m_universe; }

    /** 获取最近收到的 DMX 数据 */
    const QByteArray &lastDmxData() const { return m_dmxData; }

    /** 是否只接收指定宇宙的数据（默认 true） */
    void setFilterUniverse(bool filter) { m_filterUniverse = filter; }

signals:
    /** DMX 数据到达 (universe, data) */
    void dmxReceived(uint16_t universe, const QByteArray &data);

    /** 收到 ArtPoll 查询 */
    void artPollReceived(const QHostAddress &sender, uint8_t talkToMe);

    /** 收到 ArtPollReply */
    void artPollReplyReceived(const QHostAddress &sender,
                               const QString &shortName,
                               const QString &longName);

private slots:
    void onReadyRead();

private:
    void handleArtDmx(const QByteArray &datagram, const QHostAddress &sender);
    void handleArtPoll(const QByteArray &datagram, const QHostAddress &sender);
    void handleArtPollReply(const QByteArray &datagram, const QHostAddress &sender);

    QUdpSocket   *m_socket;
    uint16_t      m_universe;
    bool          m_filterUniverse;
    QByteArray    m_dmxData;
};

#endif // ARTNETRECEIVER_H
