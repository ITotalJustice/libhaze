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
#pragma once

#include <haze/event_reactor.hpp>
#include <haze/ptp_object_heap.hpp>
#include <haze/ptp_responder.hpp>
#include <stop_token>

namespace haze {

    class ConsoleMainLoop : public EventConsumer {
        private:
            static constexpr size_t FrameDelayNs = 33'333'333;
        private:
            EventReactor *m_reactor;
            PtpObjectHeap *m_object_heap;

            Thread m_thread;
            UEvent m_event;
            UEvent m_cancel_event;
            std::stop_token m_token;
        private:
            static void Run(void *arg) {
                static_cast<ConsoleMainLoop *>(arg)->Run();
            }

            void Run() {
                int idx;

                while (true) {
                    /* Wait for up to 1 frame delay time to be cancelled. */
                    Waiter cancel_waiter = waiterForUEvent(std::addressof(m_cancel_event));
                    Result rc = waitObjects(std::addressof(idx), std::addressof(cancel_waiter), 1, FrameDelayNs);

                    /* Finish if we were cancelled. */
                    if (R_SUCCEEDED(rc)) {
                        break;
                    }

                    /* Otherwise, signal the console update event. */
                    if (svc::ResultTimedOut::Includes(rc)) {
                        ueventSignal(std::addressof(m_event));
                    }
                }
            }
        public:
            explicit ConsoleMainLoop(std::stop_token token) : m_reactor(), m_thread(), m_event(), m_cancel_event(), m_token{token} { /* ... */ }

            Result Initialize(EventReactor *reactor, PtpObjectHeap *object_heap) {
                /* Register event reactor and heap. */
                m_reactor     = reactor;
                m_object_heap = object_heap;

                /* Initialize events. */
                ueventCreate(std::addressof(m_event), true);
                ueventCreate(std::addressof(m_cancel_event), true);

                /* Create the delay thread with higher priority than the main thread (which runs at priority 0x2c). */
                R_TRY(threadCreate(std::addressof(m_thread), ConsoleMainLoop::Run, this, nullptr, 4_KB, 0x2b, svc::IdealCoreUseProcessValue));

                /* Ensure we close the thread on failure. */
                ON_RESULT_FAILURE { threadClose(std::addressof(m_thread)); };

                /* Connect ourselves to the event loop. */
                R_UNLESS(m_reactor->AddConsumer(this, waiterForUEvent(std::addressof(m_event))), haze::ResultRegistrationFailed());

                /* Start the delay thread. */
                R_RETURN(threadStart(std::addressof(m_thread)));
            }

            void Finalize() {
                /* Signal the delay thread to shut down. */
                ueventSignal(std::addressof(m_cancel_event));

                /* Wait for the delay thread to exit and close it. */
                HAZE_R_ABORT_UNLESS(threadWaitForExit(std::addressof(m_thread)));

                HAZE_R_ABORT_UNLESS(threadClose(std::addressof(m_thread)));

                /* Disconnect from the event loop.*/
                m_reactor->RemoveConsumer(this);
            }
        protected:
            void ProcessEvent() override {
                /* Pump applet events, and check if exit was requested. */
                if (m_token.stop_requested()) {
                    m_reactor->SetResult(haze::ResultStopRequested());
                }

                /* Check if focus was lost. */
                if (appletGetFocusState() == AppletFocusState_Background) {
                    m_reactor->SetResult(haze::ResultFocusLost());
                }
            }
        private:
            static bool SuspendAndWaitForFocus(std::stop_token token) {
                /* Enable suspension with resume notification. */
                appletSetFocusHandlingMode(AppletFocusHandlingMode_SuspendHomeSleepNotify);

                /* Pump applet events. */
                while (!token.stop_requested()) {
                    /* Check if focus was regained. */
                    if (appletGetFocusState() != AppletFocusState_Background) {
                        return true;
                    }

                    svcSleepThread(FrameDelayNs);
                }

                /* Exit was requested. */
                return false;
            }
        public:
            static void RunApplication(std::stop_token token) {
                /* Declare the object heap, to hold the database for an active session. */
                PtpObjectHeap ptp_object_heap;

                /* Declare the event reactor, and components which use it. */
                EventReactor event_reactor;
                PtpResponder ptp_responder;
                ConsoleMainLoop console_main_loop{token};

                while (true) {
                    /* Disable suspension. */
                    appletSetFocusHandlingMode(AppletFocusHandlingMode_NoSuspend);

                    /* Declare result from serving to use. */
                    Result rc;
                    {
                        /* Ensure we don't go to sleep while transferring files. */
                        appletSetAutoSleepDisabled(true);

                        /* Clear the event reactor. */
                        event_reactor.SetResult(ResultSuccess());

                        /* Configure the PTP responder and console main loop. */
                        ptp_responder.Initialize(std::addressof(event_reactor), std::addressof(ptp_object_heap));
                        console_main_loop.Initialize(std::addressof(event_reactor), std::addressof(ptp_object_heap));

                        /* Ensure we maintain a clean state on exit. */
                        ON_SCOPE_EXIT {
                            /* Finalize the console main loop and PTP responder. */
                            console_main_loop.Finalize();
                            ptp_responder.Finalize();

                            /* Restore auto sleep setting. */
                            appletSetAutoSleepDisabled(false);
                        };

                        /* Begin processing requests. */
                        rc = ptp_responder.LoopProcess();
                    }

                    /* If focus was lost, try to pump the applet main loop until we receive focus again. */
                    if (haze::ResultFocusLost::Includes(rc) && SuspendAndWaitForFocus(token)) {
                        continue;
                    }

                    /* Otherwise, enable suspension and finish. */
                    appletSetFocusHandlingMode(AppletFocusHandlingMode_SuspendHomeSleep);
                    break;
                }
            }
    };

}
