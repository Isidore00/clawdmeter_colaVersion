#include "../../hal/sd_hal.h"
#include "board.h"
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"

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

// `sd raw`: the card's own init and sector 0, straight from the IDF SPI-SD
// driver with no FAT in between, so "the slot works" can be told apart from
// "the file system didn't mount". Frees the bus again afterwards.
void sd_hal_raw(void) {
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI3_HOST;
    host.max_freq_khz = 4000;
    spi_bus_config_t bus = {};
    bus.mosi_io_num = SD_MOSI;
    bus.miso_io_num = SD_MISO;
    bus.sclk_io_num = SD_SCK;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = 4096;
    esp_err_t e = spi_bus_initialize((spi_host_device_t)host.slot, &bus, SPI_DMA_CH_AUTO);
    if (e != ESP_OK) {
        Serial.printf("{\"sd_raw\":{\"step\":\"bus\",\"err\":%d}}\n", (int)e);
        return;
    }
    sdspi_device_config_t dev = SDSPI_DEVICE_CONFIG_DEFAULT();
    dev.host_id = (spi_host_device_t)host.slot;
    dev.gpio_cs = (gpio_num_t)SD_CS;
    sdspi_dev_handle_t h;
    e = sdspi_host_init_device(&dev, &h);
    if (e != ESP_OK) {
        Serial.printf("{\"sd_raw\":{\"step\":\"device\",\"err\":%d}}\n", (int)e);
        spi_bus_free((spi_host_device_t)host.slot);
        return;
    }
    host.slot = h;
    sdmmc_card_t card;
    e = sdmmc_card_init(&host, &card);
    if (e != ESP_OK) {
        Serial.printf("{\"sd_raw\":{\"step\":\"card_init\",\"err\":%d}}\n", (int)e);
    } else {
        static uint8_t sec[512];
        memset(sec, 0xEE, sizeof(sec));
        esp_err_t r = sdmmc_read_sectors(&card, sec, 0, 1);
        unsigned zeros = 0, ffs = 0;
        for (int i = 0; i < 512; i++) { zeros += sec[i] == 0; ffs += sec[i] == 0xFF; }
        Serial.printf("{\"sd_raw\":{\"card_init\":\"ok\",\"sectors\":%u,\"read0\":%d,\"sig\":\"%02x%02x\","
                      "\"zeros\":%u,\"ff\":%u,\"first16\":\"",
                      (unsigned)card.csd.capacity, (int)r, sec[510], sec[511], zeros, ffs);
        for (int i = 0; i < 16; i++) Serial.printf("%02x", sec[i]);
        Serial.print("\",\"part1\":\"");
        for (int i = 446; i < 462; i++) Serial.printf("%02x", sec[i]);
        Serial.println("\"}}");
    }
    sdspi_host_remove_device(h);
    spi_bus_free((spi_host_device_t)host.slot);
}
