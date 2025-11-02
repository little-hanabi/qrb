#include <iostream>
#include <fstream>
#include <format>

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <qrb/qr.h>
#include <qrb/page.h>

namespace {
    constexpr float tolerance = 1.0f / 16.0f; // 误差容忍度，应大于0且小于1
    constexpr double roi_scale = 1.15;        // 识别区域扩展系数，应大于1且小于1.5，否则会干扰掩码工作

    int num_col = 0; // 每列二维码数量
    int num_row = 0; // 每行二维码数量
    int page_cap = 0;
    int page_w = 0; // 页宽像素
    int page_h = 0; // 页高像素

    cv::Mat buffer;

    cv::Mat preprocess (const cv::Mat& img) { // 预处理待解码的图像
        cv::Mat result(static_cast<int>(img.rows * roi_scale), static_cast<int>(img.cols * roi_scale), CV_8UC1, cv::Scalar(255, 255, 255));

        cv::Mat gray, denoise;
        cvtColor(img, gray, cv::COLOR_BGR2GRAY);
        cv::bilateralFilter(gray, denoise, 5, 30, 30); // 经验值
        denoise.copyTo(result(cv::Rect{(result.cols - img.cols) / 2, (result.rows - img.rows) / 2, img.cols, img.rows}));

        return result;
    }

    void save(const fs::path& file) { // 写图像缓冲到文件
        std::vector<uint8_t> binary;
        if (!cv::imencode(file.extension().string(), buffer, binary, {cv::IMWRITE_PNG_COMPRESSION, 4})) return;

        std::ofstream output(file, std::ios::binary);
        if (!output.is_open()) return;
        output.write(reinterpret_cast<const char*>(binary.data()), static_cast<int64_t>(binary.size()));
        output.close();
    }

    void load(const fs::path& file) { // 读文件到图像缓冲
        std::ifstream input(file, std::ios::binary | std::ios::ate);
        if (!input.is_open()) {buffer.release(); return;}
        const auto file_size = input.tellg();
        input.seekg(0);

        std::vector<uint8_t> binary(file_size);
        input.read(reinterpret_cast<char*>(binary.data()), file_size);
        input.close();

        buffer = cv::imdecode(binary, cv::IMREAD_COLOR_BGR); // 固定三通道图像
    }

    std::vector<cv::Rect> segment(const cv::Mat& img, const std::vector<cv::Rect>& box, const bool scale_only) { // 生成识别网格
        assert(!img.empty() && !box.empty());

        std::array<std::vector<float>, 2> center{}; // 二维码区域原始中心点
        for (const auto& b : box) {
            const auto c = (b.tl() + b.br()) / 2;
            center[0].push_back(static_cast<float>(c.x));
            center[1].push_back(static_cast<float>(c.y));
        }

        std::array<float, 2> box_wh{}; // 二维码区域平均宽高
        for (const auto& b : box) {
            box_wh[0] += static_cast<float>(b.width);
            box_wh[1] += static_cast<float>(b.height);
        }
        for (auto& b : box_wh) b /= static_cast<float>(box.size());

        std::array<std::vector<float>, 2> center_xy{}; // 二维码区域各列中心点x坐标的平均值与各行中心点y坐标的平均值
        for (int c = 0; c < 2; ++c) { // 0 => x or w, 1 => y or h
            std::ranges::sort(center[c], std::less{});
            for (size_t i = 0, j = 0; i < center[c].size(); i = j) {
                float sum = center[c][i];
                for (j = i + 1; j < center[c].size(); ++j) {
                    if ((center[c][j] - center[c][j - 1]) / box_wh[c] > tolerance) break;
                    sum += center[c][j];
                }
                center_xy[c].push_back(sum / static_cast<float>(j - i));
            }
        }

        std::array<int, 2> grid_wh{}; // 网格的宽与高
        for (int c = 0; c < 2; ++c) { // 0 => x or w, 1 => y or h
            if (scale_only) {grid_wh[c] = static_cast<int>(box_wh[c]); continue;}
            float est = 0.0f;
            int count = 0;
            for (size_t i = 1; i < center_xy[c].size(); ++i) {
                const float gap = center_xy[c][i] - center_xy[c][i - 1];
                if (const float r = gap / box_wh[c] / qrb::qr::ratio(); r < 1.0f - tolerance || r > 1.0f + tolerance) continue;
                est += gap;
                ++count;
            }
            if (count != 0) grid_wh[c] = static_cast<int>(est / static_cast<float>(count));
            else grid_wh[c] = static_cast<int>(box_wh[c] * qrb::qr::ratio());
        }

        const cv::Point tl{static_cast<int>(center_xy[0][0]) % grid_wh[0], static_cast<int>(center_xy[1][0]) % grid_wh[1]};
        const std::array<int, 2> roi_w {grid_wh[0], static_cast<int>(roi_scale * grid_wh[0])};
        const std::array<int, 2> roi_h {grid_wh[1], static_cast<int>(roi_scale * grid_wh[1])};

        std::vector<cv::Rect> result;
        for (int j = 0; j < (img.rows / grid_wh[1] + 1); ++j) {
            for (int i = 0; i < (img.cols / grid_wh[0] + 1); ++i) {
                const int cx = tl.x + i * grid_wh[0];
                const int cy = tl.y + j * grid_wh[1];

                if (cx >= img.cols || cy >= img.rows) continue;
                if (cx - static_cast<int>(box_wh[0] / 2) < 0 || cy - static_cast<int>(box_wh[1] / 2) < 0) continue;
                if (cx + static_cast<int>(box_wh[0] / 2) >= img.cols || cy + static_cast<int>(box_wh[1] / 2) >= img.rows) continue;

                for (int k = 0; k < 16; ++k) result.emplace_back( // 拓展识别区域应对非严格对齐，首个区域不做任何扩展，便于掩码判断，避免重复识别
                    cv::Point{std::max(0, cx - roi_w[k & 1] / 2), std::max(0, cy - roi_h[(k >> 1) & 1] / 2)},
                    cv::Point{std::min(img.cols - 1, cx + roi_w[(k >> 2) & 1] / 2), std::min(img.rows - 1, cy + roi_h[(k >> 3) & 1] / 2)}
                );
            }
        }

        return result;
    }
}

namespace qrb::page {
    void config(const int n_col, const int n_row) {
        num_col = n_col;
        num_row = n_row;

        page_cap = num_col * num_row;
        page_w = num_col * (qr::px() + qr::sp()) + qr::sp();
        page_h = num_row * (qr::px() + qr::sp()) + qr::sp();

        buffer = cv::Mat(page_h, page_w, CV_8UC1, cv::Scalar(255)); // 固定单通道灰度图像
    }

    int cap() { return page_cap; }

    void write(const std::span<const uint8_t> data, const fs::path& file) {
        if (data.empty() || page_cap * qr::cap() < data.size()) return;

        if (page_cap != (data.size() + qr::cap() - 1) / qr::cap()) buffer.setTo(255); // 无法填满页面时，不清空则会残留上一页的部分图像
        size_t offset = 0, remain = data.size();

        while (remain > 0) {
            const auto idx = offset / qr::cap();
            const int x = static_cast<int>(idx % num_col) * (qr::px() + qr::sp()) + qr::sp();
            const int y = static_cast<int>(idx / num_col) * (qr::px() + qr::sp()) + qr::sp();

            const auto len = remain >= qr::cap() ? qr::cap() : remain;
            auto roi = buffer(cv::Rect{x, y, qr::px(), qr::px()});
            qr::encode(data.subspan(offset, len), roi);

            offset += len;
            remain -= len;
        }

        save(file);
    }

    std::vector<std::vector<uint8_t>> read(const fs::path& file) {
        load(file);
        if (buffer.empty()) return {};

        cv::Mat ori = buffer, page = preprocess(ori);

        std::vector<std::vector<uint8_t>> result;
        std::vector<cv::Rect> ref, roi;
        cv::Mat roi_mask(page.size(), CV_8UC1, cv::Scalar(255));
        rectangle(roi_mask, cv::Rect{(page.cols - ori.cols) / 2, (page.rows - ori.rows) / 2, ori.cols, ori.rows}, cv::Scalar(0), -1);

        double progress = 0.0;

        auto decode_and_update = [&](const bool single) {
            // 计算或修正网格分布
            if (!ref.empty()) roi = segment(page, ref, !single);
            else return;
            // 绘制或修正已解码掩码
            if (single) for (const auto& b : ref) {
                for (size_t i = 15; i < roi.size(); i += 16) { // 选择每组最大区域来绘制掩码
                    if (!roi[i].contains((b.tl() + b.br()) / 2)) continue;
                    rectangle(roi_mask, roi[i], cv::Scalar(255), -1);
                    break;
                }
            }
            // 尝试解码每组网格
            for (size_t i = 0; i < roi.size(); i += 16) {
                for (size_t j = i; j < roi.size() && (j - i) < 16; ++j) {
                    // 不再重复识别
                    double min_px = 0.0;
                    if (cv::minMaxLoc(roi_mask(roi[j]), &min_px); min_px == 255.0) break;
                    // 解码当前区域
                    auto [data, box] = qr::decode(page(roi[j]), single);
                    if (data.empty() || box.empty()) continue;
                    // 填充结果
                    for (auto& b : box) {
                        b.x += roi[j].x;
                        b.y += roi[j].y;
                    }
                    result.insert(result.end(), data.begin(), data.end());
                    ref.insert(ref.end(), box.begin(), box.end());
                    break;
                }

                progress += 33.3 * 16 / static_cast<double>(roi.size());
                std::cout << "\r" << std::format(" {:>4.1f}%", progress) << std::flush;
            }
        };

        // 整体识别
        ref.emplace_back((page.cols - ori.cols) / 2, (page.rows - ori.rows) / 2, ori.cols, ori.rows); // 初始区域为扩展前的原始图像
        decode_and_update(false);
        ref.erase(ref.begin());
        // 计算网格分布，独立识别，并利用可能得到的新信息修正网格分布，再次独立识别，减少遗漏
        decode_and_update(true);
        decode_and_update(true);

        // // 标记识别情况
        // for (auto& b : ref) {
        //     b.x -= (page.cols - ori.cols) / 2;
        //     b.y -= (page.rows - ori.rows) / 2;
        //     cv::rectangle(ori, b, cv::Scalar(0, 255, 0), 2);
        // }
        // if (!ref.empty()) save(file);

        return result;
    }
}