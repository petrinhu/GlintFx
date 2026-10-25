// SPDX-License-Identifier: AGPL-3.0-or-later
//
// wire_scratch_dir_selftest.cpp - the test-only scratch_dir of
// wire_shm_test_support.hpp, on its own: when it cannot open the
// descriptor of the directory it has just created, it fails the case
// and leaves nothing behind in the caller's TMPDIR.
//
// GODS_LAWS.md L-09: pure process - one fork(), a resource limit set in
// the child only, and directories this file's own scratch_dir creates.
#include "wire_shm_test_support.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

#include <dirent.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

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

// Runs in the forked child. With no descriptor left to open
// (RLIMIT_NOFILE at zero, EMFILE even for root), mkdtemp() still
// creates the directory and the open() of it fails. Returns 0 when the
// constructor failed, as it must; 1 when it did not, which would mean
// the case never reached the path it exists to test.
int build_scratch_dir_with_no_descriptor_left(const std::string &parent) {
    const int null_fd = ::open("/dev/null", O_WRONLY | O_CLOEXEC);
    if (null_fd >= 0) {
        // The failed check prints "failed: ..." on stderr; in the child
        // that line is the expected outcome, so it goes nowhere.
        (void)::dup2(null_fd, STDERR_FILENO);
        (void)::close(null_fd);
    }
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
    const pid_t child = ::fork();
    GLINTFX_CHECK(child >= 0);
    if (child == 0) {
        ::_exit(build_scratch_dir_with_no_descriptor_left(outer.path()));
    }
    int status = 0;
    GLINTFX_CHECK(::waitpid(child, &status, 0) == child);
    GLINTFX_CHECK(WIFEXITED(status));
    GLINTFX_CHECK_EQ(WEXITSTATUS(status), 0);
    GLINTFX_CHECK_EQ(entries_in(outer.path()), 0u);
}
