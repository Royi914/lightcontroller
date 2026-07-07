/*
 * DMX 宇宙管理器
 * 一个 Universe = 512 通道的 DMX 缓冲区
 * 包含若干 Fixture，负责 HTP 合并和输出
 */

#ifndef UNIVERSE_H
#define UNIVERSE_H

#include <QObject>
#include <QList>
#include <QByteArray>
#include <cstdint>

#include "ArtNetCommon.h"

class Fixture;
#include "lightprotocol_global.h"

class ArtNetSender;

class LIGHTPROTOCOL_EXPORT Universe : public QObject
{
    Q_OBJECT

public:
    explicit Universe(int id = 0, QObject *parent = nullptr);

    int  id() const { return m_id; }

    // ---- 管理灯具 ----
    void addFixture(Fixture *fixture);
    void removeFixture(Fixture *fixture);
    QList<Fixture *> fixtures() const { return m_fixtures; }
    void clear();

    // ---- 通道读写 ----
    void setChannel(int channel, uint8_t value);
    uint8_t channelValue(int channel) const;

    // ---- 渲染 ----
    // 让所有灯具写入各自的数据（HTP 合并）
    void render();
    const QByteArray &data() const { return m_data; }

    // ---- 输出 ----
    // 绑定到已有的 ArtNetSender（同一宇宙）
    void bindSender(ArtNetSender *sender);
    void send();  // 调 render() 后直接 sendDmx()

    // 临时输出：开个定时器，每隔 N ms 自动渲染+发送
    void startAutoSend(int intervalMs = 25);
    void stopAutoSend();

signals:
    void fixtureAdded(Fixture *fixture);
    void fixtureRemoved(Fixture *fixture);
    void sent();

private:
    int             m_id;
    QList<Fixture *> m_fixtures;
    QByteArray      m_data;            // 512 通道缓冲区
    ArtNetSender   *m_sender = nullptr;
};

#endif // UNIVERSE_H
