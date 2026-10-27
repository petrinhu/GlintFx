// SPDX-License-Identifier: AGPL-3.0-or-later
//
// wire_scratch_dir_selftest.cpp - the test-only scratch_dir of
// wire_shm_test_support.hpp, on its own: when it cannot open the
// descriptor of the directory it has just created, it fails the case
// and leaves nothing behind in the caller's TMPDIR.
//
// GODS_LAWS.md L-09: pure process - fork(), one pipe, resource limits
// set in children only, a read-only descriptor of "/" opened and closed
// again, and directories this file's own scratch_dir creates.
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
#include <cstdint>
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

// Which call failed when a child with a report pipe could not be started.
enum class failed_call : std::uint8_t { none, pipe, fork };

// A child started with a pipe to report on: its pid and the two ends.
// When pipe() or fork() fails, pid is -1, both ends are closed already,
// so a case that stops on the failure leaves no descriptor open behind
// it (the review of SUITE-TMPDIR-PATHS-fix2, COSMETIC-3), and `failed`
// and `error` say which call failed and with what errno. The errno is
// kept at the failed call, before the two close() calls, which may
// change it even when they succeed.
struct reporting_child {
    pid_t pid = -1;
    int read_end = -1;
    int write_end = -1;
    failed_call failed = failed_call::none;
    int error = 0;
};

reporting_child fork_with_report_pipe() {
    std::array<int, 2> ends{};
    if (::pipe(ends.data()) != 0) {
        return reporting_child{-1, -1, -1, failed_call::pipe, errno};
    }
    const pid_t pid = ::fork();
    if (pid < 0) {
        const int error = errno;
        (void)::close(ends[0]);
        (void)::close(ends[1]);
        return reporting_child{-1, -1, -1, failed_call::fork, error};
    }
    return reporting_child{pid, ends[0], ends[1], failed_call::none, 0};
}

// How many entries /proc/self/fd lists: the descriptors this process
// has open, plus the one opendir() itself holds and "." and "..", the
// same three extra before and after; -1 when the folder cannot be read.
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

// True when open_descriptors() counts one more while a descriptor is
// open, and `before` again once it is closed. Without this, a count
// that never moves would let a descriptor left open pass unseen.
bool the_count_sees_an_open_descriptor(int before) {
    const int extra = ::open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (extra < 0) {
        return false;
    }
    const int with_extra = open_descriptors();
    (void)::close(extra);
    return with_extra == before + 1 && open_descriptors() == before;
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

// Leaves this helper with no process left to start: as a user the limit binds, with RLIMIT_NPROC
// at zero, so its next fork() fails with EAGAIN. False when either step could not be done.
bool no_process_left_for_this_user() {
    if (!run_as_a_user_the_limit_binds()) {
        return false;
    }
    struct rlimit limit{};
    if (::getrlimit(RLIMIT_NPROC, &limit) != 0) {
        return false;
    }
    limit.rlim_cur = 0;
    return ::setrlimit(RLIMIT_NPROC, &limit) == 0;
}

// How the helper of the fork case ended, as its exit status: one value per way the case can end,
// so the check that fails in the case names the reason.
enum class fork_verdict : std::uint8_t {
    failed_and_left_nothing_open = 0,
    did_not_fail = 1,
    left_a_descriptor_open = 2,
    could_not_set_up = 3,
    pipe_failed_first = 4,
    failed_for_another_reason = 5,
};

// Runs in a forked helper. Passes only when the start failed at fork(), with the EAGAIN the limit
// gives, and the descriptors open after fork_with_report_pipe() are the ones open before it. Any
// other end is a verdict of its own: fork() did not fail, so the case never reached the path it
// exists to test; the pipe failed first, so fork() was never tried; fork() failed for another
// reason than the limit; or the helper could not set itself up or trust its count.
fork_verdict fork_fails_with_no_descriptor_left() {
    if (!no_process_left_for_this_user()) {
        return fork_verdict::could_not_set_up;
    }
    const int before = open_descriptors();
    if (before < 0 || !the_count_sees_an_open_descriptor(before)) {
        return fork_verdict::could_not_set_up;
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
        return fork_verdict::did_not_fail;
    }
    if (child.failed != failed_call::fork) {
        return fork_verdict::pipe_failed_first;
    }
    if (child.error != EAGAIN) {
        return fork_verdict::failed_for_another_reason;
    }
    return open_descriptors() == before ? fork_verdict::failed_and_left_nothing_open
                                        : fork_verdict::left_a_descriptor_open;
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
// keeps its own limits and user. Every way the helper can end other
// than the one this case exists for has its own check, so the line
// that fails says why: fork() did not fail, a descriptor was left
// open, the helper could not set itself up, the pipe failed before
// fork() was tried (the review of SUITE-TMPDIR-PATHS-fix4,
// IMPORTANT-1), or fork() failed for another reason than the limit.
GLINTFX_TEST(wire_scratch_dir_a_fork_that_fails_leaves_no_descriptor_open) {
    const pid_t helper = ::fork();
    GLINTFX_CHECK(helper >= 0);
    if (helper == 0) {
        ::_exit(static_cast<int>(fork_fails_with_no_descriptor_left()));
    }
    int status = 0;
    GLINTFX_CHECK(::waitpid(helper, &status, 0) == helper);
    GLINTFX_CHECK(WIFEXITED(status));
    const auto verdict = static_cast<fork_verdict>(WEXITSTATUS(status));
    GLINTFX_CHECK(verdict != fork_verdict::did_not_fail);
    GLINTFX_CHECK(verdict != fork_verdict::left_a_descriptor_open);
    GLINTFX_CHECK(verdict != fork_verdict::could_not_set_up);
    GLINTFX_CHECK(verdict != fork_verdict::pipe_failed_first);
    GLINTFX_CHECK(verdict != fork_verdict::failed_for_another_reason);
    GLINTFX_CHECK(verdict == fork_verdict::failed_and_left_nothing_open);
}
