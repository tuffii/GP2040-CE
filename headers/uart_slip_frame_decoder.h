#ifndef _UART_SLIP_FRAME_DECODER_H_
#define _UART_SLIP_FRAME_DECODER_H_

#include <cstdint>
#include <cstddef>

class SlipFrameDecoder {
public:
    static constexpr size_t MAX_FRAME_SIZE = 512;

    enum class Result {
        NONE,
        FRAME_OK,
        FRAME_BAD
    };

    SlipFrameDecoder();

    Result push(uint8_t byte);

    const uint8_t* frameData() const;
    size_t frameSize() const;

    void reset();

    uint32_t calculateCRC32(const uint8_t* data, size_t len);

private:
    uint8_t buffer[MAX_FRAME_SIZE];
    size_t index;
    bool escaped;
    size_t lastFrameSize;
};

#endif
