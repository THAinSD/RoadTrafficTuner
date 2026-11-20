// Road Traffic Tuner - core logic (no X-Plane SDK dependencies).

#include "rtt_core.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <locale>
#include <set>
#include <sstream>
#include <system_error>

namespace rtt {

const std::array<FamilyInfo, kFamilyCount> kFamilies = {{
    {"local", "Local", "Residential, minor streets", 5000, 6999, 0.1},
    {"secondary", "Secondary", "Collector roads", 3000, 4999, 0.4},
    {"primary", "Primary", "Arterials", 1000, 2999, 0.7},
    {"highway", "Highway", "Freeways, ramps", 10000, 13999, 1.0},
}};

const std::array<NetworkInfo, kNetworkCount> kNetworks = {{
    {"world", "Rest of the world", "roads.net", "lib/g10/roads.net"},
    {"europe", "Europe", "roads_EU.net", "lib/g10/roads_EU.net"},
}};

Multipliers default_multipliers() {
    Multipliers m{};
    for (int i = 0; i < kFamilyCount; ++i) m[i] = kFamilies[i].default_mult;
    return m;
}

NetworkSet all_networks() {
    NetworkSet s{};
    s.fill(true);
    return s;
}

fs::path Paths::roads_dir() const {
    return xplane_root / "Resources" / "default scenery" / "1000 roads";
}
fs::path Paths::net_file(Network n) const { return roads_dir() / kNetworks[n].file; }
fs::path Paths::custom_scenery() const { return xplane_root / "Custom Scenery"; }
fs::path Paths::pack_dir() const { return custom_scenery() / kPackName; }

namespace {

const char* kSettingsFile = "rtt_settings.txt";
const char* kTempDir = "Road_Traffic_Tuner_building";
const char* kOldDir = "Road_Traffic_Tuner_old";

struct Token {
    size_t start;
    size_t len;
};

// Split text into whitespace-separated tokens, remembering positions.
std::vector<Token> tokenize(const std::string& s, size_t begin, size_t end) {
    std::vector<Token> out;
    size_t i = begin;
    while (i < end) {
        while (i < end && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
        if (i >= end) break;
        size_t j = i;
        while (j < end && !std::isspace(static_cast<unsigned char>(s[j]))) ++j;
        out.push_back({i, j - i});
        i = j;
    }
    return out;
}

// Locale-independent number parsing and formatting.
bool parse_double(const std::string& s, double& out) {
    std::istringstream is(s);
    is.imbue(std::locale::classic());
    is >> out;
    return !is.fail() && is.eof();
}

bool parse_int(const std::string& s, int& out) {
    std::istringstream is(s);
    is.imbue(std::locale::classic());
    is >> out;
    return !is.fail();
}

std::string fmt_ratio(double v) {
    std::ostringstream os;
    os.imbue(std::locale::classic());
    os << std::fixed << std::setprecision(8) << v;
    std::string s = os.str();
    if (s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    return s.empty() ? "0" : s;
}

std::string fmt_mult(double v) {
    std::ostringstream os;
    os.imbue(std::locale::classic());
    os << std::fixed << std::setprecision(2) << v;
    return os.str();
}

int family_of(int road_type) {
    for (int i = 0; i < kFamilyCount; ++i)
        if (road_type >= kFamilies[i].first_type && road_type <= kFamilies[i].last_type)
            return i;
    return -1;
}

std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

// Texture files an OBJ uses, as paths relative to its folder.
std::vector<std::string> obj_textures(const fs::path& obj) {
    std::vector<std::string> out;
    std::string text;
    if (!read_file(obj, text)) return out;
    std::istringstream in(text);
    std::string line;
    static const std::set<std::string> keys = {
        "TEXTURE", "TEXTURE_LIT", "TEXTURE_NORMAL", "TEXTURE_DRAPED",
        "TEXTURE_DRAPED_NORMAL"};
    while (std::getline(in, line)) {
        std::string t = trim(line);
        size_t sp = t.find_first_of(" \t");
        if (sp == std::string::npos) continue;
        if (!keys.count(t.substr(0, sp))) continue;
        std::string value = trim(t.substr(sp));
        if (!value.empty()) out.push_back(value);
    }
    return out;
}

// Copy rel (relative to src_root) into dst_root, with X-Plane's .dds stand-in
// for .png. Returns number of files copied (0 = missing).
int copy_with_standin(const fs::path& src_root, const fs::path& dst_root,
                      const fs::path& rel, std::error_code& ec) {
    std::vector<fs::path> candidates{rel};
    if (lower(rel.extension().u8string()) == ".png") {
        fs::path dds = rel;
        dds.replace_extension(".dds");
        candidates.push_back(dds);
    }
    int copied = 0;
    for (const auto& c : candidates) {
        fs::path src = src_root / c;
        if (!fs::is_regular_file(src)) continue;
        fs::path dst = dst_root / c;
        fs::create_directories(dst.parent_path(), ec);
        if (ec) return copied;
        fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
        if (ec) return copied;
        ++copied;
    }
    return copied;
}

bool copy_tree(const fs::path& from, const fs::path& to, std::error_code& ec) {
    fs::copy(from, to, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    return !ec;
}

std::string library_txt(const NetworkSet& nets) {
    std::ostringstream os;
    os << "A\n800\nLIBRARY\n\n"
       << "# Generated by " << kToolName << " " << kToolVersion << ".\n"
       << "# Replaces X-Plane's default road files with copies whose car spawn\n"
       << "# ratios are scaled per road family.\n"
       << "# Remove it from the plugin window, or delete this folder, to undo.\n";
    for (int n = 0; n < kNetworkCount; ++n)
        if (nets[n])
            os << "EXPORT " << kNetworks[n].virtual_path << " " << kNetworks[n].file << "\n";
    return os.str();
}

std::string readme_txt(const Multipliers& m, const NetworkSet& nets) {
    std::ostringstream os;
    os << kToolName << " override pack\n"
       << std::string(std::string(kToolName).size() + 14, '=') << "\n\n"
       << "Built by the " << kToolName << " plugin, version " << kToolVersion << ".\n\n"
       << "Road files replaced:\n";
    for (int n = 0; n < kNetworkCount; ++n)
        if (nets[n]) os << "  " << kNetworks[n].file << " (" << kNetworks[n].label << ")\n";
    os << "\nTraffic multipliers (1.00 = X-Plane default):\n";
    for (int i = 0; i < kFamilyCount; ++i)
        os << "  " << std::left << std::setw(10) << kFamilies[i].label << " x"
           << fmt_mult(m[i]) << "\n";
    os << "\nThis pack holds edited copies of X-Plane's default road files in which\n"
          "only the car spawn ratios were changed, plus copies of the objects and\n"
          "textures those files refer to by relative path.\n\n"
          "To undo: open Plugins > Road Traffic Tuner > Open... and press Remove\n"
          "pack, or delete this folder. Restart X-Plane afterwards.\n";
    return os.str();
}

std::string settings_txt(const Multipliers& m, const NetworkSet& nets,
                         const std::array<std::string, kNetworkCount>& fps) {
    std::ostringstream os;
    os.imbue(std::locale::classic());
    os << "# " << kToolName << " settings - written by the plugin\n"
       << "tool_version " << kToolVersion << "\n";
    for (int n = 0; n < kNetworkCount; ++n) {
        os << "network_" << kNetworks[n].key << " " << (nets[n] ? 1 : 0) << "\n";
        if (nets[n]) os << "fingerprint_" << kNetworks[n].key << " " << fps[n] << "\n";
    }
    for (int i = 0; i < kFamilyCount; ++i)
        os << kFamilies[i].key << " " << fmt_mult(m[i]) << "\n";
    return os.str();
}

}  // namespace

// ---- road file processing ---------------------------------------------------

std::string scale_roads_net(const std::string& text, const Multipliers& m,
                            LaneStats* stats) {
    std::string out;
    out.reserve(text.size());
    int road_type = 0;
    bool have_type = false;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t nl = text.find('\n', pos);
        size_t line_end = (nl == std::string::npos) ? text.size() : nl + 1;
        std::string line = text.substr(pos, line_end - pos);
        pos = line_end;

        auto toks = tokenize(line, 0, line.size());
        if (!toks.empty()) {
            std::string head = line.substr(toks[0].start, toks[0].len);
            if (head == "ROAD_TYPE") {
                have_type = toks.size() > 1 &&
                            parse_int(line.substr(toks[1].start, toks[1].len), road_type);
            } else if ((head == "CAR_DRAPED" || head == "CAR_GRADED") && have_type &&
                       toks.size() > 4) {
                int fam = family_of(road_type);
                double old_ratio = 0;
                // tokens: 0 CAR_x, 1 reverse, 2 offset, 3 speed, 4 ratio, 5 type
                if (fam >= 0 &&
                    parse_double(line.substr(toks[4].start, toks[4].len), old_ratio)) {
                    double new_ratio = old_ratio * m[fam];
                    if (stats) (*stats)[fam].emplace_back(old_ratio, new_ratio);
                    if (m[fam] != 1.0)
                        line.replace(toks[4].start, toks[4].len, fmt_ratio(new_ratio));
                }
            }
        }
        out += line;
    }
    return out;
}

std::vector<std::string> validate(const LaneStats& stats) {
    std::vector<std::string> problems;
    for (int i = 0; i < kFamilyCount; ++i)
        if (stats[i].empty())
            problems.push_back(std::string("No car lanes found for the ") +
                               kFamilies[i].label + " road family.");
    return problems;
}

std::vector<std::string> referenced_files(const std::string& text) {
    std::set<std::string> found;
    for (const auto& t : tokenize(text, 0, text.size())) {
        if (text.compare(t.start, 8, "objects/") == 0 ||
            text.compare(t.start, 9, "textures/") == 0)
            found.insert(text.substr(t.start, t.len));
    }
    return {found.begin(), found.end()};
}

std::string typical_spacing(const std::vector<std::pair<double, double>>& lanes,
                            bool scaled) {
    std::vector<double> v;
    for (const auto& l : lanes) {
        double r = scaled ? l.second : l.first;
        if (l.first > 0 && r > 0) v.push_back(r);
    }
    if (v.empty()) return "no cars";
    std::sort(v.begin(), v.end());
    double median = v.size() % 2 ? v[v.size() / 2]
                                 : 0.5 * (v[v.size() / 2 - 1] + v[v.size() / 2]);
    return std::to_string(std::lround(1.0 / median)) + " m";
}

// ---- files ------------------------------------------------------------------

bool read_file(const fs::path& p, std::string& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

bool write_file(const fs::path& p, const std::string& data) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(data.data(), static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(f);
}

std::string fingerprint(const fs::path& p) {
    std::string data;
    if (!read_file(p, data)) return "";
    uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : data) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    std::ostringstream os;
    os << std::hex << std::setw(16) << std::setfill('0') << h;
    return os.str();
}

// ---- pack -------------------------------------------------------------------

bool pack_installed(const Paths& p) {
    std::error_code ec;
    return fs::is_directory(p.pack_dir(), ec);
}

bool load_settings(const Paths& p, PackSettings& out) {
    std::string text;
    if (!read_file(p.pack_dir() / kSettingsFile, text)) return false;
    out = PackSettings{};
    std::istringstream in(text);
    std::string line;
    std::array<bool, kFamilyCount> seen{};
    bool any_network_key = false;
    while (std::getline(in, line)) {
        std::istringstream ls(trim(line));
        ls.imbue(std::locale::classic());
        std::string key;
        if (!(ls >> key) || key[0] == '#') continue;
        if (key == "tool_version") {
            ls >> out.tool_version;
        } else if (key == "roads_fingerprint") {  // early test builds: world only
            ls >> out.fingerprints[kWorld];
        }
        for (int n = 0; n < kNetworkCount; ++n) {
            if (key == std::string("network_") + kNetworks[n].key) {
                int v = 0;
                ls >> v;
                out.networks[n] = v != 0;
                any_network_key = true;
            } else if (key == std::string("fingerprint_") + kNetworks[n].key) {
                ls >> out.fingerprints[n];
            }
        }
        for (int i = 0; i < kFamilyCount; ++i) {
            double v;
            if (key == kFamilies[i].key && (ls >> v)) {
                out.mults[i] = v;
                seen[i] = true;
            }
        }
    }
    if (!any_network_key) out.networks[kWorld] = true;  // early test-build packs
    return std::all_of(seen.begin(), seen.end(), [](bool b) { return b; });
}

std::vector<Network> stale_networks(const Paths& p, const PackSettings& s) {
    std::vector<Network> out;
    for (int n = 0; n < kNetworkCount; ++n)
        if (s.networks[n] && s.fingerprints[n] != fingerprint(p.net_file(Network(n))))
            out.push_back(Network(n));
    return out;
}

std::vector<Conflict> find_conflicts(const Paths& p) {
    std::vector<Conflict> hits;
    std::error_code ec;
    fs::directory_iterator it(p.custom_scenery(), ec), end;
    for (; !ec && it != end; it.increment(ec)) {
        std::string name = it->path().filename().u8string();
        if (name == kPackName) continue;
        std::string text;
        if (!read_file(it->path() / "library.txt", text)) continue;
        std::array<bool, kNetworkCount> found{};
        std::istringstream in(text);
        std::string line;
        while (std::getline(in, line)) {
            std::istringstream ls(line);
            std::string cmd, tok;
            if (!(ls >> cmd)) continue;
            if (cmd.rfind("EXPORT", 0) != 0 || cmd == "EXPORT_BACKUP") continue;
            while (ls >> tok)
                for (int n = 0; n < kNetworkCount; ++n)
                    if (lower(tok) == lower(kNetworks[n].virtual_path)) found[n] = true;
        }
        for (int n = 0; n < kNetworkCount; ++n)
            if (found[n]) hits.push_back({name, Network(n)});
    }
    std::sort(hits.begin(), hits.end(), [](const Conflict& a, const Conflict& b) {
        return a.pack != b.pack ? a.pack < b.pack : a.network < b.network;
    });
    return hits;
}

BuildResult build_pack(const Paths& p, const Multipliers& m, const NetworkSet& nets) {
    BuildResult r;
    std::error_code ec;

    if (std::none_of(nets.begin(), nets.end(), [](bool b) { return b; })) {
        r.error = "No road network selected.";
        return r;
    }

    // Read, scale and check every selected road file before touching anything.
    std::array<std::string, kNetworkCount> originals, scaled, fps;
    for (int n = 0; n < kNetworkCount; ++n) {
        if (!nets[n]) continue;
        if (!read_file(p.net_file(Network(n)), originals[n])) {
            r.error = "Could not read " + p.net_file(Network(n)).u8string();
            return r;
        }
        scaled[n] = scale_roads_net(originals[n], m, &r.stats[n]);
        auto problems = validate(r.stats[n]);
        if (!problems.empty()) {
            r.error = std::string("X-Plane's ") + kNetworks[n].file +
                      " does not have the layout this plugin expects, so nothing was changed.";
            for (const auto& s : problems) r.error += "\n" + s;
            return r;
        }
        fps[n] = fingerprint(p.net_file(Network(n)));
    }

    const fs::path tmp = p.xplane_root / kTempDir;
    const fs::path old = p.xplane_root / kOldDir;
    fs::remove_all(tmp, ec);
    fs::remove_all(old, ec);
    ec.clear();
    fs::create_directories(tmp, ec);
    if (ec) {
        r.error = "Could not create " + tmp.u8string() + ": " + ec.message();
        return r;
    }

    // Collect files: those the road files name, plus textures used by those OBJs.
    std::vector<fs::path> needed;
    std::set<std::string> seen;
    auto add = [&](const fs::path& rel) {
        fs::path n = rel.lexically_normal();
        std::string s = n.generic_string();
        if (s.empty() || s.rfind("..", 0) == 0) return;  // stay inside 1000 roads
        if (seen.insert(s).second) needed.push_back(n);
    };
    for (int n = 0; n < kNetworkCount; ++n)
        if (nets[n])
            for (const auto& f : referenced_files(originals[n])) add(fs::path(f));
    for (size_t i = 0; i < needed.size(); ++i) {
        if (lower(needed[i].extension().u8string()) != ".obj") continue;
        for (const auto& t : obj_textures(p.roads_dir() / needed[i]))
            add(needed[i].parent_path() / fs::path(t));
    }

    for (const auto& rel : needed) {
        int n = copy_with_standin(p.roads_dir(), tmp, rel, ec);
        if (ec) {
            r.error = "Could not copy " + rel.generic_string() + ": " + ec.message();
            fs::remove_all(tmp, ec);
            return r;
        }
        if (n == 0) r.missing.push_back(rel.generic_string());
        r.files_copied += n;
    }

    bool ok = write_file(tmp / "library.txt", library_txt(nets)) &&
              write_file(tmp / "README.txt", readme_txt(m, nets)) &&
              write_file(tmp / kSettingsFile, settings_txt(m, nets, fps));
    for (int n = 0; n < kNetworkCount && ok; ++n)
        if (nets[n]) ok = write_file(tmp / kNetworks[n].file, scaled[n]);
    if (!ok) {
        r.error = "Could not write the pack files in " + tmp.u8string();
        fs::remove_all(tmp, ec);
        return r;
    }

    // Swap the new pack into place.
    fs::create_directories(p.custom_scenery(), ec);
    ec.clear();
    const fs::path pack = p.pack_dir();
    if (fs::exists(pack)) {
        fs::rename(pack, old, ec);
        if (ec) {  // e.g. different drives: fall back to delete
            ec.clear();
            fs::remove_all(pack, ec);
            if (ec) {
                r.error = "Could not replace the existing pack: " + ec.message();
                return r;
            }
        }
    }
    fs::rename(tmp, pack, ec);
    if (ec) {  // Custom Scenery on another drive: copy instead of move
        ec.clear();
        if (!copy_tree(tmp, pack, ec)) {
            r.error = "Could not install the pack: " + ec.message();
            std::error_code ec2;
            fs::remove_all(pack, ec2);
            if (fs::exists(old)) fs::rename(old, pack, ec2);  // put the old pack back
            return r;
        }
        fs::remove_all(tmp, ec);
    }
    fs::remove_all(old, ec);
    r.ok = true;
    return r;
}

bool remove_pack(const Paths& p, std::string& error) {
    std::error_code ec;
    fs::remove_all(p.pack_dir(), ec);
    if (ec) {
        error = "Could not remove " + p.pack_dir().u8string() + ": " + ec.message();
        return false;
    }
    return true;
}

}  // namespace rtt
