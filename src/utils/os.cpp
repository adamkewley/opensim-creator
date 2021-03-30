#include "os.hpp"

#include "src/log.hpp"

#include <SDL_error.h>
#include <SDL_filesystem.h>
#include <SDL_stdinc.h>

#include <cstring>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

using std::literals::string_literals::operator""s;

static std::filesystem::path convert_sdl_path_to_stdpath(char const* methodname, char* p) {
    if (p == nullptr) {
        std::stringstream ss;
        ss << methodname;
        ss << ": returned null: ";
        ss << SDL_GetError();
        throw std::runtime_error{std::move(ss).str()};
    }

    size_t len = std::strlen(p);

    if (len == 0) {
        std::stringstream ss;
        ss << methodname;
        ss << ": returned an empty string";
        throw std::runtime_error{std::move(ss).str()};
    }

    // remove trailing slash: it interferes with std::filesystem::path
    p[len - 1] = '\0';

    return std::filesystem::path{p};
}

static std::filesystem::path get_current_exe_dir() {
    std::unique_ptr<char, decltype(&SDL_free)> p{SDL_GetBasePath(), SDL_free};
    return convert_sdl_path_to_stdpath("SDL_GetBasePath", p.get());
}

std::filesystem::path const& osmv::current_exe_dir() {
    // can be expensive to compute: cache after first retrieval
    static std::filesystem::path const d = get_current_exe_dir();
    return d;
}

static std::filesystem::path get_user_data_dir() {
    std::unique_ptr<char, decltype(&SDL_free)> p{SDL_GetPrefPath("cbl", "osmv"), SDL_free};
    return convert_sdl_path_to_stdpath("SDL_GetPrefPath", p.get());
}

std::filesystem::path const& osmv::user_data_dir() {
    // can be expensive to compute: cache after first retrieval
    static std::filesystem::path const d = get_user_data_dir();
    return d;
}

#ifdef __LINUX__

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef __USE_GNU
#define __USE_GNU
#endif

#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>
#include <unistd.h>

// TODO
void osmv::write_backtrace_to_log(log::level::Level_enum lvl) {
    void* array[50];
    int size = backtrace(array, 50);
    char** messages = backtrace_symbols(array, size);

    if (messages == nullptr) {
        return;
    }

    for (int i = 0; i < size; ++i) {
        osmv::log::log(lvl, "%s", messages[i]);
    }

    free(messages);
}

/* This structure mirrors the one found in /usr/include/asm/ucontext.h */
typedef struct _sig_ucontext {
    unsigned long uc_flags;
    ucontext_t* uc_link;
    stack_t uc_stack;
    sigcontext uc_mcontext;
    sigset_t uc_sigmask;
} sig_ucontext_t;

[[noreturn]] static void OSMV_critical_error_handler(int sig_num, siginfo_t* info, void* ucontext) {
    sig_ucontext_t* uc = static_cast<sig_ucontext_t*>(ucontext);

    /* Get the address at the time the signal was raised */
#if defined(__i386__)  // gcc specific
    void* caller_address = reinterpret_cast<void*>(uc->uc_mcontext.eip);  // EIP: x86 specific
#elif defined(__x86_64__)  // gcc specific
    void* caller_address = reinterpret_cast<void*>(uc->uc_mcontext.rip);  // RIP: x86_64 specific
#else
#error Unsupported architecture. // TODO: Add support for other arch.
#endif

    fprintf(
        stderr,
        "osmv: critical error: signal %d (%s) received from OS: address is %p from %p\n",
        sig_num,
        strsignal(sig_num),
        info->si_addr,
        caller_address);

    void* array[50];
    int size = backtrace(array, 50);
    array[1] = caller_address;  // overwrite sigaction with caller's address

    char** messages = backtrace_symbols(array, size);

    if (messages == nullptr) {
        exit(EXIT_FAILURE);
    }

    /* skip first stack frame (points here) */
    for (int i = 1; i < size; ++i) {
        fprintf(stderr, "    #%-2d %s\n", i, messages[i]);
    }

    free(messages);

    exit(EXIT_FAILURE);
}

void osmv::install_backtrace_handler() {
    struct sigaction sigact;

    sigact.sa_sigaction = OSMV_critical_error_handler;
    sigact.sa_flags = SA_RESTART | SA_SIGINFO;

    // install segfault handler
    if (sigaction(SIGSEGV, &sigact, nullptr) != 0) {
        fprintf(
            stderr,
            "osmv: warning: could not set signal handler for %d (%s): error reporting may not work as intended\n",
            SIGSEGV,
            strsignal(SIGSEGV));
    }

    // install abort handler: this triggers whenever a non-throwing `assert` causes a termination
    if (sigaction(SIGABRT, &sigact, nullptr) != 0) {
        fprintf(
            stderr,
            "osmv: warning: could not set signal handler for %d (%s): error reporting may not work as intended\n",
            SIGABRT,
            strsignal(SIGABRT));
    }
}

#elif defined(__APPLE__)
#include <execinfo.h>  // backtrace(), backtrace_symbols()
#include <signal.h>  // sigaction(), struct sigaction, strsignal()
#include <stdlib.h>  // exit(), free()

void osmv::write_backtrace_to_log(log::level::Level_enum lvl) {
    void* array[50];
    int size = backtrace(array, 50);
    char** messages = backtrace_symbols(array, size);

    if (messages == nullptr) {
        return;
    }

    for (int i = 0; i < size; ++i) {
        osmv::log::log(lvl, "%s", messages[i]);
    }

    free(messages);
}

[[noreturn]] static void OSMV_critical_error_handler(int sig_num, siginfo_t* info, void* ucontext) {
    osmv::log::error("critical error: signal %d (%s) received from OS", sig_num, strsignal(sig_num));
    osmv::log::error("backtrace:");
    osmv::write_backtrace_to_log(osmv::log::level::err);
    exit(EXIT_FAILURE);
}

void osmv::install_backtrace_handler() {
    struct sigaction sigact;

    sigact.sa_sigaction = OSMV_critical_error_handler;
    sigact.sa_flags = SA_RESTART | SA_SIGINFO;

    // enable SIGSEGV (segmentation fault) handler
    if (sigaction(SIGSEGV, &sigact, nullptr) != 0) {
        log::warn("could not set a signal handler for SIGSEGV: crash error reporting may not work as intended");
    }

    // enable SIGABRT (abort) handler - usually triggers when `assert` fails or std::terminate is called
    if (sigaction(SIGABRT, &sigact, nullptr) != 0) {
        log::warn("could not set a signal handler for SIGABRT: crash error reporting may not work as intended");
    }
}
#else

#include <Windows.h>  // PVOID, RtlCaptureStackBackTrace(), MEMORY_BASIC_INFORMATION, VirtualQuery(), DWORD64, TCHAR, GetModuleFileName()
#include <cinttypes>  // PRIXPTR

void osmv::write_backtrace_to_log(log::level::Level_enum lvl) {
    constexpr size_t skipped_frames = 0;
    constexpr size_t num_frames = 16;

    PVOID return_addrs[num_frames];

    // popupate [0, n) with return addresses (see MSDN)
    USHORT n = RtlCaptureStackBackTrace(skipped_frames, num_frames, return_addrs, nullptr);

    log::log(lvl, "backtrace:");
    for (size_t i = 0; i < n; ++i) {
        // figure out where the address is relative to the start of the page range the address
        // falls in (effectively, where it is relative to the start of the memory-mapped DLL/exe)
        MEMORY_BASIC_INFORMATION bmi;
        VirtualQuery(return_addrs[i], &bmi, sizeof(bmi));
        DWORD64 base_addr = reinterpret_cast<DWORD64>(bmi.AllocationBase);
        static_assert(sizeof(DWORD64) == 8 && sizeof(PVOID) == 8, "review this code - might not work so well on 32-bit systems");

        // use the base address to figure out the file name
        TCHAR module_namebuf[1024];
        GetModuleFileName(reinterpret_cast<HMODULE>(base_addr), module_namebuf, 1024);

        // find the final element in the filename
        TCHAR* cursor = module_namebuf;
        TCHAR* filename_start = cursor;
        {
            while (*cursor != '\0') {
                if (*cursor == '\\') {
                    filename_start = cursor + 1;  // skip the slash
                }
                ++cursor;
            }
        }

        PVOID relative_addr = reinterpret_cast<PVOID>(reinterpret_cast<DWORD64>(return_addrs[i]) - base_addr);

        log::log(lvl, "    #%zu %s+0x%" PRIXPTR " [0x%" PRIXPTR "]", i, filename_start, (uintptr_t)relative_addr, (uintptr_t)return_addrs[i]);
    }
    log::log(
        lvl,
        "note: backtrace addresses are return addresses, not call addresses (see: https://devblogs.microsoft.com/oldnewthing/20170505-00/?p=96116)");
    log::log(lvl, "to analyze the backtrace in WinDbg: `ln osmv.exe+ADDR`");

    // in windbg: ln osmv.exe+ADDR
    // viewing it: https://stackoverflow.com/questions/54022914/c-is-there-any-command-likes-addr2line-on-windows
}

static LONG crash_handler(EXCEPTION_POINTERS* info) {
    osmv::log::error("exception propagated to root of OSMV: might be a segfault?");
    osmv::write_backtrace_to_log(osmv::log::level::err);
    return EXCEPTION_CONTINUE_SEARCH;
}

void osmv::install_backtrace_handler() {
    SetErrorMode(0);  // system default: display all errors
    SetUnhandledExceptionFilter(crash_handler);  // when the application crashes, call this handler
}
#endif
