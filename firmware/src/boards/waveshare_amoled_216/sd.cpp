#include "../../hal/sd_hal.h"
#include "board.h"
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

// TF slot on SPI (GPIO 1 MOSI, 2 SCK, 3 MISO, 41 CS; see this board's
// CLAUDE.md). The display owns the S3's SPI2 for its QSPI bus, so the card
// gets SPI3 (HSPI). Nothing here runs at boot: sd_hal_init() only builds the
// SPIClass object, and the card is mounted on the first ask.

static SPIClass* sd_spi = nullptr;
static bool mounted = false;

void sd_hal_init(void) {
    if (!sd_spi) sd_spi = new SPIClass(HSPI);
}

static bool mount(void) {
    if (mounted) return true;
    if (!sd_spi) sd_hal_init();
    sd_spi->begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    // 20 MHz: a safe speed for a slot with short traces; format_if_empty off,
    // so the firmware never wipes a card.
    if (SD.begin(SD_CS, *sd_spi, 20000000, "/sd", 5, false) && SD.cardType() != CARD_NONE) {
        mounted = true;
        return true;
    }
    SD.end();
    sd_spi->end();      // give the pins back until the next try
    return false;
}

bool sd_hal_present(void) { return mount(); }

static const char* type_name(uint8_t t) {
    switch (t) {
        case CARD_MMC:  return "MMC";
        case CARD_SD:   return "SD";
        case CARD_SDHC: return "SDHC";
        default:        return "unknown";
    }
}

void sd_hal_status(void) {
    if (!mount()) {
        Serial.println("{\"sd\":{\"present\":false}}");
        return;
    }
    Serial.printf("{\"sd\":{\"present\":true,\"type\":\"%s\",\"size_mb\":%llu,\"total_mb\":%llu,"
                  "\"used_mb\":%llu,\"free_mb\":%llu}}\n",
                  type_name(SD.cardType()),
                  SD.cardSize() / (1024ULL * 1024ULL),
                  SD.totalBytes() / (1024ULL * 1024ULL),
                  SD.usedBytes() / (1024ULL * 1024ULL),
                  (SD.totalBytes() - SD.usedBytes()) / (1024ULL * 1024ULL));
    sd_hal_ls();
}

void sd_hal_ls(void) {
    if (!mount()) {
        Serial.println("{\"sd\":{\"present\":false}}");
        return;
    }
    File root = SD.open("/");
    if (!root || !root.isDirectory()) {
        Serial.println("{\"sd\":{\"ls\":false}}");
        return;
    }
    int n = 0;
    for (File f = root.openNextFile(); f; f = root.openNextFile()) {
        Serial.printf("{\"sd_file\":\"%s\",\"dir\":%s,\"bytes\":%u}\n", f.name(),
                      f.isDirectory() ? "true" : "false", (unsigned)f.size());
        n++;
    }
    root.close();
    Serial.printf("{\"sd_ls\":{\"count\":%d}}\n", n);
}

int32_t sd_hal_read(const char* path, uint32_t offset, uint8_t* buf, uint32_t len) {
    if (!path || !buf || !mount()) return -1;
    File f = SD.open(path, FILE_READ);
    if (!f || f.isDirectory()) return -1;
    int32_t got = 0;
    if (f.seek(offset)) got = (int32_t)f.read(buf, len);
    f.close();
    return got;
}
