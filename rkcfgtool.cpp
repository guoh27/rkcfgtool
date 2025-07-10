#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <codecvt>
#include <locale>
#include <cstdint>
#include <iomanip>
#include <cstring>
#include <sstream>

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
    char     magic[4];          // "CFG\0"
    uint16_t day, year, month;  // little-endian
    uint16_t hour, minute, second;
};
#pragma pack(pop)

struct Entry {
    std::u16string name;
    std::u16string path;
};

// UTF-16LE ⇄ UTF-8 converter
static std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> cvt;

/*--------------------------------------------------------------------
 * 2. Helper functions
 *-------------------------------------------------------------------*/
static std::u16string readU16Z(const std::vector<uint8_t>& buf, size_t& pos)
{
    size_t start = pos;
    while (pos + 1 < buf.size()) {                 // search for 0x0000
        if (buf[pos] == 0 && buf[pos + 1] == 0) { pos += 2; break; }
        pos += 2;
    }
    const char16_t* p = reinterpret_cast<const char16_t*>(&buf[start]);
    return std::u16string(p, (pos - start) / 2 - 1); // drop terminator
}

static void writeU16Z(std::vector<uint8_t>& out, const std::u16string& s)
{
    const uint8_t* p = reinterpret_cast<const uint8_t*>(s.data());
    out.insert(out.end(), p, p + s.size() * 2);
    out.push_back(0); out.push_back(0);            // trailing 0x0000
}

/*--------------------------------------------------------------------
 * 3. Parse existing CFG file
 *-------------------------------------------------------------------*/
static bool parseCfg(const std::string& file,
                     std::vector<uint8_t>& raw,
                     std::vector<uint8_t>& prefix,
                     std::vector<Entry>& items)
{
    std::ifstream in(file, std::ios::binary);
    if (!in) { std::cerr << "Cannot open " << file << '\n'; return false; }

    raw.assign(std::istreambuf_iterator<char>(in),
               std::istreambuf_iterator<char>());

    if (raw.size() < sizeof(Header)) {
        std::cerr << "File too small\n"; return false;
    }
    auto* hdr = reinterpret_cast<const Header*>(raw.data());
    if (std::string(hdr->magic, 3) != "CFG") {
        std::cerr << "Bad magic number\n"; return false;
    }

    // Copy header + reserved-zero prefix
    size_t pos = sizeof(Header);
    while (pos < raw.size() && raw[pos] == 0) ++pos; // skip zeros
    if (pos & 1) ++pos;                              // ensure 2‑byte alignment
    prefix.assign(raw.begin(), raw.begin() + pos);

    // Read directory entries
    while (pos + 1 < raw.size()) {
        bool allZero = true;
        for (size_t i = pos; i < std::min(pos + 16, raw.size()); ++i)
            if (raw[i]) { allZero = false; break; }
        if (allZero) break;                          // 16 zeros ⇒ end

        Entry e;
        e.name = readU16Z(raw, pos);

        while (pos + 1 < raw.size() && raw[pos] == 0 && raw[pos + 1] == 0)
            pos += 2;                                // skip padding

        e.path = readU16Z(raw, pos);
        items.push_back(std::move(e));
    }
    return true;
}

/*--------------------------------------------------------------------
 * 4. Rebuild & write CFG
 *-------------------------------------------------------------------*/
static bool rebuildAndWrite(const std::string& outFile,
                            const std::vector<uint8_t>& prefix,
                            const std::vector<Entry>& items)
{
    std::vector<uint8_t> out(prefix);               // header + zeros
    for (const auto& e : items) {
        writeU16Z(out, e.name);
        writeU16Z(out, e.path);
    }
    out.insert(out.end(), 16, 0);                   // terminator

    std::ofstream ofs(outFile, std::ios::binary);
    if (!ofs) { std::cerr << "Cannot write " << outFile << '\n'; return false; }

    ofs.write(reinterpret_cast<const char*>(out.data()), out.size());
    std::cout << "Written " << outFile << " (" << out.size() << " bytes)\n";
    return true;
}

/*--------------------------------------------------------------------
 * 5. CLI help
 *-------------------------------------------------------------------*/
static void showHelp()
{
    std::cout <<
R"(Usage:
  cfgtool <cfg> [actions…] [-o <output.cfg>]

Actions (may repeat; executed in order):
  --list                         List entries (default)
  --set-path <idx> <newPath>     Change path of entry <idx>
  --add      <name> <path>       Append a new entry
  --del      <idx>               Delete entry <idx>
)";
}

/*--------------------------------------------------------------------
 * 6. Main entry
 *-------------------------------------------------------------------*/
int main(int argc, char* argv[])
{
    if (argc < 2) { showHelp(); return 0; }
    const std::string inFile = argv[1];

    // Parse original CFG
    std::vector<uint8_t> raw, prefix;
    std::vector<Entry> items;
    if (!parseCfg(inFile, raw, prefix, items)) return 1;

    // Process CLI arguments
    std::string outFile;
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--list") {
            // no‑op: default action
        }
        else if (arg == "--set-path" && i + 2 < argc) {
            size_t idx = std::stoul(argv[++i]);
            std::u16string newP = cvt.from_bytes(argv[++i]);
            if (idx >= items.size()) { std::cerr << "Index out of range\n"; return 1; }
            items[idx].path = std::move(newP);
        }
        else if (arg == "--add" && i + 2 < argc) {
            Entry e;
            e.name = cvt.from_bytes(argv[++i]);
            e.path = cvt.from_bytes(argv[++i]);
            items.push_back(std::move(e));
        }
        else if (arg == "--del" && i + 1 < argc) {
            size_t idx = std::stoul(argv[++i]);
            if (idx >= items.size()) { std::cerr << "Index out of range\n"; return 1; }
            items.erase(items.begin() + idx);
        }
        else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            outFile = argv[++i];
        }
        else {
            std::cerr << "Unknown or incomplete option: " << arg << '\n';
            return 1;
        }
    }

    // Show directory entries
    std::cout << "=== Entry list (" << items.size() << ") ===\n";
    for (size_t i = 0; i < items.size(); ++i)
        std::cout << std::setw(2) << i << ": "
                  << cvt.to_bytes(items[i].name) << "  ->  "
                  << cvt.to_bytes(items[i].path) << '\n';

    // Write back if requested
    if (!outFile.empty())
        return rebuildAndWrite(outFile, prefix, items) ? 0 : 1;

    return 0;
}
