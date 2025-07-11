#include <codecvt>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

#include "version.hpp"
/*--------------------------------------------------------------------
 * 1. File layout (deduced from sample)
 *    ┌────────────┐  Header (magic "CFG\0" + timestamp)
 *    │  Header    │
 *    ├────────────┤  Reserved zero area
 *    │  zeros …   │
 *    ├────────────┤  Directory entries (UTF-16LE strings)
 *    │ name\0     │
 *    │ path\0     │
 *    │ …          │
 *    └────────────┘  16×0 terminator
 *-------------------------------------------------------------------*/

#pragma pack(push, 1)
struct Header {
  char magic[4];             // "CFG\0"
  uint16_t day, year, month; // little-endian
  uint16_t hour, minute, second;
};
#pragma pack(pop)

struct Entry {
  std::u16string name;
  std::u16string path;
  std::vector<uint8_t> gap;
};

// UTF-16LE ⇄ UTF-8 converter
static std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> cvt;

/*--------------------------------------------------------------------
 * 2. Helper functions
 *-------------------------------------------------------------------*/
static std::u16string readU16Z(const std::vector<uint8_t> &buf, size_t &pos) {
  size_t start = pos;
  while (pos + 1 < buf.size()) { // search for 0x0000
    if (buf[pos] == 0 && buf[pos + 1] == 0) {
      pos += 2;
      break;
    }
    pos += 2;
  }
  const char16_t *p = reinterpret_cast<const char16_t *>(&buf[start]);
  return std::u16string(p, (pos - start) / 2 - 1); // drop terminator
}

static void writeU16Z(std::vector<uint8_t> &out, const std::u16string &s) {
  const uint8_t *p = reinterpret_cast<const uint8_t *>(s.data());
  out.insert(out.end(), p, p + s.size() * 2);
  out.push_back(0);
  out.push_back(0); // trailing 0x0000
}

static std::string jsonEscape(const std::string &s) {
  std::string out;
  for (char c : s) {
    switch (c) {
    case '\\':
      out += "\\\\";
      break;
    case '"':
      out += "\\\"";
      break;
    case '\b':
      out += "\\b";
      break;
    case '\f':
      out += "\\f";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      if (static_cast<unsigned char>(c) < 0x20) {
        char buf[7];
        std::snprintf(buf, sizeof(buf), "\\u%04x",
                      static_cast<unsigned char>(c));
        out += buf;
      } else {
        out += c;
      }
    }
  }
  return out;
}

// Find the next plausible UTF-16LE ASCII string beginning at or after `pos`.
// Returns the starting offset or `std::string::npos` if none found.
static size_t findAsciiU16(const std::vector<uint8_t> &buf, size_t pos) {
  auto isAscii = [](uint8_t b) { return b >= 0x20 && b <= 0x7e; };
  for (; pos + 3 < buf.size(); ++pos) {
    if (buf[pos + 1] == 0 && isAscii(buf[pos]) && buf[pos + 3] == 0 &&
        (isAscii(buf[pos + 2]) || buf[pos + 2] == 0)) {
      return pos;
    }
  }
  return std::string::npos;
}

static std::vector<uint8_t> createPrefix() {
  Header hdr{};
  std::memcpy(hdr.magic, "CFG", 3);
  std::time_t t = std::time(nullptr);
  std::tm tm = *std::localtime(&t);
  hdr.day = tm.tm_mday;
  hdr.year = tm.tm_year + 1900;
  hdr.month = tm.tm_mon + 1;
  hdr.hour = tm.tm_hour;
  hdr.minute = tm.tm_min;
  hdr.second = tm.tm_sec;

  std::vector<uint8_t> buf(sizeof(Header) + 2, 0);
  std::memcpy(buf.data(), &hdr, sizeof(hdr));
  return buf;
}

/*--------------------------------------------------------------------
 * 3. Parse existing CFG file
 *-------------------------------------------------------------------*/
static bool parseCfg(const std::string &file, std::vector<uint8_t> &raw,
                     std::vector<uint8_t> &prefix, std::vector<Entry> &items,
                     std::vector<uint8_t> &suffix) {
  std::ifstream in(file, std::ios::binary);
  if (!in) {
    std::cerr << "Cannot open " << file << '\n';
    return false;
  }

  raw.assign(std::istreambuf_iterator<char>(in),
             std::istreambuf_iterator<char>());

  if (raw.size() < sizeof(Header)) {
    std::cerr << "File too small\n";
    return false;
  }
  auto *hdr = reinterpret_cast<const Header *>(raw.data());
  if (std::string(hdr->magic, 3) != "CFG") {
    std::cerr << "Bad magic number\n";
    return false;
  }

  // Copy header and locate first UTF-16 entry heuristically
  size_t pos = sizeof(Header);
  while (pos < raw.size() && raw[pos] == 0)
    ++pos; // skip zeros
  if (pos & 1)
    ++pos; // ensure 2‑byte alignment
  pos = findAsciiU16(raw, pos);
  if (pos == std::string::npos) {
    std::cerr << "No entries found in " << file << '\n';
    return false;
  }
  prefix.assign(raw.begin(), raw.begin() + pos);
  size_t lastEnd = pos;

  // Extract all UTF-16 strings along with their byte ranges and gaps
  std::vector<std::u16string> strs;
  std::vector<std::pair<size_t, size_t>> ranges;
  std::vector<std::vector<uint8_t>> gaps;
  auto validAscii = [](char16_t c) { return c >= 0x21 && c < 0x7f; };
  while (pos != std::string::npos && pos + 1 < raw.size()) {
    size_t start = pos;
    std::u16string s = readU16Z(raw, pos);
    size_t end = pos;
    lastEnd = pos;
    bool ok = !s.empty();
    bool hasAlnum = false;
    for (char16_t ch : s) {
      if (!validAscii(ch)) {
        ok = false;
        break;
      }
      if (std::isalnum(static_cast<unsigned char>(ch)))
        hasAlnum = true;
    }
    if (ok && hasAlnum) {
      strs.push_back(std::move(s));
      ranges.emplace_back(start, end);
    }
    size_t next = findAsciiU16(raw, pos);
    if (ok && hasAlnum) {
      gaps.emplace_back(raw.begin() + pos, next == std::string::npos
                                               ? raw.end()
                                               : raw.begin() + next);
    }
    pos = next;
  }

  if (!gaps.empty()) {
    suffix = std::move(gaps.back());
    gaps.pop_back();
  } else {
    suffix.clear();
  }

  // Pair them as name/path entries. Some files may omit paths for certain
  // names. Heuristically treat a string as a path only if it looks like one.
  // However, configs generated by rkcfgtool itself always store entries in
  // strict name/path order. If heuristic detection yields no paths at all,
  // fall back to pairing consecutively.
  bool forcePair = false;
  {
    size_t pathCount = 0;
    for (size_t i = 1; i < strs.size(); ++i) {
      const auto &cand = strs[i];
      if (cand.find(u'\\') != std::u16string::npos ||
          cand.find(u'/') != std::u16string::npos ||
          cand.find(u'.') != std::u16string::npos) {
        ++pathCount;
      }
    }
    if (pathCount == 0 && (strs.size() % 2 == 0))
      forcePair = true;
  }

  for (size_t i = 0, g = 0; i < strs.size(); ++i, ++g) {
    Entry e;
    e.name = std::move(strs[i]);
    if (i + 1 < strs.size()) {
      const auto &cand = strs[i + 1];
      bool looksLikePath = cand.find(u'\\') != std::u16string::npos ||
                           cand.find(u'/') != std::u16string::npos ||
                           cand.find(u'.') != std::u16string::npos;
      if (forcePair || looksLikePath) {
        e.path = cand;
        ++i;
        ++g;
      }
    }
    if (g < gaps.size())
      e.gap = std::move(gaps[g]);
    items.push_back(std::move(e));
  }

  return true;
}

/*--------------------------------------------------------------------
 * 4. Rebuild & write CFG
 *-------------------------------------------------------------------*/
static bool rebuildAndWrite(const std::string &outFile,
                            const std::vector<uint8_t> &prefix,
                            const std::vector<Entry> &items,
                            const std::vector<uint8_t> &suffix) {
  std::vector<uint8_t> out(prefix); // header + zeros
  for (const auto &e : items) {
    writeU16Z(out, e.name);
    writeU16Z(out, e.path);
    out.insert(out.end(), e.gap.begin(), e.gap.end());
  }
  out.insert(out.end(), suffix.begin(), suffix.end());

  std::ofstream ofs(outFile, std::ios::binary);
  if (!ofs) {
    std::cerr << "Cannot write " << outFile << '\n';
    return false;
  }

  ofs.write(reinterpret_cast<const char *>(out.data()), out.size());
  std::cout << "Written " << outFile << " (" << out.size() << " bytes)\n";
  return true;
}

/*--------------------------------------------------------------------
 * 5. CLI help
 *-------------------------------------------------------------------*/
static void showHelp() {
  std::cout <<
      R"(Usage:
  rkcfgtool <cfg> [--create] [actions…] [-o <output.cfg>]
  rkcfgtool --help | --version

Actions (may repeat; executed in order):
  --list                         List entries (default)
  --set-path <idx> <newPath>     Change path of entry <idx>
  --set-name <idx> <newName>     Change name of entry <idx>
  --add      <name> <path>       Append a new entry
  --del      <idx>               Delete entry <idx>
  --disable  <idx>               Clear path of entry <idx>
  --json                         Output entries as JSON
  --create                       Start a new CFG instead of reading one
  -o, --output <file>            Write result to <file>
  -V, --version                  Show rkcfgtool version
  -h, --help                     Show this help message
)";
}

/*--------------------------------------------------------------------
 * 6. Main entry
 *-------------------------------------------------------------------*/
int main(int argc, char *argv[]) {
  for (int j = 1; j < argc; ++j) {
    std::string arg = argv[j];
    if (arg == "--help" || arg == "-h") {
      showHelp();
      return 0;
    }
    if (arg == "--version" || arg == "-V") {
      std::cout << "rkcfgtool " << RKCFGTOOL_VERSION << '\n';
      return 0;
    }
  }

  if (argc < 2) {
    showHelp();
    return 0;
  }

  std::vector<uint8_t> raw, prefix, suffix;
  std::vector<Entry> items;
  bool jsonOut = false;
  std::string outFile;
  const std::string inFile = argv[1];

  bool create = false;
  for (int j = 2; j < argc; ++j)
    if (std::string(argv[j]) == "--create")
      create = true;

  if (create) {
    prefix = createPrefix();
    suffix.assign(16, 0);
  } else {
    if (!parseCfg(inFile, raw, prefix, items, suffix))
      return 1;
  }

  bool modified = create;
  int i = 2;

  for (; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "--list") {
      // no-op: default action
    } else if (arg == "--set-path" && i + 2 < argc) {
      size_t idx = std::stoul(argv[++i]);
      std::u16string newP = cvt.from_bytes(argv[++i]);
      if (idx >= items.size()) {
        std::cerr << "Index out of range\n";
        return 1;
      }
      items[idx].path = std::move(newP);
      modified = true;
    } else if (arg == "--set-name" && i + 2 < argc) {
      size_t idx = std::stoul(argv[++i]);
      std::u16string newN = cvt.from_bytes(argv[++i]);
      if (idx >= items.size()) {
        std::cerr << "Index out of range\n";
        return 1;
      }
      items[idx].name = std::move(newN);
      modified = true;
    } else if (arg == "--add" && i + 2 < argc) {
      Entry e;
      e.name = cvt.from_bytes(argv[++i]);
      e.path = cvt.from_bytes(argv[++i]);
      items.push_back(std::move(e));
      modified = true;
    } else if (arg == "--del" && i + 1 < argc) {
      size_t idx = std::stoul(argv[++i]);
      if (idx >= items.size()) {
        std::cerr << "Index out of range\n";
        return 1;
      }
      items.erase(items.begin() + idx);
      modified = true;
    } else if (arg == "--disable" && i + 1 < argc) {
      size_t idx = std::stoul(argv[++i]);
      if (idx >= items.size()) {
        std::cerr << "Index out of range\n";
        return 1;
      }
      items[idx].path.clear();
      modified = true;
    } else if (arg == "--json") {
      jsonOut = true;
    } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
      outFile = argv[++i];
    } else if (arg == "--create") {
      // already handled
    } else {
      std::cerr << "Unknown or incomplete option: " << arg << '\n';
      return 1;
    }
  }

  // Show directory entries
  if (jsonOut) {
    std::cout << "[\n";
    for (size_t i = 0; i < items.size(); ++i) {
      if (i)
        std::cout << ",\n";
      std::cout << "  {\"index\":" << i << ",\"name\":\""
                << jsonEscape(cvt.to_bytes(items[i].name)) << "\",\"path\":\""
                << jsonEscape(cvt.to_bytes(items[i].path)) << "\"}";
    }
    std::cout << "\n]\n";
  } else {
    std::cout << "=== Entry list (" << items.size() << ") ===\n";
    for (size_t i = 0; i < items.size(); ++i)
      std::cout << std::setw(2) << i << " " << cvt.to_bytes(items[i].name)
                << " " << cvt.to_bytes(items[i].path) << '\n';
  }

  if (modified && outFile.empty())
    outFile = inFile;

  // Write back if needed
  if (!outFile.empty())
    return rebuildAndWrite(outFile, prefix, items, suffix) ? 0 : 1;

  return 0;
}
