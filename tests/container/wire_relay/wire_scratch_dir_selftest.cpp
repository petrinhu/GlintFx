// SPDX-License-Identifier: AGPL-3.0-or-later
//
// wire_scratch_dir_selftest.cpp - the test-only scratch_dir of
// wire_shm_test_support.hpp, on its own: when it cannot open the
// descriptor of the directory it has just created, it fails the case
// and leaves nothing behind in the caller's TMPDIR.
//
// GODS_LAWS.md L-09: pure process - fork(), one pipe, resource limits
// set in children only, and directories this file's own scratch_dir
// creates.
#include "wire_shm_test_support.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

#include <dirent.h>
#include <fcntl.h>
#include <sys/prctl.h>
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

// A child started with a pipe to report on: its pid and the two ends.
// When pipe() or fork() fails, pid is -1 and both ends are closed
// already, so a case that stops on the failure leaves no descriptor
// open behind it (the review of SUITE-TMPDIR-PATHS-fix2, COSMETIC-3).
struct reporting_child {
    pid_t pid = -1;
    int read_end = -1;
    int write_end = -1;
};

reporting_child fork_with_report_pipe() {
    std::array<int, 2> ends{};
    if (::pipe(ends.data()) != 0) {
        return {};
    }
    const pid_t pid = ::fork();
    if (pid < 0) {
        (void)::close(ends[0]);
        (void)::close(ends[1]);
        return {};
    }
    return reporting_child{pid, ends[0], ends[1]};
}

// How many descriptors this process has open, counted in /proc/self/fd
// (the one opendir() itself holds included, the same before and after);
// -1 when the folder cannot be read.
int open_descriptors() {
    DIR *dir = ::opendir("/proc/self/fd");
    if (dir == nullptr) {
        return -1;
    }
    int count = 0;
    while (::readdir(dir) != nullptr) {
        ++count;
    }
    (void)::closedir(dir);
    return count;
}

// The user and group a helper runs as when it starts as root: nobody's numbers, which need no
// entry in /etc/passwd or /etc/group.
constexpr uid_t k_unprivileged_user = 65534;
constexpr gid_t k_unprivileged_group = 65534;

// Root, uid 0 of the first user namespace, is never held to RLIMIT_NPROC, whatever its
// capabilities: kernel/fork.c checks "p->real_cred->user != INIT_USER" before CAP_SYS_RESOURCE and
// CAP_SYS_ADMIN (measured: as root, with Docker's default capabilities and with --cap-drop=ALL,
// fork() still succeeds at a limit of zero; as any other user it fails with EAGAIN). So a helper
// that starts as root gives up its group and user first (the limit reads the real user only, so
// the supplementary groups stay: setgroups() would need <grp.h>, outside the gate of
// tests/tools/check_dep_zero.py), and keeps /proc/self readable (a change of user clears the
// dumpable flag, which hands /proc/self/fd to root). True when the helper now runs as a user the
// limit binds.
bool run_as_a_user_the_limit_binds() {
    if (::geteuid() != 0) {
        return true;
    }
    if (::setgid(k_unprivileged_group) != 0 || ::setuid(k_unprivileged_user) != 0) {
        return false;
    }
    return ::prctl(PR_SET_DUMPABLE, 1L, 0L, 0L, 0L) == 0;
}

// Runs in a forked helper. With RLIMIT_NPROC at zero, fork() fails with EAGAIN for a user the limit
// binds (run_as_a_user_the_limit_binds()). Returns 0 when fork() failed and the descriptors open
// after fork_with_report_pipe() are the ones open before it; 1 when fork() did not fail, so the
// case never reached the path it exists to test; 2 when a descriptor was left open; 3 when the
// helper could not become such a user, set the limit or count the descriptors.
int fork_fails_with_no_descriptor_left() {
    if (!run_as_a_user_the_limit_binds()) {
        return 3;
    }
    const int before = open_descriptors();
    struct rlimit limit{};
    if (before < 0 || ::getrlimit(RLIMIT_NPROC, &limit) != 0) {
        return 3;
    }
    limit.rlim_cur = 0;
    if (::setrlimit(RLIMIT_NPROC, &limit) != 0) {
        return 3;
    }
    const reporting_child child = fork_with_report_pipe();
    if (child.pid == 0) {
        ::_exit(0);
    }
    if (child.pid > 0) {
        (void)::close(child.read_end);
        (void)::close(child.write_end);
        int status = 0;
        (void)::waitpid(child.pid, &status, 0);
        return 1;
    }
    return open_descriptors() == before ? 0 : 2;
}

} // namespace

GLINTFX_TEST(wire_scratch_dir_leaves_nothing_behind_when_it_cannot_open_its_descriptor) {
    const scratch_dir outer;
    const reporting_child spawned = fork_with_report_pipe();
    GLINTFX_CHECK(spawned.pid >= 0);
    const pid_t child = spawned.pid;
    if (child == 0) {
        (void)::close(spawned.read_end);
        ::_exit(build_scratch_dir_with_no_descriptor_left(outer.path(), spawned.write_end));
    }
    (void)::close(spawned.write_end);
    const std::string child_stderr = read_until_closed(spawned.read_end);
    (void)::close(spawned.read_end);
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

// The fork that fails leaves no descriptor open (the review of
// SUITE-TMPDIR-PATHS-fix2, COSMETIC-3). The limit is set in a helper
// child, which gives up root first when it has it, so this process
// keeps its own limits and user; exit 1 from the helper means fork()
// did not fail there, and the case fails instead of passing without
// testing anything.
GLINTFX_TEST(wire_scratch_dir_a_fork_that_fails_leaves_no_descriptor_open) {
    const pid_t helper = ::fork();
    GLINTFX_CHECK(helper >= 0);
    if (helper == 0) {
        ::_exit(fork_fails_with_no_descriptor_left());
    }
    int status = 0;
    GLINTFX_CHECK(::waitpid(helper, &status, 0) == helper);
    GLINTFX_CHECK(WIFEXITED(status));
    GLINTFX_CHECK_EQ(WEXITSTATUS(status), 0);
}
