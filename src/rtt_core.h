// Road Traffic Tuner - core logic (no X-Plane SDK dependencies).
//
// Builds a Custom Scenery override pack that replaces X-Plane's default road
// networks (lib/g10/roads.net for most of the world, lib/g10/roads_EU.net for
// Europe) with copies whose car spawn ratios are scaled per road family. Only
// the files those road files use by relative path are copied into the pack;
// everything they use by library path is untouched.

#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace rtt {

namespace fs = std::filesystem;

inline const char* kToolName = "Road Traffic Tuner";
inline const char* kToolVersion = "1.0";
inline const char* kPackName = "zzz_Road_Traffic_Tuner";

// ---- road families ----------------------------------------------------------

enum Family { kLocal = 0, kSecondary, kPrimary, kHighway, kFamilyCount };

struct FamilyInfo {
    const char* key;
    const char* label;
    const char* description;
    int first_type;  // inclusive ROAD_TYPE id range
    int last_type;
    double default_mult;
};

extern const std::array<FamilyInfo, kFamilyCount> kFamilies;  // order = Family

using Multipliers = std::array<double, kFamilyCount>;
Multipliers default_multipliers();

// ---- road networks (road files) ---------------------------------------------

enum Network { kWorld = 0, kEurope, kNetworkCount };

struct NetworkInfo {
    const char* key;
    const char* label;         // e.g. "Rest of the world"
    const char* file;          // file name in 1000 roads and in the pack
    const char* virtual_path;  // library path X-Plane's scenery uses
};

extern const std::array<NetworkInfo, kNetworkCount> kNetworks;  // order = Network

using NetworkSet = std::array<bool, kNetworkCount>;
NetworkSet all_networks();

// Per family: (original, scaled) spawn ratio for every car lane seen.
using LaneStats = std::array<std::vector<std::pair<double, double>>, kFamilyCount>;

struct Paths {
    fs::path xplane_root;
    fs::path roads_dir() const;              // Resources/default scenery/1000 roads
    fs::path net_file(Network n) const;      // .../roads.net or .../roads_EU.net
    fs::path custom_scenery() const;
    fs::path pack_dir() const;               // Custom Scenery/zzz_Road_Traffic_Tuner
};

// ---- road file processing ---------------------------------------------------

// Scale the spawn-ratio column of CAR_DRAPED / CAR_GRADED lines. Every other
// byte of the text is preserved. Appends to stats if non-null.
std::string scale_roads_net(const std::string& text, const Multipliers& m,
                            LaneStats* stats);

// Empty if the file has the expected layout; otherwise human-readable problems.
std::vector<std::string> validate(const LaneStats& stats);

// Relative paths (objects/..., textures/...) used by a road file.
std::vector<std::string> referenced_files(const std::string& text);

// Typical spacing between cars in a lane, from the median spawn ratio of the
// original or scaled values, e.g. "45 m" ("no cars" if all are zero).
std::string typical_spacing(const std::vector<std::pair<double, double>>& lanes,
                            bool scaled);

// ---- files ------------------------------------------------------------------

bool read_file(const fs::path& p, std::string& out);
bool write_file(const fs::path& p, const std::string& data);

// FNV-1a 64-bit hash of a file's contents, as 16 hex digits ("" on error).
std::string fingerprint(const fs::path& p);

// ---- pack -------------------------------------------------------------------

struct PackSettings {
    Multipliers mults{};
    NetworkSet networks{};                                // which files the pack replaces
    std::array<std::string, kNetworkCount> fingerprints;  // of the originals used
    std::string tool_version;
};

bool pack_installed(const Paths& p);
bool load_settings(const Paths& p, PackSettings& out);

// Networks the pack replaces whose original X-Plane file has changed since
// the pack was built (e.g. after an X-Plane update).
std::vector<Network> stale_networks(const Paths& p, const PackSettings& s);

struct Conflict {
    std::string pack;  // Custom Scenery folder name
    Network network;
};

// Other Custom Scenery packs whose library.txt also replaces a road file.
std::vector<Conflict> find_conflicts(const Paths& p);

struct BuildResult {
    bool ok = false;
    std::string error;                 // set when !ok
    std::array<LaneStats, kNetworkCount> stats;
    int files_copied = 0;
    std::vector<std::string> missing;  // referenced files not found
};

// Build (or rebuild) the pack for the selected networks. Writes to a temporary
// folder first and swaps it into place, so a failed build never leaves a
// half-written pack.
BuildResult build_pack(const Paths& p, const Multipliers& m, const NetworkSet& nets);

// Delete the pack. Returns false with error text on failure.
bool remove_pack(const Paths& p, std::string& error);

}  // namespace rtt
