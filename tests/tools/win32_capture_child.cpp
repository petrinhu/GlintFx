// SPDX-License-Identifier: AGPL-3.0-or-later
#include "win32_capture_child.hpp"

#include <cstdio>
#include <vector>

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

// Only the NUL input and the output pipe's write end are handed to the child (a handle list): it
// inherits nothing else, in particular not the handles this tool got from its own launcher, which
// are the driver's pipes. A child holding those would keep the driver waiting for the pipe's end
// after this tool is gone (review A4).
bool create_with_handle_list(std::wstring &command_line, HANDLE (&handles)[2],
                             PROCESS_INFORMATION &info) {
    SIZE_T size = 0;
    ::InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
    std::vector<char> storage(size);
    auto *attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    if (::InitializeProcThreadAttributeList(attributes, 1, 0, &size) == 0) {
        return false;
    }
    const bool listed =
        ::UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles,
                                    sizeof(handles), nullptr, nullptr) != 0;
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES; // deliberately NOT STARTF_USESHOWWINDOW
    startup.StartupInfo.hStdInput = handles[0];
    startup.StartupInfo.hStdOutput = handles[1];
    startup.StartupInfo.hStdError = handles[1];
    startup.lpAttributeList = attributes;
    const bool created = listed && ::CreateProcessW(nullptr, command_line.data(), nullptr, nullptr,
                                                    TRUE, EXTENDED_STARTUPINFO_PRESENT, nullptr,
                                                    nullptr, &startup.StartupInfo, &info) != 0;
    const DWORD error = ::GetLastError();
    ::DeleteProcThreadAttributeList(attributes);
    ::SetLastError(error);
    return created;
}

bool create_with_pipe(std::wstring &command_line, HANDLE output_write, HANDLE input,
                      PROCESS_INFORMATION &info) {
    HANDLE handles[2] = {input, output_write};
    return create_with_handle_list(command_line, handles, info);
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
