#include "Fixture.h"

Fixture::Fixture(const FixtureDef &def, QObject *parent)
    : QObject(parent)
    , m_def(def)
    , m_name(def.name)
    , m_universe(0)
    , m_address(1)
    , m_values(def.channels, '\xFF')  // 默认 255
{
}

QString Fixture::channelName(int ch) const
{
    if (ch >= 0 && ch < m_def.channelNames.size())
        return m_def.channelNames[ch];
    return QString("通道 %1").arg(ch + 1);
}

QList<ChannelRange> Fixture::channelRanges(int ch) const
{
    if (ch >= 0 && ch < m_def.ranges.size())
        return m_def.ranges[ch];
    return {};
}

void Fixture::setChannel(int channel, uint8_t value)
{
    if (channel < 0 || channel >= m_values.size())
        return;
    if (static_cast<uint8_t>(m_values[channel]) == value)
        return;
    m_values[channel] = static_cast<char>(value);
    emit valueChanged(channel, value);
}

uint8_t Fixture::channelValue(int channel) const
{
    if (channel >= 0 && channel < m_values.size())
        return static_cast<uint8_t>(m_values[channel]);
    return 0;
}

void Fixture::setAllChannels(const QByteArray &values)
{
    int len = qMin(values.size(), m_values.size());
    for (int i = 0; i < len; i++)
        setChannel(i, static_cast<uint8_t>(values[i]));
}

void Fixture::setDimmer(uint8_t value)
{
    if (m_def.channels > 0)
        setChannel(0, value);
}

uint8_t Fixture::dimmer() const
{
    return channelValue(0);
}

void Fixture::render(QByteArray &universeData) const
{
    int offset = m_address - 1;  // DMX 地址 1 = 数组索引 0
    int end = qMin(offset + m_values.size(), universeData.size());
    for (int i = 0; i < m_values.size() && (offset + i) < end; i++)
    {
        if (static_cast<uint8_t>(universeData[offset + i]) != 0)
        {
            // HTP 合并：取两者最大值
            uint8_t existing = static_cast<uint8_t>(universeData[offset + i]);
            uint8_t mine = static_cast<uint8_t>(m_values[i]);
            universeData[offset + i] = static_cast<char>(qMax(existing, mine));
        }
        else
        {
            universeData[offset + i] = m_values[i];
        }
    }
}
