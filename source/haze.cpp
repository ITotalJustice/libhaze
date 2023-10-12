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

namespace {

Thread haze_thread{};
std::mutex mutex;
std::stop_source stop_source{};
bool is_running{};
HazeCallback callback{};

void thread_func(void* arg) {
    haze::ConsoleMainLoop::RunApplication(stop_source.get_token(), callback);
}

} // namespace

extern "C" bool hazeInitialize(HazeCallback _callback) {
    std::scoped_lock lock{mutex};
    if (is_running) {
        return false;
    }

    /* Reset stop token */
    stop_source = {};

    /* Load device firmware version and serial number. */
    HAZE_R_ABORT_UNLESS(haze::LoadDeviceProperties());

    callback = _callback;
    /* Run the application. */
    if (R_FAILED(threadCreate(&haze_thread, thread_func, nullptr, nullptr, 1024*32, 0x2C, -2))) {
        return false;
    }

    if (R_FAILED(threadStart(&haze_thread))) {
        threadClose(&haze_thread);
        return false;
    }

    return is_running = true;
}

extern "C" void hazeExit() {
    std::scoped_lock lock{mutex};
    if (!is_running) {
        return;
    }

    stop_source.request_stop();
    threadWaitForExit(&haze_thread);
    threadClose(&haze_thread);
    is_running = false;
}
