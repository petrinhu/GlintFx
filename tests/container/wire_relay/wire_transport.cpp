// SPDX-License-Identifier: AGPL-3.0-or-later
#include "wire_transport.hpp"

#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#include <cstring>

namespace glintfx::test::wire_relay {

namespace {

constexpr std::size_t read_chunk_size = 4096;

std::size_t cmsg_space_for(std::size_t fd_count) {
    return static_cast<std::size_t>(CMSG_SPACE(fd_count * sizeof(int)));
}

void collect_fds(const struct msghdr &msg, std::vector<int> &out) {
    for (struct cmsghdr *header = CMSG_FIRSTHDR(&msg); header != nullptr;
         header = CMSG_NXTHDR(const_cast<struct msghdr *>(&msg), header)) {
        if (header->cmsg_level != SOL_SOCKET || header->cmsg_type != SCM_RIGHTS) {
            continue;
        }
        const std::size_t fd_count = (header->cmsg_len - CMSG_LEN(0)) / sizeof(int);
        const auto *fd_data = reinterpret_cast<const int *>(CMSG_DATA(header));
        out.insert(out.end(), fd_data, fd_data + fd_count);
    }
}

} // namespace

wire_transport::read_result wire_transport::read_once() const {
    read_result result;
    result.bytes.resize(read_chunk_size);

    struct iovec iov{};
    iov.iov_base = result.bytes.data();
    iov.iov_len = result.bytes.size();

    std::vector<std::uint8_t> control(cmsg_space_for(max_fds_per_read));

    struct msghdr msg{};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control.data();
    msg.msg_controllen = control.size();

    const ssize_t received = ::recvmsg(m_fd, &msg, 0);
    if (received <= 0) {
        result.bytes.clear();
        result.end_of_file = true;
        return result;
    }
    result.bytes.resize(static_cast<std::size_t>(received));
    result.control_truncated = (msg.msg_flags & MSG_CTRUNC) != 0;
    collect_fds(msg, result.fds);
    return result;
}

void wire_transport::write_once(const std::uint8_t *bytes, std::size_t len,
                                const std::vector<int> &fds) const {
    struct iovec iov{};
    iov.iov_base = const_cast<std::uint8_t *>(bytes);
    iov.iov_len = len;

    struct msghdr msg{};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    std::vector<std::uint8_t> control;
    if (!fds.empty()) {
        control.resize(cmsg_space_for(fds.size()));
        msg.msg_control = control.data();
        msg.msg_controllen = control.size();
        struct cmsghdr *header = CMSG_FIRSTHDR(&msg);
        header->cmsg_level = SOL_SOCKET;
        header->cmsg_type = SCM_RIGHTS;
        header->cmsg_len = CMSG_LEN(fds.size() * sizeof(int));
        std::memcpy(CMSG_DATA(header), fds.data(), fds.size() * sizeof(int));
        msg.msg_controllen = header->cmsg_len;
    }
    ::sendmsg(m_fd, &msg, MSG_NOSIGNAL);
}

void wire_transport::shutdown_write() const { ::shutdown(m_fd, SHUT_WR); }

} // namespace glintfx::test::wire_relay
