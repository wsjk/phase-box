#pragma once
// Preset slots kept in raw NOR flash (the device's own program flash), so they survive power-off.
//
// Layout: two 4 KB sectors per slot (slot 1 = sectors 0-1, slot 2 = sectors 2-3, ...). A sector holds eight
// 512-byte records, written one after another:
//   u32 sequence | u32 length | data (the encoded preset) | u32 CRC-32 of everything before it | 0xFF padding
// Reading picks the valid record with the highest sequence number in either sector. Writing appends the next
// record; only when the sector is full is the *other* sector erased and written from the start. So:
//   - a sector is erased once every 8 saves of that slot (flash lasts ~100k erases per sector), and
//   - losing power during a write or an erase never loses the last good save: a torn record fails its CRC and
//     the previous one is used, and the sector being erased never holds the newest record.
// The flash chip itself is behind FlashDevice, so all of this is unit-tested on a computer with a RAM fake.
#include <cstdint>
#include <cstring>
#include "PresetSession.hpp"

/// The raw flash region the presets live in. Offsets are from the start of the region.
class FlashDevice {
public:
    static constexpr uint32_t SECTOR_BYTES = 4096;   // smallest erasable unit
    static constexpr uint32_t PAGE_BYTES = 256;      // smallest programmable unit

    virtual ~FlashDevice() = default;
    /// False if the region cannot be used (e.g. the firmware image reaches into it).
    virtual bool available() const { return true; }
    virtual void read(uint32_t offset, uint8_t* out, size_t n) = 0;
    /// Set one sector (offset a multiple of SECTOR_BYTES) to all 0xFF.
    virtual bool erase_sector(uint32_t offset) = 0;
    /// Program `n` bytes (offset and n multiples of PAGE_BYTES) onto erased flash.
    virtual bool program(uint32_t offset, const uint8_t* data, size_t n) = 0;
};

class FlashPresetStorage : public PresetStorage {
public:
    static constexpr int SLOTS = PresetSession::MAX_SLOT;
    static constexpr uint32_t RECORD_BYTES = 512;
    static constexpr uint32_t HEADER_BYTES = 8;    // sequence + length; the CRC follows the data
    static constexpr uint32_t MAX_DATA = RECORD_BYTES - HEADER_BYTES - 4;
    static constexpr uint32_t RECORDS_PER_SECTOR = FlashDevice::SECTOR_BYTES / RECORD_BYTES;
    static constexpr uint32_t REGION_BYTES = static_cast<uint32_t>(SLOTS) * 2 * FlashDevice::SECTOR_BYTES;   // 64 KB
    static_assert(RECORD_BYTES % FlashDevice::PAGE_BYTES == 0, "records must be whole flash pages");

    explicit FlashPresetStorage(FlashDevice& dev) : dev_(dev) {}

    bool available() const override { return dev_.available(); }

    size_t read(int slot, uint8_t* out, size_t cap) override {
        if (!available() || slot < 1 || slot > SLOTS) return 0;
        const Scan sc = scan(slot);
        if (!sc.has_latest || sc.latest_len > cap) return 0;
        dev_.read(record_offset(slot, sc.latest_sector, sc.latest_index), rec_, RECORD_BYTES);
        std::memcpy(out, rec_ + HEADER_BYTES, sc.latest_len);
        return sc.latest_len;
    }

    bool write(int slot, const uint8_t* data, size_t n) override {
        if (!available() || slot < 1 || slot > SLOTS || n == 0 || n > MAX_DATA) return false;
        const Scan sc = scan(slot);
        const int target = sc.has_latest ? sc.latest_sector : 0;
        int sector = target;
        uint32_t index = static_cast<uint32_t>(sc.last_used[target] + 1);
        if (index >= RECORDS_PER_SECTOR) {                     // full: start over in the other sector
            sector = sc.has_latest ? 1 - target : 0;
            if (!dev_.erase_sector(sector_offset(slot, sector))) return false;
            index = 0;
        }

        const uint32_t seq = sc.has_latest ? sc.latest_seq + 1 : 1;
        const uint32_t len = static_cast<uint32_t>(n);
        std::memset(rec_, 0xFF, RECORD_BYTES);
        std::memcpy(rec_, &seq, 4);
        std::memcpy(rec_ + 4, &len, 4);
        std::memcpy(rec_ + HEADER_BYTES, data, n);
        const uint32_t crc = PresetManager::crc32(rec_, HEADER_BYTES + len);
        std::memcpy(rec_ + HEADER_BYTES + len, &crc, 4);

        const uint32_t off = record_offset(slot, sector, index);
        if (!dev_.program(off, rec_, RECORD_BYTES)) return false;
        dev_.read(off, check_, RECORD_BYTES);                  // only report success if it really is in flash
        return std::memcmp(rec_, check_, RECORD_BYTES) == 0;
    }

private:
    struct Scan {
        bool has_latest = false;
        int latest_sector = 0;
        uint32_t latest_index = 0, latest_seq = 0, latest_len = 0;
        int last_used[2] = {-1, -1};   // highest record index in each sector that is not blank
    };

    static uint32_t sector_offset(int slot, int sector) {
        return (static_cast<uint32_t>(slot - 1) * 2 + static_cast<uint32_t>(sector)) * FlashDevice::SECTOR_BYTES;
    }
    static uint32_t record_offset(int slot, int sector, uint32_t index) { return sector_offset(slot, sector) + index * RECORD_BYTES; }

    Scan scan(int slot) {
        Scan sc;
        for (int sector = 0; sector < 2; ++sector) {
            for (uint32_t i = 0; i < RECORDS_PER_SECTOR; ++i) {
                dev_.read(record_offset(slot, sector, i), rec_, RECORD_BYTES);
                bool blank = true;
                for (uint32_t b = 0; b < RECORD_BYTES && blank; ++b) blank = rec_[b] == 0xFF;
                if (blank) continue;
                sc.last_used[sector] = static_cast<int>(i);

                uint32_t seq, len, crc;
                std::memcpy(&seq, rec_, 4);
                std::memcpy(&len, rec_ + 4, 4);
                if (len == 0 || len > MAX_DATA) continue;                                    // torn or damaged
                std::memcpy(&crc, rec_ + HEADER_BYTES + len, 4);
                if (PresetManager::crc32(rec_, HEADER_BYTES + len) != crc) continue;
                if (!sc.has_latest || seq > sc.latest_seq) {
                    sc.has_latest = true;
                    sc.latest_sector = sector;
                    sc.latest_index = i;
                    sc.latest_seq = seq;
                    sc.latest_len = len;
                }
            }
        }
        return sc;
    }

    FlashDevice& dev_;
    uint8_t rec_[RECORD_BYTES];
    uint8_t check_[RECORD_BYTES];
};
