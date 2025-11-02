#pragma once

#include <span>
#include <filesystem>

#include <opencv2/core.hpp>

namespace fs = std::filesystem;

namespace qrb::page {
    void config(int num_col, int num_row);

    // 每页能容量的二维码个数
    int cap();

    // 将一页数据编码并写到文件
    void write(std::span<const uint8_t> data, const fs::path& file);

    // 读取文件并解码页原始数据
    std::vector<std::vector<uint8_t>> read(const fs::path& file);
}