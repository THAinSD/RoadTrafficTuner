// Tests for the Road Traffic Tuner core. Not part of the plugin build.
// Usage: test_core <fake X-Plane root whose 1000 roads holds roads.net and roads_EU.net>

#include <iostream>

#include "../src/rtt_core.h"

using namespace rtt;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::cerr << "FAILED line " << __LINE__ << ": " #cond "\n"; \
            return 1;                                                   \
        }                                                               \
    } while (0)

int main(int argc, char** argv) {
    if (argc < 2) return 2;
    Paths p{argv[1]};
    std::string err;

    for (int n = 0; n < kNetworkCount; ++n) {
        std::string text;
        CHECK(read_file(p.net_file(Network(n)), text));
        LaneStats st1, st;
        CHECK(scale_roads_net(text, Multipliers{1, 1, 1, 1}, &st1) == text);  // identity
        CHECK(validate(st1).empty());
        scale_roads_net(text, default_multipliers(), &st);
        std::cout << kNetworks[n].file << ": " << referenced_files(text).size()
                  << " referenced files\n";
        for (int i = 0; i < kFamilyCount; ++i)
            std::cout << "  " << kFamilies[i].label << ": " << typical_spacing(st[i], false)
                      << " -> " << typical_spacing(st[i], true) << "\n";
        if (n == kWorld) write_file("cpp_scaled.net", scale_roads_net(text, default_multipliers(), nullptr));
    }

    // Zero multiplier shows "no cars".
    {
        std::string text;
        read_file(p.net_file(kWorld), text);
        LaneStats st;
        scale_roads_net(text, Multipliers{0, 1, 1, 1}, &st);
        CHECK(typical_spacing(st[kLocal], true) == "no cars");
    }

    // Each combination of networks.
    const NetworkSet combos[] = {{true, false}, {false, true}, {true, true}};
    for (const auto& nets : combos) {
        remove_pack(p, err);
        auto r = build_pack(p, default_multipliers(), nets);
        if (!r.ok) std::cerr << r.error << "\n";
        CHECK(r.ok);
        std::string lib;
        CHECK(read_file(p.pack_dir() / "library.txt", lib));
        for (int n = 0; n < kNetworkCount; ++n) {
            CHECK(fs::exists(p.pack_dir() / kNetworks[n].file) == nets[n]);
            CHECK((lib.find(kNetworks[n].virtual_path) != std::string::npos) == nets[n]);
        }
        PackSettings s;
        CHECK(load_settings(p, s));
        CHECK(s.networks == nets && s.mults == default_multipliers());
        CHECK(stale_networks(p, s).empty());
        std::cout << "networks " << nets[0] << nets[1] << ": copied " << r.files_copied
                  << ", missing " << r.missing.size() << "\n";
    }
    CHECK(!fs::exists(p.xplane_root / "Road_Traffic_Tuner_building"));
    CHECK(!fs::exists(p.xplane_root / "Road_Traffic_Tuner_old"));

    // Nothing selected is refused.
    CHECK(!build_pack(p, default_multipliers(), NetworkSet{false, false}).ok);
    CHECK(pack_installed(p));  // previous pack untouched

    // Stale detection: change the Europe original.
    {
        PackSettings s;
        load_settings(p, s);
        std::string eu;
        read_file(p.net_file(kEurope), eu);
        write_file(p.net_file(kEurope), eu + "\n# changed\n");
        auto stale = stale_networks(p, s);
        CHECK(stale.size() == 1 && stale[0] == kEurope);
        write_file(p.net_file(kEurope), eu);
        CHECK(stale_networks(p, s).empty());
    }

    // Settings files from early test builds still load (world only).
    write_file(p.pack_dir() / "rtt_settings.txt",
               "tool_version 0.9\nroads_fingerprint " + fingerprint(p.net_file(kWorld)) +
                   "\nlocal 0.10\nsecondary 0.40\nprimary 0.70\nhighway 1.00\n");
    {
        PackSettings s;
        CHECK(load_settings(p, s));
        CHECK(s.networks[kWorld] && !s.networks[kEurope]);
        CHECK(stale_networks(p, s).empty());
    }

    std::cout << "conflicts:";
    for (auto& c : find_conflicts(p))
        std::cout << " [" << c.pack << " / " << kNetworks[c.network].label << "]";
    std::cout << "\n";

    CHECK(remove_pack(p, err));
    CHECK(!pack_installed(p));
    CHECK(fs::exists(p.net_file(kWorld)) && fs::exists(p.net_file(kEurope)));
    std::cout << "ALL TESTS PASSED\n";
    return 0;
}
