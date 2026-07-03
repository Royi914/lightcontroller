#include "Universe.h"
#include "Fixture.h"
#include "ArtNetSender.h"
#include <QTimer>

Universe::Universe(int id, QObject *parent)
    : QObject(parent)
    , m_id(id)
{
    m_data.fill(0, DMX_UNIVERSE_SIZE);
}

void Universe::addFixture(Fixture *fixture)
{
    if (!fixture || m_fixtures.contains(fixture))
        return;
    m_fixtures.append(fixture);
    emit fixtureAdded(fixture);
}

void Universe::removeFixture(Fixture *fixture)
{
    if (m_fixtures.removeOne(fixture))
        emit fixtureRemoved(fixture);
}

void Universe::clear()
{
    for (auto *f : m_fixtures)
        emit fixtureRemoved(f);
    m_fixtures.clear();
    m_data.fill(0);
}

void Universe::setChannel(int channel, uint8_t value)
{
    if (channel >= 0 && channel < DMX_UNIVERSE_SIZE)
        m_data[channel] = static_cast<char>(value);
}

uint8_t Universe::channelValue(int channel) const
{
    if (channel >= 0 && channel < DMX_UNIVERSE_SIZE)
        return static_cast<uint8_t>(m_data[channel]);
    return 0;
}

void Universe::render()
{
    // 从 0 开始，每个灯具 HTP 合并
    m_data.fill(0);
    for (auto *fixture : m_fixtures)
        fixture->render(m_data);
}

void Universe::bindSender(ArtNetSender *sender)
{
    m_sender = sender;
    if (sender)
        sender->setUniverse(m_id);
}

void Universe::send()
{
    if (!m_sender)
        return;
    // 如果没手动调过 render，自动调
    // m_sender->setChannels(m_data);
    m_sender->setChannels(m_data);
    m_sender->sendDmx();
    emit sent();
}

void Universe::startAutoSend(int intervalMs)
{
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this]() {
        render();
        send();
    });
    timer->start(intervalMs);
}

void Universe::stopAutoSend()
{
    auto timers = findChildren<QTimer *>();
    for (auto *t : timers)
        t->stop();
}
