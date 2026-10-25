// SPDX-License-Identifier: AGPL-3.0-or-later
//
// wire_scratch_dir_selftest.cpp - the test-only scratch_dir of
// wire_shm_test_support.hpp, on its own: when it cannot open the
// descriptor of the directory it has just created, it fails the case
// and leaves nothing behind in the caller's TMPDIR.
//
// GODS_LAWS.md L-09: pure process - one fork(), one pipe, a resource
// limit set in the child only, and directories this file's own
// scratch_dir creates.
#include "wire_shm_test_support.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

#include <dirent.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstddef>
#include <string>

using namespace glintfx::test::wire_relay;

namespace {

// Entries of `path`, "." and ".." left out.
std::size_t entries_in(const std::string &path) {
    DIR *dir = ::opendir(path.c_str());
    GLINTFX_CHECK(dir != nullptr);
    std::size_t count = 0;
    while (const struct dirent *entry = ::readdir(dir)) {
        const std::string name = entry->d_name;
        if (name != "." && name != "..") {
            ++count;
        }
    }
    (void)::closedir(dir);
    return count;
}

// Everything written on `fd` until every writer has closed it.
std::string read_until_closed(int fd) {
    std::string text;
    std::array<char, 256> chunk{};
    for (;;) {
        const ssize_t got = ::read(fd, chunk.data(), chunk.size());
        if (got > 0) {
            text.append(chunk.data(), static_cast<std::size_t>(got));
        } else if (got < 0 && errno == EINTR) {
            continue;
        } else {
            return text;
        }
    }
}

// Runs in the forked child. With no descriptor left to open
// (RLIMIT_NOFILE at zero, EMFILE even for root), mkdtemp() still
// creates the directory and the open() of it fails. The failed check
// prints "<file>:<line>: failed: <expression>" on stderr
// (tests/harness/check.cpp), and stderr is `report_fd`, so the parent
// can tell WHICH check failed. Returns 0 when the constructor failed,
// as it must; 1 when it did not, which would mean the case never
// reached the path it exists to test.
int build_scratch_dir_with_no_descriptor_left(const std::string &parent, int report_fd) {
    // Before the limit: with RLIMIT_NOFILE at zero, dup2() onto
    // descriptor 2 fails with EBADF.
    if (::dup2(report_fd, STDERR_FILENO) < 0) {
        return 2;
    }
    (void)::close(report_fd);
    struct rlimit limit{};
    if (::getrlimit(RLIMIT_NOFILE, &limit) != 0) {
        return 2;
    }
    limit.rlim_cur = 0;
    if (::setrlimit(RLIMIT_NOFILE, &limit) != 0) {
        return 2;
    }
    try {
        const scratch_dir dir(parent);
        return 1;
    } catch (const glintfx::test::case_check_failed &) {
        return 0;
    }
}

} // namespace

GLINTFX_TEST(wire_scratch_dir_leaves_nothing_behind_when_it_cannot_open_its_descriptor) {
    const scratch_dir outer;
    std::array<int, 2> report{};
    GLINTFX_CHECK(::pipe(report.data()) == 0);
    const pid_t child = ::fork();
    GLINTFX_CHECK(child >= 0);
    if (child == 0) {
        (void)::close(report[0]);
        ::_exit(build_scratch_dir_with_no_descriptor_left(outer.path(), report[1]));
    }
    (void)::close(report[1]);
    const std::string child_stderr = read_until_closed(report[0]);
    (void)::close(report[0]);
    int status = 0;
    GLINTFX_CHECK(::waitpid(child, &status, 0) == child);
    GLINTFX_CHECK(WIFEXITED(status));
    GLINTFX_CHECK_EQ(WEXITSTATUS(status), 0);
    // The precondition, before the result: the check that failed in the
    // child is the one on the open() of the directory mkdtemp() had just
    // made (wire_shm_test_support.hpp), not any earlier one. A failure
    // before mkdtemp() also leaves the folder empty, and without this
    // line the case would pass without testing anything. The harness
    // stops at the first failed check, so the child prints at most one
    // such line; its whole end is matched, not a substring anywhere, so a
    // longer expression that merely contains this one does not count.
    GLINTFX_CHECK(child_stderr.ends_with(": failed: m_dir_fd >= 0\n"));
    GLINTFX_CHECK_EQ(entries_in(outer.path()), 0u);
}
