// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <print>
#include <string>
#include <string_view>

#include <unistd.h>

#include "platform/port/power_source.hpp"
#include "platform/port/power_source_port.hpp"
#include "platform/wayland/power_source_adapter.hpp"
#include "platform/wayland/selected_power_source_adapter.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// power_source_adapter_test.cpp - GFX-PRESET, fatia P2 (docs/plano-w6b-fatias-5.md sec. 5.1,
// D-W6b-34; /var/tmp/cto-w7d/PLANO-errata.md D-P2-1/D-P2-2): the reader of /sys/class/power_supply,
// over a FAKE tree the test builds under the temporary directory (the real root is a literal only
// the adapter passes; nothing here reads the machine's own supplies). The reader NEVER allocates:
// every value it reads sits in a 32-char buffer, the directory is visited entry by entry, and what
// comes out is the answer of the pure rule (mains, battery or unknown) - so each cell below is an
// OBSERVABLE consequence, chosen so that the wrong default, the missing trim or a truncated value
// would flip it:
//   - a missing `present` file must read as PRESENT (1): a Battery Discharging with no `present`
//     file is `battery`, not `unknown`;
//   - a missing `online` file must read as OFFLINE (0): a Mains supply with no `online` file next
//     to a Battery Discharging is `battery`, not `mains`;
//   - a missing `scope` file must read as System: the same battery is `battery`, not `unknown`;
//   - trailing spaces, tab and CR are cut: a Battery "Full \r" is mains (energy held), and only
//     the cut makes "Full" equal "Full";
//   - a value LONGER than the buffer is unreadable, NEVER a truncated prefix: a status of "Full"
//     followed by 30 spaces and an "x" would read "Full" if it were truncated to 32 characters and
//     trimmed (mains); it must read as an unknown status (battery). Same for a 40-char `type`.
//
// This test is Linux-only (the reader is): its CMake registration sits inside if(UNIX). The Windows
// twin is the pure power_status_rule_test plus the CI's own Windows runner.
//
// RED, SEEN (D-P2-2): written BEFORE the implementation - first against no header at all
// (compilation), then against a stub that always answers `unknown` (assertion).

using glintfx::platform::gltfx_power_source;
using glintfx::platform::read_power_source_at;

namespace {
namespace fs = std::filesystem;

// A fake power_supply root; removed when the case ends.
struct fake_root {
    fs::path path;
    fake_root() {
        static int counter = 0; // with the pid: unique per case and per process, no rand()
        const char *tmp = std::getenv("TMPDIR");
        path = fs::path(tmp != nullptr ? tmp : "/var/tmp") /
               ("glintfx-power-source-test-" + std::to_string(::getpid()) + "-" +
                std::to_string(counter++));
        fs::create_directories(path);
    }
    ~fake_root() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
    fake_root(const fake_root &) = delete;
    fake_root &operator=(const fake_root &) = delete;

    void put(std::string_view supply, std::string_view file, std::string_view content) const {
        fs::create_directories(path / supply);
        std::ofstream(path / supply / file) << content;
    }
    void supply(std::string_view name) const { fs::create_directories(path / name); }
    [[nodiscard]] gltfx_power_source read() const {
        const std::string root = path.string();
        return read_power_source_at(root.c_str());
    }
};

constexpr gltfx_power_source k_unknown = gltfx_power_source::unknown;
constexpr gltfx_power_source k_mains = gltfx_power_source::mains;
constexpr gltfx_power_source k_battery = gltfx_power_source::battery;
} // namespace

GLINTFX_TEST(power_source_adapter_reads_the_fake_tree) {
    int analyzed = 0;

    // A directory that does not exist: unknown.
    GLINTFX_CHECK(read_power_source_at("/nonexistent/glintfx/power_supply") == k_unknown);
    ++analyzed;
    // An empty root, and a supply directory with nothing in it: unknown.
    {
        const fake_root root;
        GLINTFX_CHECK(root.read() == k_unknown);
        root.supply("EMPTY");
        GLINTFX_CHECK(root.read() == k_unknown);
        ++analyzed;
    }
    // Every file present, the plain case: a Battery Discharging.
    {
        const fake_root root;
        root.put("BAT0", "type", "Battery\n");
        root.put("BAT0", "scope", "System\n");
        root.put("BAT0", "present", "1\n");
        root.put("BAT0", "status", "Discharging\n");
        GLINTFX_CHECK(root.read() == k_battery);
        ++analyzed;
    }
    // A missing `present` file reads as present; `present` = 0 is an empty bay.
    {
        const fake_root root;
        root.put("BAT0", "type", "Battery\n");
        root.put("BAT0", "status", "Discharging\n");
        GLINTFX_CHECK(root.read() == k_battery); // no present file, no scope file: present, System
        root.put("BAT0", "present", "0\n");
        GLINTFX_CHECK(root.read() == k_unknown); // the same battery, now an empty bay
        ++analyzed;
    }
    // A missing `online` file reads as offline: the Mains next to the discharging battery does
    // NOT make it mains.
    {
        const fake_root root;
        root.put("AC", "type", "Mains\n");
        root.put("BAT0", "type", "Battery\n");
        root.put("BAT0", "status", "Discharging\n");
        GLINTFX_CHECK(root.read() == k_battery);
        root.put("AC", "online", "1\n");
        GLINTFX_CHECK(root.read() == k_mains);
        ++analyzed;
    }
    // A missing `scope` reads as System; a scope of Device leaves the battery out of the rule.
    {
        const fake_root root;
        root.put("BAT0", "type", "Battery\n");
        root.put("BAT0", "status", "Discharging\n");
        GLINTFX_CHECK(root.read() == k_battery);
        root.put("BAT0", "scope", "Device\n");
        GLINTFX_CHECK(root.read() == k_unknown);
        ++analyzed;
    }
    // A value that is not a number takes the default: `present` "yes" is present, `online` "1x" is
    // offline.
    {
        const fake_root root;
        root.put("AC", "type", "Mains\n");
        root.put("AC", "online", "1x\n");
        root.put("BAT0", "type", "Battery\n");
        root.put("BAT0", "present", "yes\n");
        root.put("BAT0", "status", "Discharging\n");
        GLINTFX_CHECK(root.read() == k_battery);
        ++analyzed;
    }
    // The trailing whitespace is cut (space, CR, LF): "Full \r\n" is "Full", energy held -> mains.
    {
        const fake_root root;
        root.put("BAT0", "type", "Battery\n");
        root.put("BAT0", "status", "Full \r\n");
        GLINTFX_CHECK(root.read() == k_mains);
        ++analyzed;
    }
    // A value LONGER than the 32-char buffer is unreadable, never a truncated prefix: "Full" +
    // 30 spaces + "x" would be "Full" (mains) if cut to 32 and trimmed; the correct answer is an
    // unknown status, which is a battery.
    {
        const fake_root root;
        root.put("BAT0", "type", "Battery\n");
        root.put("BAT0", "status", std::string("Full") + std::string(30, ' ') + "x\n");
        GLINTFX_CHECK(root.read() == k_battery);
        ++analyzed;
    }
    // A 40-char `type` starting with "Battery": unreadable, so the entry is ignored (unknown).
    {
        const fake_root root;
        root.put("BAT0", "type", std::string("Battery") + std::string(33, 'x') + "\n");
        root.put("BAT0", "status", "Discharging\n");
        GLINTFX_CHECK(root.read() == k_unknown);
        ++analyzed;
    }
    // A value of exactly 31 characters plus the newline still fits (the buffer is 32): a `type` of
    // "Battery" + 24 spaces is 31 chars, trimmed to "Battery".
    {
        const fake_root root;
        root.put("BAT0", "type", std::string("Battery") + std::string(24, ' ') + "\n");
        root.put("BAT0", "status", "Discharging\n");
        GLINTFX_CHECK(root.read() == k_battery);
        ++analyzed;
    }
    // F18: the scope-Device USB-C charger online next to a Charging battery, read from files:
    // mains.
    {
        const fake_root root;
        root.put("ucsi-source-psy", "type", "USB\n");
        root.put("ucsi-source-psy", "scope", "Device\n");
        root.put("ucsi-source-psy", "online", "1\n");
        root.put("BAT0", "type", "Battery\n");
        root.put("BAT0", "scope", "System\n");
        root.put("BAT0", "present", "1\n");
        root.put("BAT0", "status", "Charging\n");
        GLINTFX_CHECK(root.read() == k_mains);
        ++analyzed;
    }
    // The selected adapter answers something of the closed vocabulary on THIS machine (which one is
    // the machine's own business: it is not asserted), and satisfies the port.
    {
        const glintfx::platform::selected_power_source_adapter adapter;
        const gltfx_power_source now = adapter.read();
        GLINTFX_CHECK(now == k_unknown || now == k_mains || now == k_battery);
        static_assert(glintfx::platform::power_source_adapter_port<
                      glintfx::platform::selected_power_source_adapter>);
        ++analyzed;
    }

    GLINTFX_CHECK_EQ(analyzed, 13);
    std::println("power_source_adapter_test: {} celula(s) conferida(s)", analyzed);
}
