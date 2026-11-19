#include "dshot_melody.h"
#include <assert.h>

uint8_t get_crc8(uint8_t *bytes, uint8_t length)
{
    uint8_t crc = 0;
    for (unsigned i = 0; i < length; i++) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; bit++) {
            crc = (uint8_t)((crc << 1) ^ (crc & 0x80 ? 7 : 0));
        }
    }
    return crc;
}

static uint32_t now;

static melody_input_t send(uint8_t command, uint16_t frequency, uint8_t length, bool idle,
                           bool corrupt)
{
    uint8_t packet[] = {0xA6, command, (uint8_t)(frequency >> 8), (uint8_t)frequency, length, 0};
    packet[5] = get_crc8(packet, 5) ^ corrupt;
    melody_input_t result = MELODY_IGNORED;
    for (unsigned i = 0; i < sizeof(packet); i++) {
        for (uint8_t bit = 0x80; bit; bit >>= 1) {
            result = dshotMelodyReceive(packet[i] & bit ? 31 : 30, true, idle, now++);
        }
    }
    return result;
}

int main(void)
{
    assert(dshotMelodyReceive(48, false, true, now++) == MELODY_IGNORED);
    assert(send(1, 0, 0, false, false) == MELODY_HANDLED && !dshotMelodyActive());
    assert(send(1, 0, 0, true, true) == MELODY_HANDLED && !dshotMelodyActive());
    assert(send(1, 0, 0, true, false) == MELODY_ENTERED && dshotMelodyActive());
    send(0x16, 440, 25, false, false);
    assert(dshotMelodyPoll(now).action == MELODY_UNCHANGED);

    send(0x12, 440, 25, false, false);
    const uint32_t start = now;
    melody_output_t note = dshotMelodyPoll(start);
    assert(note.action == MELODY_NOTE && note.frequency == 440 && note.volume == 2);
    send(0x11, 660, 10, false, false);
    send(0x15, 880, 10, false, false);
    assert(dshotMelodyPoll(start + 249).action == MELODY_UNCHANGED);
    note = dshotMelodyPoll(start + 250);
    assert(note.action == MELODY_NOTE && note.frequency == 660 && note.volume == 1);
    note = dshotMelodyPoll(start + 350);
    assert(note.action == MELODY_NOTE && note.frequency == 0);
    now = start + 351;
    assert(dshotMelodyReceive(220, false, false, now) == MELODY_HANDLED);
    assert(dshotMelodyPoll(now++).action == MELODY_EXIT && !dshotMelodyActive());

    assert(send(1, 0, 0, true, false) == MELODY_ENTERED);
    send(0x15, 4000, 25, false, false);
    note = dshotMelodyPoll(now);
    assert(note.action == MELODY_NOTE && note.frequency == 4000 && note.volume == 5);
    send(3, 0, 0, false, false);
    note = dshotMelodyPoll(now);
    assert(note.action == MELODY_NOTE && note.frequency == 0);
    assert(dshotMelodyPoll(now + 398).action == MELODY_UNCHANGED);
    now += 399;
    assert(dshotMelodyPoll(now++).action == MELODY_RESET && !dshotMelodyActive());

    now = UINT32_MAX - 70;
    assert(send(1, 0, 0, true, false) == MELODY_ENTERED);
    assert(dshotMelodyPoll(now + 748).action == MELODY_UNCHANGED);
    assert(dshotMelodyPoll(now + 749).action == MELODY_EXIT && !dshotMelodyActive());
    return 0;
}
