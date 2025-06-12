/*
 * Copyright (c) Atmosphère-NX
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <haze.hpp>
#include <haze/console_main_loop.hpp>
#include <mutex>

namespace haze {
namespace {

Thread g_haze_thread{};
std::mutex g_mutex;
std::stop_source g_stop_source{};
bool g_is_running{};
Callback g_callback{};
int g_cpuid{};
int g_prio{};
FsEntries g_entries{};

void thread_func(void* arg) {
    haze::ConsoleMainLoop::RunApplication(g_stop_source.get_token(), g_callback, g_prio, g_cpuid, g_entries);
}

} // namespace

bool Initialize(Callback callback, int prio, int cpuid, const FsEntries& entries) {
    std::scoped_lock lock{g_mutex};
    if (g_is_running) {
        return false;
    }

    if (entries.empty()) {
        return false;
    }

    /* Reset stop token */
    g_stop_source = {};

    /* Load device firmware version and serial number. */
    HAZE_R_ABORT_UNLESS(haze::LoadDeviceProperties());

    g_callback = callback;
    g_prio = prio;
    g_cpuid = cpuid;
    g_entries = entries;

    /* Run the application. */
    if (R_FAILED(threadCreate(&g_haze_thread, thread_func, nullptr, nullptr, 1024*32, prio, cpuid))) {
        return false;
    }

    if (R_FAILED(threadStart(&g_haze_thread))) {
        threadClose(&g_haze_thread);
        return false;
    }

    return g_is_running = true;
}

void Exit() {
    std::scoped_lock lock{g_mutex};
    if (!g_is_running) {
        return;
    }

    g_stop_source.request_stop();
    threadWaitForExit(&g_haze_thread);
    threadClose(&g_haze_thread);
    g_is_running = false;
    g_callback = nullptr;
    g_entries.clear();
}

} // namespace haze
