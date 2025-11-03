#pragma once

#include <filesystem>
#include <unordered_map>

namespace fs = std::filesystem;

namespace qrb::file {
    // 配置编码模式下的文件处理
    bool config(const fs::path& input_file, const fs::path& output_dir);

    // 配置解码模式下的文件处理
    bool config(const fs::path& input_dir, const fs::path& output_dir, const fs::path& ecc_dir);

    // 关闭文件流
    void clean();

    // 需处理字节总数或文件总数
    uint64_t total();

    // 剩余的字节总数或文件总数
    uint64_t remain();

    // 读指定长度字节到缓冲
    uint64_t read(std::span<uint8_t> data, uint64_t offset, uint64_t length);

    // 写页原始数据到文件
    void write(std::span<const uint8_t> data, const fs::path& file_name, bool is_ecc);

    // 读文件的原始页数据
    std::pair<std::vector<std::vector<uint8_t>>, bool> read();

    // 写数据到文件的指定序号对应的偏移位置
    void write(std::span<const uint8_t> data, uint64_t offset, uint32_t index, bool is_ecc);

    // 解码并应用附加的元数据
    std::tuple<uint64_t, uint32_t, fs::path> metadata();

    // 奇偶校验修复缺失的数据
    void repair(std::array<std::unordered_map<uint32_t, bool>, 2>& index);
}

