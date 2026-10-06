// SPDX-License-Identifier: AGPL-3.0-or-later
#include "win32_capture_child.hpp"

#include <cstdio>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "win32_capture_text.hpp"

namespace glintfx::capture_tool {

namespace {

constexpr DWORD k_poll_ms = 20;
constexpr DWORD k_read_chunk = 4096;
constexpr DWORD k_terminate_wait_ms = 5000;

HANDLE as_handle(void *opaque) { return static_cast<HANDLE>(opaque); }

std::wstring quote_argument(const std::string &argument) {
    const std::wstring wide = utf8_to_wide(argument);
    if (!wide.empty() && wide.find_first_of(L" \t") == std::wstring::npos) {
        return wide;
    }
    return L"\"" + wide + L"\"";
}

std::wstring build_command_line(const std::vector<std::string> &argv) {
    std::wstring line;
    for (const std::string &argument : argv) {
        line += (line.empty() ? L"" : L" ") + quote_argument(argument);
    }
    return line;
}

bool make_output_pipe(HANDLE &read_end, HANDLE &write_end) {
    SECURITY_ATTRIBUTES inheritable{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    if (::CreatePipe(&read_end, &write_end, &inheritable, 0) == 0) {
        return false;
    }
    return ::SetHandleInformation(read_end, HANDLE_FLAG_INHERIT, 0) != 0;
}

HANDLE open_nul_for_input() {
    SECURITY_ATTRIBUTES inheritable{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    return ::CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inheritable,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
}

bool create_with_pipe(std::wstring &command_line, HANDLE output_write, HANDLE input,
                      PROCESS_INFORMATION &info) {
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES; // deliberately NOT STARTF_USESHOWWINDOW
    startup.hStdInput = input;
    startup.hStdOutput = output_write;
    startup.hStdError = output_write;
    return ::CreateProcessW(nullptr, command_line.data(), nullptr, nullptr, TRUE, 0, nullptr,
                            nullptr, &startup, &info) != 0;
}

} // namespace

bool launch_child(const std::vector<std::string> &argv, child_process &child) {
    HANDLE output_read = nullptr;
    HANDLE output_write = nullptr;
    const HANDLE input = open_nul_for_input();
    if (argv.empty() || input == INVALID_HANDLE_VALUE ||
        !make_output_pipe(output_read, output_write)) {
        std::fprintf(stderr, "win32_window_capture: cannot prepare the child's pipes (error %lu)\n",
                     ::GetLastError());
        return false;
    }
    std::wstring command_line = build_command_line(argv);
    PROCESS_INFORMATION info{};
    const bool created = create_with_pipe(command_line, output_write, input, info);
    const DWORD error = ::GetLastError();
    ::CloseHandle(output_write); // the child holds the only write end: EOF is then observable
    ::CloseHandle(input);
    if (!created) {
        ::CloseHandle(output_read);
        std::fprintf(stderr, "win32_window_capture: CreateProcessW failed (error %lu)\n", error);
        return false;
    }
    ::CloseHandle(info.hThread);
    child = {.process = info.hProcess, .output_read = output_read, .id = info.dwProcessId};
    return true;
}

void drain_output(const child_process &child, std::string &captured) {
    char buffer[k_read_chunk];
    DWORD available = 0;
    while (::PeekNamedPipe(as_handle(child.output_read), nullptr, 0, nullptr, &available,
                           nullptr) != 0 &&
           available > 0) {
        DWORD read = 0;
        if (::ReadFile(as_handle(child.output_read), buffer, k_read_chunk, &read, nullptr) == 0 ||
            read == 0) {
            return;
        }
        captured.append(buffer, read);
        std::fwrite(buffer, 1, read, stdout);
    }
    std::fflush(stdout);
}

wait_outcome wait_for_output(const child_process &child, std::string_view needle, int budget_ms,
                             std::string &captured) {
    const ULONGLONG deadline = ::GetTickCount64() + static_cast<ULONGLONG>(budget_ms);
    while (::GetTickCount64() < deadline) {
        drain_output(child, captured);
        if (captured.find(needle) != std::string::npos) {
            return wait_outcome::found;
        }
        if (::WaitForSingleObject(as_handle(child.process), 0) == WAIT_OBJECT_0) {
            drain_output(child, captured);
            return captured.find(needle) != std::string::npos ? wait_outcome::found
                                                              : wait_outcome::child_exited;
        }
        ::Sleep(k_poll_ms);
    }
    return wait_outcome::timed_out;
}

bool wait_for_exit(const child_process &child, int budget_ms, unsigned long &exit_code) {
    std::string ignored;
    const ULONGLONG deadline = ::GetTickCount64() + static_cast<ULONGLONG>(budget_ms);
    while (::GetTickCount64() < deadline) {
        drain_output(child, ignored);
        if (::WaitForSingleObject(as_handle(child.process), k_poll_ms) == WAIT_OBJECT_0) {
            drain_output(child, ignored);
            DWORD code = 1;
            ::GetExitCodeProcess(as_handle(child.process), &code);
            exit_code = code;
            return true;
        }
    }
    return false;
}

void terminate_child(const child_process &child) {
    ::TerminateProcess(as_handle(child.process), 1);
    ::WaitForSingleObject(as_handle(child.process), k_terminate_wait_ms);
}

void close_child(child_process &child) {
    ::CloseHandle(as_handle(child.process));
    ::CloseHandle(as_handle(child.output_read));
    child = {};
}

} // namespace glintfx::capture_tool
