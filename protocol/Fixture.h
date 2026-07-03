/*
 * DMX 灯具模型
 * 一个 Fixture 实例 = 软件里的一台灯
 * 绑定到物理灯光的方式：设置 universe + address（DMX 地址）
 */

#ifndef FIXTURE_H
#define FIXTURE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QMap>
#include <QList>
#include <cstdint>

// ===== 通道自定义值域 =====
struct ChannelRange
{
    QString name;      // "图案片1", "频闪慢速", ...
    int     minValue;  // 4
    int     maxValue;  // 7
};

// ===== 灯具定义（型号模板） =====
struct FixtureDef
{
    QString     name;            // "Mac 700", "LED PAR 64"
    QString     manufacturer;    // "Martin", "Chauvet"
    int         channels;        // 占用的通道数
    QStringList channelNames;    // {"Dimmer","R","G","B","Pan","Tilt",...}
    // 每个通道可选的自定义值域：ranges[i] 为空 = 该通道用滑动条 0-255
    // ranges[i] 不为空 = 该通道用下拉框，列出所有功能
    QList<QList<ChannelRange>> ranges;

    FixtureDef() : channels(0) {}
    FixtureDef(const QString &n, const QString &mfr, int ch, const QStringList &cn)
        : name(n), manufacturer(mfr), channels(ch), channelNames(cn) {}
};

// ===== 灯具实例（你添加的每一台灯） =====
class Fixture : public QObject
{
    Q_OBJECT

public:
    explicit Fixture(const FixtureDef &def = FixtureDef(), QObject *parent = nullptr);

    // ---- 身份 ----
    void setName(const QString &name) { m_name = name; }
    QString name() const { return m_name; }

    const FixtureDef &definition() const { return m_def; }
    int channelCount() const { return m_def.channels; }
    QString channelName(int ch) const;
    QList<ChannelRange> channelRanges(int ch) const;

    // ---- 物理绑定（核心：和实体灯关联的唯一方式） ----
    void setUniverse(int universe) { m_universe = universe; }
    int  universe() const { return m_universe; }

    void setAddress(int address) { m_address = address; }
    int  address() const { return m_address; }

    // ---- 通道读写 ----
    void setChannel(int channel, uint8_t value);
    uint8_t channelValue(int channel) const;
    void setAllChannels(const QByteArray &values);

    // ---- 常用属性快捷 ----
    void setDimmer(uint8_t value);
    uint8_t dimmer() const;

    // ---- 渲染 ----
    void render(QByteArray &universeData) const;

signals:
    void valueChanged(int channel, uint8_t value);

private:
    FixtureDef  m_def;
    QString     m_name;
    int         m_universe = 0;
    int         m_address  = 1;
    QByteArray  m_values;
};

#endif // FIXTURE_H
