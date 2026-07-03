/*
 * Art-Net 协议常量
 * 参考: Art-Net 4 Specification (Artistic Licence)
 * QLC+ 项目 artnetpacketizer.cpp 中提取
 */

#ifndef ARTNETCOMMON_H
#define ARTNETCOMMON_H

#include <cstdint>

// ============ OpCode 常量 ============
constexpr uint16_t ARTNET_POLL       = 0x2000;  // 节点发现请求
constexpr uint16_t ARTNET_POLL_REPLY = 0x2100;  // 节点发现响应
constexpr uint16_t ARTNET_DMX        = 0x5000;  // DMX 数据
constexpr uint16_t ARTNET_RDM        = 0x8300;  // RDM 数据

// ============ 端口常量 ============
constexpr uint16_t ARTNET_PORT       = 6454;    // Art-Net 默认端口
constexpr uint8_t  ARTNET_HEADER_SIZE = 12;     // 固定头长度

// ============ 头部常量 ============
constexpr const char ARTNET_ID[] = "Art-Net";   // 协议标识 (8 字节，补 \0)
constexpr uint16_t ARTNET_VERSION = 14;         // 协议版本 14

// ============ DMX512 常量 ============
constexpr int DMX_UNIVERSE_SIZE = 512;          // 每个宇宙 512 通道
constexpr int DMX_MAX_CHANNEL   = 255;          // 通道最大值

#endif // ARTNETCOMMON_H
