#include <opencv2/imgproc.hpp>

#include <QRReader.h>
#include <QRWriter.h>
#include <QRCodecMode.h>
#include <QRVersion.h>
#include <BitMatrix.h>
#include <GlobalHistogramBinarizer.h>

#include <qrb/qr.h>

namespace {
    constexpr int scale = 4;  // 图像缩放4倍
    constexpr int margin = 2; // 留白2模块

    bool update = true;

    int qr_cap = 0;
    int qr_px = 0;
    int qr_sp = 0;
    float qr_ratio = 0.0f;

    auto encoder = ZXing::QRCode::Writer{};
    auto decoder = ZXing::QRCode::Reader(ZXing::ReaderOptions{}, false);
}

namespace qrb::qr {
    void config(const int qr_version, const int qr_ecc) {
        update = true;

        const auto v = ZXing::QRCode::Version::Model2(qr_version);
        const auto e = static_cast<ZXing::QRCode::ErrorCorrectionLevel>(qr_ecc);

        qr_cap = v->totalCodewords() - v->ecBlocksForLevel(e).totalCodewords() - ZXing::QRCode::CharacterCountBits(ZXing::QRCode::CodecMode::BYTE, qr_version) / 8 - 1;
        qr_px = (4 * qr_version + 17 + 2 * margin) * scale;
        qr_sp = ((4 * qr_version + 17) / 8 - margin) * scale;
        qr_ratio = static_cast<float>(qr_px + qr_sp) / static_cast<float>(qr_px - 2 * margin * scale);

        encoder.setVersion(qr_version);
        encoder.setMargin(margin);
        encoder.setErrorCorrectionLevel(e);
    }

    void fresh() {
        update = true;

        ZXing::QRCode::qr_version.reset();
        ZXing::QRCode::qr_ecc.reset();
    }

    int px() { return qr_px; }
    int sp() { return qr_sp; }
    float ratio() { return qr_ratio; }
    int cap() { return qr_cap; }

    void encode(const std::span<const uint8_t> data, cv::Mat& img) {
        ZXing::BitArray block;
        for (const auto& byte : data) block.appendBits(byte, 8);
        const ZXing::BitMatrix qr = encoder.encode(block, qr_px, qr_px);
        for (int y = 0; y < qr.height(); ++y)
            for (int x = 0; x < qr.width(); ++x)
                if (qr.get(x, y)) img.at<uint8_t>(y, x) = 0; else img.at<uint8_t>(y, x) = 255; // 两种情况都要写入，否则可能会残留上一页的缓冲
    }

    std::pair<std::vector<std::vector<uint8_t>>, std::vector<cv::Rect>> decode(const cv::Mat& img, const bool single) {
        try {
            // 必须为单通道灰度图
            const auto iv = ZXing::ImageView(img.data, img.cols, img.rows, ZXing::ImageFormat::Lum, static_cast<int>(img.step[0]), 1);
            const auto [data, quad] = decoder.decode(ZXing::GlobalHistogramBinarizer(iv), single);

            std::vector<cv::Rect> box;
            for (const auto& q : quad) {
                std::vector<cv::Point> b;
                for (const auto& p : q) b.emplace_back(p.x, p.y);
                if (!b.empty()) box.push_back(cv::boundingRect(b));
            }

            if (!box.empty() && !data.empty()) {
                if (update && ZXing::QRCode::qr_version.has_value() && ZXing::QRCode::qr_ecc.has_value()) {
                    config(ZXing::QRCode::qr_version.value(), ZXing::QRCode::qr_ecc.value());
                    update = false;
                }
                
                return {data, box};
            }
        } catch (...) { return {}; }

        return {};
    }
}