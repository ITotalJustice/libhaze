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

#include <haze/async_usb_server.hpp>
#include <haze/common.hpp>
#include <haze/ptp.hpp>

namespace haze {

    class PtpDataParser final {
        private:
            AsyncUsbServer *m_server;
            u32 m_received_size;
            u32 m_offset;
            u8 *m_data;
            bool m_eot;
        private:
            Result Flush(u8* buffer, u32 size, u32* out_size) {
                R_UNLESS(!m_eot, haze::ResultEndOfTransmission());

                *out_size = 0;
                m_received_size = 0;
                m_offset = 0;

                ON_SCOPE_EXIT {
                    /* End of transmission occurs when receiving a bulk transfer less than the buffer size. */
                    /* PTP uses zero-length termination, so zero is a possible size to receive. */
                    m_eot = *out_size < size;
                    if (m_eot) {
                        log_write("End of transmission detected (received %u bytes)\n", *out_size);
                    }
                };

                R_RETURN(m_server->ReadPacket(buffer, size, out_size));
            }

            Result Flush() {
                R_RETURN(this->Flush(m_data, haze::UsbBulkPacketBufferSize, std::addressof(m_received_size)));
            }
        public:
            constexpr explicit PtpDataParser(void *data, AsyncUsbServer *server) : m_server(server), m_received_size(), m_offset(), m_data(static_cast<u8 *>(data)), m_eot() { /* ... */ }

            Result Finalize() {
                /* Read until the transmission completes. */
                while (true) {
                    Result rc = this->Flush();

                    R_SUCCEED_IF(m_eot || haze::ResultEndOfTransmission::Includes(rc));
                    R_TRY(rc);
                }
            }

            Result ReadBuffer(u8 *buffer, u32 count, u32 *out_read_count) {
                *out_read_count = 0;

                while (count > 0) {
                    /* If we cannot read more bytes now, flush. */
                    if (m_offset == m_received_size) {
                        log_write("ReadBuffer: flushing to get more data: %u\n", count);
                        R_TRY(this->Flush());
                        log_write("ReadBuffer: flushed, got %u bytes left: %u\n", m_received_size, count);
                    }

                    /* Calculate how many bytes we can read now. */
                    u32 read_size = std::min<u32>(count, m_received_size - m_offset);

                    /* Read this number of bytes. */
                    std::memcpy(buffer + *out_read_count, m_data + m_offset, read_size);
                    *out_read_count += read_size;
                    m_offset += read_size;
                    count -= read_size;
                }

                R_SUCCEED();
            }

            // buffer must be page aligned.
            // as well as the buffer internal size being page aligned.
            // count must at least the size of max USB packet size (unless its the last read).
            // it's best to always read in multiples of 1024 as this is the max packet size for USB 3.0.
            // this function should only be used to prevent windows from freezing betweeen transfers
            // if the previous usb transfer took longer than 3s.
            // caller should read 1024 bytes and sleep 1-100ms between reads until the write buffer
            // has space.
            // in which case, use the above ReadBuffer as normal, or continue to use this function.
            Result ReadBufferInPlace(u8 *buffer, u32 count, u32 *out_read_count) {
                *out_read_count = 0;

                // enable is debug builds only, we don't want to fatal in release builds.
                // todo: replace all asserts with proper error handling, we should NEVER fatal.
                // HAZE_ASSERT(!util::IsAligned((u64)buffer, 0x1000));
                R_UNLESS(util::IsAligned((u64)buffer, 0x1000), haze::ResultBufferNotAligned());
                R_RETURN(this->Flush(buffer, count, out_read_count));
            }

            template <typename T>
            Result Read(T *out_t) {
                u32 read_count;
                u8 bytes[sizeof(T)];

                R_TRY(this->ReadBuffer(bytes, sizeof(T), std::addressof(read_count)));

                std::memcpy(out_t, bytes, sizeof(T));

                R_SUCCEED();
            }

            /* NOTE: out_string must contain room for 256 bytes. */
            /* The result will be null-terminated on successful completion. */
            Result ReadString(char *out_string) {
                u8 len;
                R_TRY(this->Read(std::addressof(len)));

                /* Read characters one by one. */
                for (size_t i = 0; i < len; i++) {
                    u16 chr;
                    R_TRY(this->Read(std::addressof(chr)));

                    *out_string++ = static_cast<char>(chr);
                }

                /* Write null terminator. */
                *out_string++ = '\x00';

                R_SUCCEED();
            }
    };

}
