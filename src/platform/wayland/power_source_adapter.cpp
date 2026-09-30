// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/wayland/power_source_adapter.hpp"

#include <array>
#include <cerrno>
#include <charconv>
#include <cstddef>
#include <string_view>
#include <system_error>

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

namespace glintfx::platform {

namespace {
constexpr const char *k_power_supply_root = "/sys/class/power_supply";

// Every value the kernel writes in these files is a short word of a closed vocabulary (Battery,
// Mains, USB, Discharging, Not charging, System, Device, 0, 1, 2...); a dozen characters at most.
constexpr std::size_t k_value_capacity = 32;

// One value read from a file: the first line, trailing whitespace cut, in a buffer of the caller.
struct sysfs_value {
    std::array<char, k_value_capacity> text{};
    std::size_t length = 0;

    [[nodiscard]] std::string_view view() const noexcept {
        return std::string_view(text.data(), length);
    }
};

// Reads file `name` of the directory `dir_fd` into `out`. Missing, empty, unreadable or LONGER than
// the buffer (32 bytes and no newline among them) leaves `out` EMPTY: never a truncated prefix.
void read_value(int dir_fd, const char *name, sysfs_value &out) noexcept {
    out.length = 0;
    const int fd = ::openat(dir_fd, name, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return;
    }
    ssize_t got = 0;
    do {
        got = ::read(fd, out.text.data(), out.text.size());
    } while (got < 0 && errno == EINTR);
    ::close(fd);
    if (got <= 0) {
        return;
    }
    const auto bytes = static_cast<std::size_t>(got);
    std::size_t end = 0;
    while (end < bytes && out.text[end] != '\n') {
        ++end;
    }
    if (end == bytes && bytes == out.text.size()) {
        return; // the buffer is full and the line has not ended: the value is longer than we hold
    }
    while (end > 0 &&
           (out.text[end - 1] == ' ' || out.text[end - 1] == '\t' || out.text[end - 1] == '\r')) {
        --end;
    }
    out.length = end;
}

// A whole number; `fallback` for a value that is missing or not a number.
[[nodiscard]] int read_number(int dir_fd, const char *name, int fallback) noexcept {
    sysfs_value value;
    read_value(dir_fd, name, value);
    if (value.length == 0) {
        return fallback;
    }
    int number = fallback;
    const char *first = value.text.data();
    const char *last = first + value.length;
    const std::from_chars_result parsed = std::from_chars(first, last, number);
    return (parsed.ec == std::errc{} && parsed.ptr == last) ? number : fallback;
}

void fold_supply(int supply_fd, power_supply_tally &tally) noexcept {
    sysfs_value type;
    sysfs_value scope;
    sysfs_value status;
    read_value(supply_fd, "type", type);
    read_value(supply_fd, "scope", scope);
    read_value(supply_fd, "status", status);

    power_supply_entry entry;
    entry.type = type.view();
    entry.scope = scope.view();
    entry.present = read_number(supply_fd, "present", 1);
    entry.online = read_number(supply_fd, "online", 0);
    entry.status = status.view();
    tally.add(entry);
}
} // namespace

void read_power_supplies(const char *root, power_supply_tally &tally) noexcept {
    DIR *dir = ::opendir(root);
    if (dir == nullptr) {
        return;
    }
    const int root_fd = ::dirfd(dir);
    if (root_fd <
        0) { // cannot happen for a directory stream that opened, but openat() must never see -1
        ::closedir(dir);
        return;
    }
    while (const dirent *listed = ::readdir(dir)) {
        if (listed->d_name[0] == '.') { // ".", ".." and hidden names are never supplies
            continue;
        }
        const int supply_fd = ::openat(root_fd, listed->d_name, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (supply_fd < 0) {
            continue;
        }
        fold_supply(supply_fd, tally);
        ::close(supply_fd);
    }
    ::closedir(dir);
}

gltfx_power_source read_power_source_at(const char *root) noexcept {
    power_supply_tally tally;
    read_power_supplies(root, tally);
    return tally.result();
}

gltfx_power_source wayland_power_source_adapter::read() const noexcept {
    return read_power_source_at(k_power_supply_root);
}

} // namespace glintfx::platform
