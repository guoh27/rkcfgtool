#include "rkcfg.hpp"
#include <algorithm>
#include <codecvt>
#include <cstring>
#include <fstream>
#include <iostream>
#include <locale>

// Convert between UTF-8 and UTF-16LE strings used in the configuration.
static std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> cvt;


// Read a Rockchip CFG file from disk and fill hdr and items. Returns true on success.
bool readRkcfg(const std::string &path, RKCfgHeader &hdr,
               std::vector<Entry> &items) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    std::cerr << "Cannot open " << path << "\n";
    return false;
  }
  in.read(reinterpret_cast<char *>(&hdr), sizeof(hdr));
  if (std::strncmp(hdr.magic, "CFG", 3) != 0) {
    std::cerr << "Bad magic number\n";
    return false;
  }
  if (hdr.itemSize != sizeof(RKCfgItem)) {
    std::cerr << "Unsupported item size\n";
    return false;
  }
  items.clear();
  for (size_t i = 0; i < hdr.length; ++i) {
    RKCfgItem item{};
    in.read(reinterpret_cast<char *>(&item), sizeof(item));
    if (item.size != hdr.itemSize) {
      std::cerr << "Item size mismatch\n";
      return false;
    }
    Entry e{};
    e.raw = item;
    e.name = readFixed(item.name, 40);
    e.path = readFixed(item.imagePath, 260);
    e.address = item.address;
    e.selected = item.isSelected;
    items.push_back(std::move(e));
  }
  return true;
}

// Write hdr and items to a Rockchip CFG file. Returns true when the file is saved successfully.
bool writeRkcfg(const std::string &path, RKCfgHeader hdr,
                const std::vector<Entry> &items) {
  hdr.length = static_cast<uint8_t>(items.size());
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    std::cerr << "Cannot write " << path << "\n";
    return false;
  }
  out.write(reinterpret_cast<const char *>(&hdr), sizeof(hdr));
  for (const auto &e : items) {
    RKCfgItem item = e.raw;
    item.size = hdr.itemSize;
    item.address = e.address;
    item.isSelected = e.selected;
    out.write(reinterpret_cast<const char *>(&item), sizeof(item));
  }
  std::cout << "Written " << path << " ("
            << sizeof(hdr) + sizeof(RKCfgItem) * items.size() << " bytes)\n";
  return true;
}
