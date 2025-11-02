#include <iostream>
#include <chrono>
#include <ranges>
#include <valarray>

#include <qrb/qr.h>
#include <qrb/page.h>
#include <qrb/index.h>
#include <qrb/file.h>
#include <qrb/qrb.h>

namespace {
    std::error_code err;
}

namespace qrb {
    bool config(const fs::path& input_file, const fs::path& output_dir, const int num_col, const int num_row, const int qr_version, const int qr_ecc, const int file_ecc) {
        if (num_col < 1 || num_row < 1 || qr_version < 1 || qr_version > 40 || qr_ecc < 0 || qr_ecc > 3 || file_ecc < 0 || file_ecc > 6) return false;
        if (!fs::exists(input_file, err) || err || !fs::is_regular_file(input_file, err) || err) return false;

        qr::config(qr_version, qr_ecc);
        page::config(num_col, num_row); // page依赖qr，需先配置qr
        index::config(file_ecc);

        if (page::cap() > index::max()) return false;

        return file::config(input_file, output_dir); // file依赖qr、index和page，需最后配置
    }

    bool config(const fs::path& input_dir, const fs::path& output_dir, const fs::path& ecc_dir) {
        if (!fs::exists(input_dir, err) || err || !fs::is_directory(input_dir, err) || err) return false;

        qr::fresh();
        index::config(0);

        return file::config(input_dir, output_dir, ecc_dir);
    }

    void clean() { file::clean(); }

    void write() {
        // [0] -> 文件 [1] -> 奇偶校验
        std::array buffer = {std::valarray<uint8_t>(qr::cap() * page::cap()), std::valarray<uint8_t>(static_cast<uint8_t>(0), qr::cap() * page::cap())};
        std::array<uint32_t, 2> index = {1, 0};
        std::array<uint64_t, 2> offset = {0, 0};

        const bool use_ecc = index::step() != 1;
        bool stop = false;

        while (!stop) { // 按块循环，按页缓冲
            uint64_t beg = offset[0];

            auto len = qr::cap() - index::len(index[0]);
            if (const auto flg_len = index::len(0); flg_len + file::remain() <= len) { // 文件块尾块
                offset[0] += index::encode(0, std::span{buffer[0]}.subspan(offset[0]), false);
                beg += flg_len;
                len -= flg_len;
                stop = true;
            }
            offset[0] += index::encode(index[0], std::span{buffer[0]}.subspan(offset[0]), false);;
            offset[0] += file::read(buffer[0], offset[0], len);

            if (use_ecc) {// 处理奇偶校验
                buffer[1][std::slice(offset[1],offset[0] - beg,1)] ^= buffer[0][std::slice(beg,offset[0] - beg,1)];
                if ((index[0] + 1) % index::step() == 0 || stop) { // 奇偶校验换块
                    index::encode(index[1], std::span{buffer[1]}.subspan(offset[1]), true);
                    ++index[1];
                    offset[1] += qr::cap();
                }
            }

            for (int c = 0; c <= use_ecc; ++c) if (offset[c] == buffer[c].size() || stop) { // 换页
                file::write(std::span{buffer[c]}.first(offset[c]), std::to_string((index[c] + page::cap() - 1) / page::cap()) + ".png", c);
                offset[c] = 0;
                if (c == 1) buffer[c] = 0; // 奇偶校验缓冲置零
            }

            ++index[0]; // 文件块换块

            std::cout << "\r"
                      << std::format(" {:>4.1f}% [Encode]", 99.9 * (1 - static_cast<double>(file::remain()) / static_cast<double>(file::total())))
                      << std::flush;
        }

        std::cout << "\r" << "100.0%" << std::endl << std::endl << "Blocks: " << index[0] - 1;
        if (use_ecc) std::cout << " + " << index[1] << "(ECC)";
        std::cout << std::endl;
    }
    
    void read() {
        // [0] -> 文件 [1] -> 奇偶校验
        std::array<std::unordered_map<uint32_t, bool>, 2> index{};
        std::optional<uint32_t> last_index;

        while (file::remain() != 0) {
            std::cout << "\r" << "Check  [Decode] [Total: "
                      << std::format(" {:>4.1f}%", 99.9 * (1 - static_cast<double>(file::remain()) / static_cast<double>(file::total())))
                      << "]" << std::flush;

            const auto [data, is_ecc] = file::read();

            for (const auto& block : data) {
                auto [idx, len] = index::decode(block, is_ecc);

                if (len == 0 || (!is_ecc && last_index.has_value() && idx > last_index)) continue; // 序号合法性检查
                if (index[is_ecc].contains(idx)) continue; // 去重
                if (block.size() == len || ((is_ecc || idx != 0) && block.size() != qr::cap())) continue; // 块长度合法性检查

                uint32_t offset = len;

                if (idx == 0 && !last_index.has_value() && !is_ecc) { // 文件块尾块
                    std::tie(idx, len) = index::decode(std::span{block}.subspan(offset), false);
                    if (len == 0 || index[0].contains(idx) || idx == 0) continue; // 尾块序号合法性检查
                    last_index = idx;
                    offset += len;
                }

                file::write(block, offset, idx, is_ecc);
                index[is_ecc][idx] = true;
            }

            std::cout << "\r" << "100.0%" << std::flush;
        }

        std::cout << "\r" << "100.0% [Decode] [Total: 100.0%]" << std::flush;

        file::repair(index);

        std::cout << std::endl << std::endl << "Blocks:  " << index[0].size() << " / ";
        if (last_index.has_value()) std::cout << last_index.value() << std::endl; else std::cout << "?" << std::endl;

        if (!last_index.has_value() || index[0].size() != last_index) { // 存在缺块
            std::cout << "Missing:";
            if (const auto m = std::ranges::max(index[0] | std::views::keys); index[0].size() != m) {
                for (uint32_t i = 1; i <= m; ++i) if (!index[0].contains(i)) std::cout << " [" << i << "]";
                if (!last_index.has_value()) std::cout << " and more";
            }
            else std::cout << " Unknown";
            std::cout << std::endl;
            return;
        }

        auto [file_size, timestamp, file_name] = file::metadata();
        std::cout << "Size:    " << file_size << " Bytes" << std::endl;
        std::cout << "Name:    " << reinterpret_cast<const char*>(file_name.u8string().c_str()); // 强制使用UTF-8编码输出
        if (std::cout.fail()) std::cout.clear(); // 防止终端字符集错误导致无法继续输出后续内容
        std::cout << std::endl;
        std::cout << "Time:    " << std::format("{:%Y-%m-%d %H:%M:%S} UTC", std::chrono::sys_time{std::chrono::seconds{timestamp}}) << std::endl; // 时区支持较差，固定为UTC
    }
}