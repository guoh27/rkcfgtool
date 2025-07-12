#include <codecvt>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "rkcfg.hpp"
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

// UTF-16LE ⇄ UTF-8 converter
static std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> cvt;

// Convert a user-supplied index argument into a valid position within items.
static std::optional<size_t> parseIndex(const std::vector<Entry> &items,
                                        const char *arg) {
  int idx = std::stoi(arg);
  if (idx == -1) {
    if (items.empty())
      return std::nullopt;
    return items.size() - 1;
  }
  if (idx < 0 || static_cast<size_t>(idx) >= items.size())
    return std::nullopt;
  return static_cast<size_t>(idx);
}

/*--------------------------------------------------------------------
 * 2. Helper functions
 *-------------------------------------------------------------------*/

// Escape a string so it can be safely embedded in JSON output.
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

// Generate a minimal RKCfgHeader for a new configuration file.
static RKCfgHeader createHeader() {
  RKCfgHeader hdr{};
  std::memcpy(hdr.magic, "CFG", 3);
  hdr.begin = sizeof(RKCfgHeader);
  hdr.itemSize = sizeof(RKCfgItem);
  hdr.length = 0;
  return hdr;
}

// Load an existing CFG file into memory.
static bool parseCfg(const std::string &file, RKCfgHeader &hdr,
                     std::vector<Entry> &items) {
  return readRkcfg(file, hdr, items);
}

// Write a modified configuration back to disk.
static bool rebuildAndWrite(const std::string &outFile, RKCfgHeader hdr,
                            const std::vector<Entry> &items) {
  return writeRkcfg(outFile, hdr, items);
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
  --enable   <idx> <1|0>         Set enable flag of entry <idx>
  --json                         Output entries as JSON
  --script                       Output entries as machine readable text
  --create                       Start a new CFG instead of reading one
  -o, --output <file>            Write result to <file>
  -V, --version                  Show rkcfgtool version
  -h, --help                     Show this help message

  <idx> may be -1 to target the last entry
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

  RKCfgHeader hdr{};
  std::vector<Entry> items;
  bool jsonOut = false;
  bool scriptOut = false;
  std::string outFile;
  const std::string inFile = argv[1];

  bool create = false;
  for (int j = 2; j < argc; ++j)
    if (std::string(argv[j]) == "--create")
      create = true;

  if (create) {
    hdr = createHeader();
  } else {
    if (!parseCfg(inFile, hdr, items))
      return 1;
  }

  bool modified = create;
  int i = 2;

  for (; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "--list") {
      // no-op: default action
    } else if (arg == "--set-path" && i + 2 < argc) {
      auto idxOpt = parseIndex(items, argv[++i]);
      std::u16string newP = cvt.from_bytes(argv[++i]);
      if (!idxOpt) {
        std::cerr << "Index out of range\n";
        return 1;
      }
      size_t idx = *idxOpt;
      items[idx].path = std::move(newP);
      writeFixed(items[idx].raw.imagePath, 260, items[idx].path);
      modified = true;
    } else if (arg == "--set-name" && i + 2 < argc) {
      auto idxOpt = parseIndex(items, argv[++i]);
      std::u16string newN = cvt.from_bytes(argv[++i]);
      if (!idxOpt) {
        std::cerr << "Index out of range\n";
        return 1;
      }
      size_t idx = *idxOpt;
      items[idx].name = std::move(newN);
      writeFixed(items[idx].raw.name, 40, items[idx].name);
      modified = true;
    } else if (arg == "--add" && i + 2 < argc) {
      Entry e{};
      e.raw.size = hdr.itemSize;
      e.name = cvt.from_bytes(argv[++i]);
      e.path = cvt.from_bytes(argv[++i]);
      writeFixed(e.raw.name, 40, e.name);
      writeFixed(e.raw.imagePath, 260, e.path);
      items.push_back(std::move(e));
      modified = true;
    } else if (arg == "--del" && i + 1 < argc) {
      auto idxOpt = parseIndex(items, argv[++i]);
      if (!idxOpt) {
        std::cerr << "Index out of range\n";
        return 1;
      }
      size_t idx = *idxOpt;
      items.erase(items.begin() + idx);
      modified = true;
    } else if (arg == "--enable" && i + 2 < argc) {
      auto idxOpt = parseIndex(items, argv[++i]);
      int flag = std::stoi(argv[++i]);
      if (!idxOpt) {
        std::cerr << "Index out of range\n";
        return 1;
      }
      size_t idx = *idxOpt;
      items[idx].selected = flag ? 1 : 0;
      items[idx].raw.isSelected = items[idx].selected;
      modified = true;
    } else if (arg == "--json") {
      jsonOut = true;
    } else if (arg == "--script") {
      scriptOut = true;
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
                << jsonEscape(cvt.to_bytes(items[i].path))
                << "\",\"enabled\":" << static_cast<int>(items[i].selected)
                << "}";
    }
    std::cout << "\n]\n";
  } else if (scriptOut) {
    std::cout << "index,enabled,name,path\n";
    for (size_t i = 0; i < items.size(); ++i)
      std::cout << i << "," << static_cast<int>(items[i].selected) << ","
                << cvt.to_bytes(items[i].name) << ","
                << cvt.to_bytes(items[i].path) << '\n';
  } else {
    std::cout << "=== Entry list (" << items.size() << ") ===\n";
    for (size_t i = 0; i < items.size(); ++i)
      std::cout << std::setw(2) << i << " "
                << static_cast<int>(items[i].selected) << " "
                << cvt.to_bytes(items[i].name) << " "
                << cvt.to_bytes(items[i].path) << '\n';
  }

  if (modified && outFile.empty())
    outFile = inFile;

  // Write back if needed
  if (!outFile.empty())
    return rebuildAndWrite(outFile, hdr, items) ? 0 : 1;

  return 0;
}
