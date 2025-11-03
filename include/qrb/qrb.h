#pragma once

#include <string>
#include <filesystem>

namespace fs = std::filesystem;

namespace qrb{
    constexpr std::string NAME = "qrb";
    constexpr std::string VERSION = "1.0.1";

    // 配置编码参数
    bool config(const fs::path& input_file, const fs::path& output_dir, int num_col, int num_row, int qr_version, int qr_ecc, int file_ecc = 0);
    
    // 配置解码参数
    bool config(const fs::path& input_dir, const fs::path& output_dir, const fs::path& ecc_dir = {});

    // 关闭文件流
    void clean();

    // 编码
    void write();

    // 解码
    void read();
}