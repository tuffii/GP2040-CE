#include "uart_slip_frame_decoder.h"
// #include "uart_protocol.h"
#include "uart_slip.h"

SlipFrameDecoder::SlipFrameDecoder() {
    reset();
}

void SlipFrameDecoder::reset() {
    index = 0;
    escaped = false;
    lastFrameSize = 0;
}

SlipFrameDecoder::Result SlipFrameDecoder::push(uint8_t byte) {
    // 1. Обработка escape-состояния
    if (escaped) {
        if (index < MAX_FRAME_SIZE) {
            if (byte == SLIP_ESC_END)      buffer[index++] = SLIP_END;
            else if (byte == SLIP_ESC_ESC) buffer[index++] = SLIP_ESC;
            else                      buffer[index++] = byte;
        }
        escaped = false;
        return Result::NONE;
    }

    // 2. Управляющие байты
    if (byte == SLIP_ESC) {
        escaped = true;
        return Result::NONE;
    }

    if (byte == SLIP_END) {
        // Конец кадра
        if (index < 5) {
            // слишком короткий — сбрасываем
            reset();
            return Result::NONE;
        }

        // CRC32: последние 4 байта, little-endian
        uint32_t received =
            (uint32_t)buffer[index - 4] |
            ((uint32_t)buffer[index - 3] << 8) |
            ((uint32_t)buffer[index - 2] << 16) |
            ((uint32_t)buffer[index - 1] << 24);

        uint32_t calculated = calculateCRC32(buffer, index - 4);

        if (received == calculated) {
            lastFrameSize = index - 4;
            index = 0;
            return Result::FRAME_OK;
        } else {
            reset();
            return Result::FRAME_BAD;
        }
    }

    // 3. Обычный байт данных
    if (index < MAX_FRAME_SIZE) {
        buffer[index++] = byte;
    } else {
        // переполнение — сбрасываемся
        reset();
    }

    return Result::NONE;
}

const uint8_t* SlipFrameDecoder::frameData() const {
    return buffer;
}

size_t SlipFrameDecoder::frameSize() const {
    return lastFrameSize;
}

uint32_t SlipFrameDecoder::calculateCRC32(const uint8_t* buf, size_t len) {
    uint32_t c = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        c = crc_table[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFF;
}
