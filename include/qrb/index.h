#pragma once

#include <string>
#include <span>
#include <cstdint>

namespace qrb::index {
    void config(uint32_t ecc_level);

    // 文件块序号最大值
    uint32_t max();

    // 奇偶校验块序号对应文件块序号的步长
    uint32_t step();

    // 计算文件块序号或解码前的奇偶校验块序号占用字节数
    uint32_t len(uint32_t index);
    
    // 计算文件块序号对应组的奇偶校验块序号
    uint32_t convert(uint32_t index);

    // 计算当前文件块非零序号或奇偶校验块序号之前的所有文件块非零序号或奇偶校验块序号占用的字节大小之和
    uint32_t sum(uint32_t index, bool is_ecc);

    // 将文件块序号或奇偶校验块序号编码为字节序列
    uint32_t encode(uint32_t index, std::span<uint8_t> data, bool is_ecc);

    // 将字节序列解码为文件块序号或奇偶校验块序号
    std::pair<uint32_t, uint32_t> decode(std::span<const uint8_t> data, bool is_ecc);
}