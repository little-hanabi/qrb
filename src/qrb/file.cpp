#include <iostream>
#include <fstream>
#include <format>
#include <valarray>
#include <ranges>

#include <opencv2/imgcodecs.hpp>

#include <qrb/qr.h>
#include <qrb/index.h>
#include <qrb/page.h>
#include <qrb/file.h>

namespace {
    std::error_code err;

    std::array<std::fstream, 2> stream;
    std::vector<fs::path> list;
    uint64_t bnd = 0;   // 文件块图像与奇偶校验块图像分界
    uint64_t cnt_t = 0; // 总数计数器
    uint64_t cnt_r = 0; // 余量计数器

    std::vector<uint8_t> file_attr; // 编码后的元数据
    uint64_t file_size = 0;

    int64_t seek(const uint32_t index, const bool is_ecc) { return (index - (is_ecc ? 0 : 1)) * qrb::qr::cap() - qrb::index::sum(index, is_ecc); }
}

namespace qrb::file {
    bool config(const fs::path& input_file, const fs::path& output_dir) {
        const auto timestamp = static_cast<uint32_t>(std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        const auto file_name = input_file.filename().u8string(); // 强制使用UTF-8编码，否则MSVC与GCC编译产物可能无法互相解码各自导出的内容
        if (file_name.size() > 255) return false;

        file_attr.resize(0);
        file_attr.push_back(static_cast<uint8_t>(file_name.size() & 0xFF)); // 文件名长度
        for (int i = 3; i >= 0; --i) file_attr.push_back(static_cast<uint8_t>((timestamp >> (8 * i)) & 0xFF)); // UTC 秒级时间戳
        file_attr.insert(file_attr.end(), file_name.begin(), file_name.end()); // 文件名

        file_size = fs::file_size(input_file, err);
        cnt_r = cnt_t = file_size + file_attr.size();

        const auto max_file_size = index::max() * qr::cap() -
                                   index::sum(index::max(), false) -
                                   index::len(0) -
                                   index::len(index::max()) -
                                   file_attr.size();
        if (err || file_size == 0 || file_size > max_file_size) return false;

        list = {output_dir / "file", output_dir / "ecc"};
        for (const auto& dir : list) if (fs::create_directories(dir, err); err) return false;
        stream[0] = std::fstream(input_file, std::ios::binary | std::ios::in);

        return stream[0].is_open();
    }

    bool config(const fs::path& input_dir, const fs::path& output_dir, const fs::path& ecc_dir) {
        std::vector<fs::path>().swap(list);
        for (const auto& entry : fs::directory_iterator(input_dir)) {
            if (!cv::haveImageWriter(entry.path().extension().string())) continue;
            list.push_back(entry.path());
        }
        if (list.empty()) return false;

        bnd = list.size();
        if (!ecc_dir.empty()) for (const auto& entry : fs::directory_iterator(ecc_dir)) {
            if (!cv::haveImageWriter(entry.path().extension().string())) continue;
            list.push_back(entry.path());
        }

        cnt_r = cnt_t = list.size();

        list.push_back(output_dir);
        if (fs::create_directories(output_dir, err); err) return false;
        stream[0] = std::fstream(output_dir / "file.bin", std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
        stream[1] = std::fstream(output_dir / "ecc.bin", std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);

        return stream[0].is_open() && stream[1].is_open();
    }

    void clean() {
        for (auto& s : stream) s.close();
        std::vector<fs::path>().swap(list);
        std::vector<uint8_t>().swap(file_attr);
        bnd = 0;
        cnt_t = 0;
        cnt_r = 0;
    }

    uint64_t total() { return cnt_t; }
    uint64_t remain() { return cnt_r; }

    uint64_t read(std::span<uint8_t> data, uint64_t offset, const uint64_t length) {
        const auto bin_len = std::min(length, file_size - stream[0].tellg()); // 文件流字节长
        const auto m_len = std::min(length - bin_len, file_attr.size());      // 元数据字节长

        cnt_r -= bin_len + m_len;

        stream[0].read(reinterpret_cast<char*>(&data[0]) + offset, static_cast<int64_t>(bin_len));
        offset += bin_len;

        std::reverse_copy(file_attr.end() - static_cast<int64_t>(m_len), file_attr.end(), begin(data) + static_cast<int64_t>(offset));
        file_attr.resize(file_attr.size() - m_len);

        return bin_len + m_len;
    }

    void write(std::span<const uint8_t> data, const fs::path& file_name, const bool is_ecc) {
        page::write(data, list[is_ecc] / file_name);
    }

    std::pair<std::vector<std::vector<uint8_t>>, bool> read() {
        if (cnt_r == 0) return {};
        const auto index = cnt_t - (cnt_r--);
        return {page::read(list[index]), index >= bnd};
    }

    void write(std::span<const uint8_t> data, const uint64_t offset, const uint32_t index, const bool is_ecc) {
        stream[is_ecc].seekp(seek(index, is_ecc)).write(reinterpret_cast<const char*>(data.data()) + offset, static_cast<int64_t>(data.size() - offset));
    }

    std::tuple<uint64_t, uint32_t, fs::path> metadata() {
        file_size = stream[0].seekg(0, std::ios::end).tellg();
        file_attr = std::vector<uint8_t>(std::min(static_cast<uint64_t>(260), file_size));
        if (file_attr.size() < 5) return {};

        const auto offset = static_cast<int64_t>(file_attr.size());
        stream[0].seekg(-offset, std::ios::end).read(reinterpret_cast<char *>(file_attr.data()), offset);
        std::ranges::reverse(file_attr);

        const uint32_t timestamp = (file_attr[1] << 24) | (file_attr[2] << 16) | (file_attr[3] << 8) | file_attr[4];
        if (file_attr.size() < 5 + file_attr[0]) return {};
        const auto file_name = fs::path(std::u8string(file_attr.begin() + 5, file_attr.begin() + 5 + file_attr[0])).filename(); // 防止路径穿越
        file_size -= 5 + file_attr[0];

        for (auto& s : stream) s.close();
        fs::resize_file(list.back() / "file.bin", file_size, err);
        fs::remove(list.back() / "ecc.bin", err);
        fs::rename(list.back() / "file.bin", list.back() / file_name, err);

        return {file_size, timestamp, file_name};
    }

    void repair(std::array<std::unordered_map<uint32_t, bool>, 2>& index, const bool has_last) {
        if (index::step() == 1 || index[1].empty()) return;
        std::array buffer{std::valarray<uint8_t>(qr::cap()), std::valarray<uint8_t>(qr::cap())};

        for (uint32_t i = 0, m = std::ranges::max(index[0] | std::views::keys); i <= m; i += index::step()) { // 按组处理
            std::cout << "\r"
                      << std::format(" {:>4.1f}% [Repair]                ", 99.9 * static_cast<double>(i) / static_cast<double>(m))
                      << std::flush;

            if (!index[1].contains(index::convert(i))) continue; // 该组的奇偶校验块不存在

            for (uint32_t s = (i == 0 ? 1 : i), j = s; j - i < index::step() && j <= m; ++j) {
                if (index[0].contains(j)) continue;

                const auto len = qr::cap() - index::len(i);
                stream[0].seekg(seek(s, false));
                stream[1].seekg(seek(index::convert(s), true)).read(reinterpret_cast<char*>(&buffer[0][0]), len);

                bool success = true;
                for (uint32_t k = s; k - i < index::step() && k <= m; ++k) {
                    stream[0].read(reinterpret_cast<char*>(&buffer[1][0]), len); // 连续读该组每一块，避免重新定位
                    if (k == j) continue;
                    if (!index[0].contains(k) || (k == m && !has_last)) { success = false; break; } // 每组损坏超过1块，则无法恢复
                    buffer[0][std::slice(0, stream[0].gcount(), 1)] ^= buffer[1][std::slice(0, stream[0].gcount(), 1)]; //尾块存在序号前缀导致长度不足，故以实际读入字节数为准
                }
                if (!success) break;

                if (stream[0].eof()) stream[0].clear(); // 复位
                stream[0].seekp(seek(j, false)).write(reinterpret_cast<char*>(&buffer[0][0]), len);
                index[0][j] = true;
                break;
            }
        }

        std::cout << "\r" << "100.0% [Repair]" << std::flush;
    }
}