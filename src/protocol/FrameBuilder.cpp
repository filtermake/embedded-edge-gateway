#include "FrameBuilder.h"
#include "CRC16.h"

namespace gateway {

std::vector<uint8_t> buildFrame(uint8_t type, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> frame;
    frame.reserve(6 + payload.size());
    CRC16 crc;

    // 帧头:只 push,不进 CRC
    frame.push_back(0xAA);
    frame.push_back(0x55);

    // LEN:进 CRC
    uint8_t len = (uint8_t)(1 + payload.size());
    frame.push_back(len);
    crc.update(len);

    // TYPE:进 CRC
    frame.push_back(type);
    crc.update(type);

    // payload:进 CRC
    for (uint8_t b : payload) {
        frame.push_back(b);
        crc.update(b);
    }

    // CRC 小端写:先 LO 后 HI
    uint16_t c = crc.value();
    frame.push_back((uint8_t)(c & 0xFF));
    frame.push_back((uint8_t)(c >> 8));

    return frame;
}

}