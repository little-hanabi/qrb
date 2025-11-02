#pragma once

#include <span>

#include <opencv2/core.hpp>

namespace qrb::qr {
    void config(int qr_version, int qr_ecc);
    
    // 刷新ZXing状态
    void fresh();

    // 含留白的二维码缩放后的边长像素
    int px();

    // 二维码图像之间的间隔像素
    int sp();

    // 扩展前的ROI区域与无留白二维码区域的边长比例
    float ratio();

    // 二维码在当前版本和纠错等级下的容量
    int cap();

    // 编码单个二维码
    void encode(std::span<const uint8_t> data, cv::Mat& img);

    // 解码单个或多个二维码
    std::pair<std::vector<std::vector<uint8_t>>, std::vector<cv::Rect>> decode(const cv::Mat& img, bool single);
}