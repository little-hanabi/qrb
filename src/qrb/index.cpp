#include <qrb/index.h>

namespace {
    constexpr uint32_t max_len = 4;
    constexpr uint32_t max_idx = (1U << (7 * max_len)) - 1;

    uint32_t ecc_level = 0;
    uint32_t ecc_step = 1;
}

namespace qrb::index {
    void config(const uint32_t e) {
        ecc_level = e;
        ecc_step = 1U << ecc_level;
    }

    uint32_t max() { return max_idx; }

    uint32_t len(uint32_t index) {
        uint32_t count = 0;
        do { index >>= 7; } while (++count < max_len && index > 0);
        return count;
    }

    uint32_t step() { return ecc_step; }

    uint32_t convert(const uint32_t index) { return index >> ecc_level; }

    uint32_t sum(const uint32_t index, const bool is_ecc) {
        uint32_t result = 0, count = 0;
        uint32_t next_max = 1U << (7 - (is_ecc ? ecc_level : 0)), cur_max = is_ecc ? (ecc_level == 0) : 1;
        do {
            if (count != 0) next_max <<= 7;
            result += (std::min(index, next_max) - cur_max) * (count + 1);
            cur_max = next_max;
        } while (++count < max_len && index > next_max);
        return result;
    }

    uint32_t encode(uint32_t index, std::span<uint8_t> data, const bool is_ecc) {
        if (is_ecc && ecc_level != 0) {
            index <<= ecc_level;
            index ^= (index ^ (1U << (ecc_level - 1))) & ((1U << ecc_level) - 1);
        }
        uint32_t count = 0;
        for (auto& byte : data) {
            if (++count > max_len) break;
            byte = index & 0x7F;
            index >>= 7;
            if (index != 0) byte |= 0x80; else break;
        }
        return count;
    }

    std::pair<uint32_t, uint32_t> decode(const std::span<const uint8_t> data, const bool is_ecc) {
        uint32_t result = 0, count = 0;
        bool stop = false;
        for (const auto& byte : data) {
            if (stop || ++count > max_len) break;
            result |= (byte & 0x7F) << ((count - 1) * 7);
            if ((byte & 0x80) == 0) stop = true;
        }
        if (!stop || len(result) != count) return {0, 0};
        if (!is_ecc) return {result, count};
        for (int i = 1; i <= 6; ++i) if ((result & ((1U << i) - 1)) == (1U << (i - 1))) {
            if (ecc_level != i) {
                ecc_level = i;
                ecc_step = 1U << ecc_level;
            }
            return {result >> ecc_level, count};
        }
        return {0, 0};
    }
}