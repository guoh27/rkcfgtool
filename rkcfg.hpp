#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <algorithm>

#pragma pack(push, 1)
struct RKCfgHeader {
  char magic[4];
  char gap0[18];
  uint8_t length;
  uint32_t begin;
  uint16_t itemSize;
};

struct RKCfgItem {
  uint16_t size;
  char16_t name[40];
  char16_t imagePath[260];
  uint32_t address;
  uint8_t isSelected;
  char gap1[3];
};
#pragma pack(pop)

// Convert a fixed-size UTF-16 buffer to a string without trailing zeros.
inline std::u16string readFixed(const char16_t *buf, size_t len) {
  size_t n = 0;
  while (n < len && buf[n])
    ++n;
  return std::u16string(buf, n);
}

// Copy a string into a fixed-size UTF-16 buffer and pad unused bytes with zeros.
inline void writeFixed(char16_t *dest, size_t len, const std::u16string &s) {
  std::fill_n(dest, len, 0);
  size_t n = std::min(len - 1, s.size());
  std::copy_n(s.data(), n, dest);
}

struct Entry {
  RKCfgItem raw;
  std::u16string name;
  std::u16string path;
  uint32_t address;
  uint8_t selected;
};

bool readRkcfg(const std::string &path, RKCfgHeader &hdr,
               std::vector<Entry> &items);
bool writeRkcfg(const std::string &path, RKCfgHeader hdr,
                const std::vector<Entry> &items);
