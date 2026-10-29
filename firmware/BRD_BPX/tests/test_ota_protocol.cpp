/* [V1.2 新增] 測試實際 Session；只以記憶體代替 MCU Flash。 */
#include "brd_ota_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <vector>
using namespace brd_ota;
struct Flash : Sink {
    std::vector<uint8_t> image;
    unsigned begins = 0, writes = 0, finishes = 0, aborts = 0;
    bool begin_ok = true, write_ok = true, finish_ok = true;
    uint32_t capacity() const override { return 0x140000U; }
    bool begin(uint32_t, const uint8_t[16]) override { begins++; image.clear(); return begin_ok; }
    bool write(const uint8_t *d, size_t n) override {
        writes++; if (!write_ok) { return false; } image.insert(image.end(), d, d + n); return true;
    }
    bool finish() override { finishes++; return finish_ok; }
    void abort() override { aborts++; }
};
static const Policy ready = {true, true, false, 240};
static std::vector<uint8_t> image() {
    std::vector<uint8_t> bytes(173U, 0x33U);
    bytes[0] = 0xE9U; bytes[12] = 5U; bytes[13] = 0U;
    bytes[32] = 0x32U; bytes[33] = 0x54U; bytes[34] = 0xCDU; bytes[35] = 0xABU;
    memcpy(bytes.data() + 63U, IMAGE_TAG, strlen(IMAGE_TAG));
    return bytes;
}
static Response command(Session &s, uint8_t op, uint8_t id = 7, uint32_t value = 0,
                        const Policy &p = ready, uint32_t now = 10) {
    uint8_t b[18] = {op, id}; size_t n = 2;
    if (op == HASH) { n = 18; }
    if (op == BEGIN) {
        n = 6; for (unsigned i = 0; i < 4; i++) { b[2 + i] = (value >> (8U * i)) & 255U; }
    }
    return s.handle(b, n, now, p);
}
static void begin(Session &s, size_t length, uint32_t now = 10) {
    assert(command(s, HASH, 7, 0, ready, now).result == OK);
    assert(command(s, BEGIN, 7, length, ready, now).result == OK);
    assert(s.phase() == RECEIVING);
}
static Response data(Session &s, const uint8_t *bytes, size_t size, uint32_t offset,
                     uint8_t id = 7, uint32_t now = 20, const Policy &p = ready) {
    std::vector<uint8_t> packet(DATA_HEADER + size);
    packet[0] = DATA; packet[1] = id;
    for (unsigned i = 0; i < 4; i++) { packet[2 + i] = (offset >> (8U * i)) & 255U; }
    memcpy(packet.data() + DATA_HEADER, bytes, size);
    return s.handle(packet.data(), packet.size(), now, p);
}
int main() {
    auto bytes = image();
    {
        Flash f; Session s(f); begin(s, bytes.size());
        assert(data(s, bytes.data(), 100, 0).offset == 100);
        assert(data(s, bytes.data(), 100, 0).result == OK);
        assert(f.writes == 1);
        auto changed = bytes; changed[5]++;
        assert(data(s, changed.data(), 100, 0).result == OFFSET);
        assert(data(s, bytes.data() + 100, 73, 101).result == OFFSET);
        assert(data(s, bytes.data() + 100, 73, 100).offset == 173);
        assert(command(s, END).phase == COMPLETE);
        assert(command(s, END).result == OK);
        assert(f.finishes == 1 && f.image == bytes);
        assert(command(s, ABORT).result == BUSY);
        s.disconnect(); assert(s.phase() == COMPLETE && s.busy());
    }
    {
        Flash f; Session s(f);
        assert(command(s, BEGIN, 7, bytes.size()).result == SESSION);
        command(s, HASH);
        assert(command(s, BEGIN, 7, bytes.size(), {false, true, false, 240}).result == BUSY);
        assert(command(s, BEGIN, 7, bytes.size(), {true, false, false, 240}).result == POWER);
        assert(command(s, BEGIN, 7, 0).result == SIZE);
        assert(command(s, BEGIN, 7, f.capacity() + 1).result == SIZE);
        assert(f.begins == 0);
        begin(s, bytes.size());
        assert(command(s, BEGIN, 7, bytes.size()).result == OK && f.begins == 1);
        assert(command(s, HASH, 8).result == BUSY);
        assert(data(s, bytes.data(), 10, 0, 8).result == SESSION);
        assert(command(s, END).result == SIZE);
        assert(command(s, ABORT).phase == IDLE && f.aborts == 1);
    }
    {
        Flash f; Session s(f); begin(s, bytes.size());
        Policy mtu23 = {true, true, false, 14};
        assert(data(s, bytes.data(), 15, 0, 7, 20, mtu23).result == BAD_LENGTH);
        for (size_t offset = 0; offset < bytes.size(); offset += 14) {
            size_t n = bytes.size() - offset; if (n > 14) { n = 14; }
            assert(data(s, bytes.data() + offset, n, offset, 7, 20, mtu23).result == OK);
        }
        assert(command(s, END).result == OK);
        uint8_t encoded[RESPONSE_SIZE];
        encode(s.snapshot(INFO, 7, OK, mtu23), encoded);
        assert(encoded[0] == INFO && encoded[3] == COMPLETE && encoded[4] == 173);
        assert(encoded[16] == 14 && encoded[17] == 0 && encoded[19] == 1);
    }
    for (unsigned broken : {0U, 12U, 32U, 63U}) {
        Flash f; Session s(f); auto bad = bytes; bad[broken] ^= 1U;
        begin(s, bad.size()); auto response = data(s, bad.data(), bad.size(), 0);
        if (broken == 63) { assert(command(s, END).result == IMAGE); }
        else { assert(response.result == IMAGE); }
        assert(s.phase() == ERROR && f.finishes == 0 && f.aborts == 1);
    }
    {
        Flash f; Session s(f); f.finish_ok = false; begin(s, bytes.size());
        assert(data(s, bytes.data(), bytes.size(), 0).result == OK);
        assert(command(s, END).result == VERIFY && s.phase() == ERROR);
        assert(f.aborts == 1);
    }
    {
        Flash f; Session s(f); f.write_ok = false; begin(s, bytes.size());
        assert(data(s, bytes.data(), 10, 0).result == WRITE && s.phase() == ERROR);
    }
    {
        Flash f; Session s(f); begin(s, bytes.size(), 0xFFFFFF00U);
        assert(!s.tick(0xFFFFFF00U + TIMEOUT_MS - 1U));
        assert(s.tick(0xFFFFFF00U + TIMEOUT_MS));
        assert(s.phase() == ERROR && f.aborts == 1);
        begin(s, bytes.size()); s.disconnect();
        assert(s.phase() == IDLE && f.aborts == 2);
    }
    {
        Flash f; Session s(f); uint8_t tiny[1] = {BEGIN};
        assert(s.handle(tiny, 1, 0, ready).result == BAD_LENGTH);
        assert(s.handle(nullptr, 0, 0, ready).result == BAD_LENGTH);
        assert(command(s, 99).result == BAD_COMMAND);
    }
    /* [V1.2 新增] 批次 SYNC 不變更已接受資料，後續 DATA 不需逐包確認。 */
    {
        Flash f; Session s(f); begin(s, bytes.size());
        assert(!s.batch_mode());
        auto first = command(s, SYNC);
        assert(first.result == OK && first.offset == 0 && s.batch_mode());
        assert((first.flags & FEATURE_BATCH_ACK) != 0);
        assert(data(s, bytes.data(), 50, 0).offset == 50);
        assert(data(s, bytes.data() + 50, 50, 50).offset == 100);
        assert(data(s, bytes.data() + 100, 73, 100).offset == 173);
        auto ack = command(s, SYNC);
        assert(ack.result == OK && ack.offset == 173);
        assert(data(s, bytes.data(), 50, 0).result == OFFSET);
        assert(command(s, END).phase == COMPLETE);
        assert(f.image == bytes);
    }
    {
        Flash f; Session s(f); begin(s, bytes.size());
        assert(command(s, SYNC).result == OK);
        assert(data(s, bytes.data(), 50, 0).offset == 50);
        assert(data(s, bytes.data() + 100, 73, 100).result == OFFSET);
        assert(command(s, SYNC).offset == 50);
        assert(data(s, bytes.data() + 50, 50, 50).offset == 100);
        assert(data(s, bytes.data() + 100, 73, 100).offset == 173);
        assert(command(s, END).phase == COMPLETE);
        assert(f.image == bytes);
    }
    puts("PASS: OTA sessions, image/size/power gates, exact retries, MTU=23, abort, timeout/wrap and verification failure.");
}
