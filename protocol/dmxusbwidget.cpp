/*
  Q Light Controller Plus
  dmxusbwidget.cpp

  Copyright (C) Heikki Junnila
  Copyright (C) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <QStringList>
#include <QSettings>
#include <QDebug>
#include <cmath>

#include "dmxusbwidget.h"
// 只保留 Enttec Open DMX USB，其他硬件类型未引入
// #include "enttecdmxusbpro.h"
#include "enttecdmxusbopen.h"
// #include "dmxusbopenrx.h"
// #if defined(Q_WS_X11) || defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
//   #include "nanodmx.h"
//   #include "euroliteusbdmxpro.h"
// #endif
// #include "stageprofi.h"
// #include "vinceusbdmx512.h"
// #include "usbdmxlegacy.h"

#if defined(WIN32) || defined(Q_OS_WIN)
#include <Windows.h>
#define DMXUSB_WINDOWSTIMERRESOLUTION "dmxusb/windowstimerresolution"
#endif

#if defined(WIN32) || defined(Q_OS_WIN)
uint DMXUSBWidget::s_windowsTimerResolution = 1; // Default to 1 millisecond.
#endif

DMXUSBWidget::DMXUSBWidget(DMXInterface *iface, quint32 outputLine, int frequency)
    : m_interface(iface)
    , m_outputBaseLine(outputLine)
    , m_inputBaseLine(0)
{
    Q_ASSERT(iface != NULL);

    QMap <QString, QVariant> freqMap(DMXInterface::frequencyMap());
    if (freqMap.contains(m_interface->serial()))
        setOutputFrequency(freqMap[m_interface->serial()].toInt());
    else
        setOutputFrequency(frequency);

    QList<int> ports;
    ports << (DMXUSBWidget::DMX | DMXUSBWidget::Output);
    setPortsMapping(ports);

#if defined(WIN32) || defined(Q_OS_WIN)
    QSettings settings;
    QVariant var = settings.value(DMXUSB_WINDOWSTIMERRESOLUTION);
    if (var.isValid())
        s_windowsTimerResolution = var.toUInt();

    setWindowsTimerResolution(s_windowsTimerResolution);
#endif
}

DMXUSBWidget::~DMXUSBWidget()
{
    delete m_interface;

#if defined(WIN32) || defined(Q_OS_WIN)
    clearWindowsTimerResolution(s_windowsTimerResolution);
#endif
}

DMXInterface *DMXUSBWidget::iface() const
{
    return m_interface;
}

QString DMXUSBWidget::interfaceTypeString() const
{
    if (m_interface == NULL)
        return QString();

    return m_interface->typeString();
}

QList<DMXUSBWidget *> DMXUSBWidget::widgets()
{
    QList<DMXUSBWidget *> widgetList;
    QList<DMXInterface *> interfacesList;
    quint32 input_id = 0;
    quint32 output_id = 0;

    // 仅使用 libftdi 后端（FTD2XX 和 QtSerial 未引入）
    interfacesList.append(LibFTDIInterface::interfaces(interfacesList));

    QMap <QString, QVariant> types(DMXInterface::typeMap());

    foreach (DMXInterface *iface, interfacesList)
    {
        // 仅支持 Enttec Open DMX USB TX
        if (types.contains(iface->serial()) == true)
        {
            DMXUSBWidget::Type type = (DMXUSBWidget::Type) types[iface->serial()].toInt();
            if (type == DMXUSBWidget::OpenTX)
                widgetList << new EnttecDMXUSBOpen(iface, output_id++);
        }
        // 自动检测: 非特定设备的 FTDI 芯片默认当作 OpenTX
        else if (iface->vendorID() != DMXInterface::NXPVID
                 && iface->vendorID() != DMXInterface::ATMELVID)
        {
            widgetList << new EnttecDMXUSBOpen(iface, output_id++);
        }
    }
    return widgetList;
}

// ===== 桩实现：QLC+ 接口必需但目前未使用 =====

bool DMXUSBWidget::isOpen() { return m_interface && m_interface->isOpen(); }
void DMXUSBWidget::setPortsMapping(QList<int>) { }
int DMXUSBWidget::portFlagsCount(DMXUSBWidget::LineFlags) { return 1; }
int DMXUSBWidget::openPortsCount() { return 1; }
quint32 DMXUSBWidget::lineToPortIndex(quint32, int) { return 0; }
int DMXUSBWidget::outputsNumber() { return 1; }
QStringList DMXUSBWidget::outputNames() { return { "Open DMX USB" }; }
int DMXUSBWidget::outputFrequency() { return m_frequency; }
void DMXUSBWidget::setOutputFrequency(int freq) { m_frequency = freq; }
int DMXUSBWidget::inputsNumber() { return 0; }
QStringList DMXUSBWidget::inputNames() { return {}; }
QString DMXUSBWidget::serial() const { return m_interface ? m_interface->serial() : QString(); }
QString DMXUSBWidget::name() const { return QString(); }
QString DMXUSBWidget::uniqueName(unsigned short, bool) const { return name(); }
QString DMXUSBWidget::realName() const { return name(); }
QString DMXUSBWidget::vendor() const { return m_interface ? m_interface->vendor() : QString(); }
bool DMXUSBWidget::supportRDM() { return false; }
bool DMXUSBWidget::sendRDMCommand(unsigned int, unsigned int, unsigned char, QList<QVariant>) { return false; }
bool DMXUSBWidget::writeUniverse(unsigned int, unsigned int, const QByteArray &data, bool) {
    if (m_portsInfo.size() >= 1)
        m_portsInfo[0].m_universeData = data;
    return true;
}

bool DMXUSBWidget::open(quint32, bool) { return true; }
bool DMXUSBWidget::close(quint32, bool) { return true; }
bool DMXUSBWidget::setWindowsTimerResolution(uint) { return true; }
bool DMXUSBWidget::clearWindowsTimerResolution(uint) { return true; }
