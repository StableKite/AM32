#ifdef AT32PB4_540_F421
#include "dshot_melody.h"

enum {
    SYMBOL_ZERO = 30,
    SYMBOL_ONE = 31,
    SYNC = 0xA6,
    ENTER = 1,
    EXIT = 2,
    RESET = 3,
    NOTE = 0x10,
    SYMBOL_TIMEOUT_MS = 50,
    RESET_DELAY_MS = 400,
    IDLE_TIMEOUT_MS = 750,
    SESSION_TIMEOUT_MS = 8000
};

extern uint8_t get_crc8(uint8_t *bytes, uint8_t length);

static uint8_t packet[6], shift, bits;
static uint32_t last_symbol, last_packet, session_start, note_start, reset_at;
static uint16_t duration;
static volatile bool active;
static melody_action_t stop;
static struct {
    uint16_t frequency;
    uint16_t duration;
    uint8_t volume;
} pending;

bool dshotMelodyActive(void)
{
    return active;
}

melody_input_t dshotMelodyReceive(uint16_t value, bool telemetry, bool idle, uint32_t now)
{
    if (!telemetry || (value != SYMBOL_ZERO && value != SYMBOL_ONE)) {
        shift = bits = 0;
        if (!active) {
            return MELODY_IGNORED;
        }
        if (stop == MELODY_UNCHANGED) {
            stop = MELODY_EXIT;
        }
        return MELODY_HANDLED;
    }

    if (now - last_symbol >= SYMBOL_TIMEOUT_MS) {
        shift = bits = 0;
    }
    last_symbol = now;
    shift = (uint8_t)((shift << 1) | (value == SYMBOL_ONE));
    if (!bits) {
        if (shift == SYNC) {
            packet[0] = SYNC;
            bits = 8;
        }
        return MELODY_HANDLED;
    }
    bits++;
    if ((bits & 7) == 0) {
        packet[bits / 8 - 1] = shift;
    }
    if (bits != 8 * sizeof(packet)) {
        return MELODY_HANDLED;
    }
    shift = bits = 0;
    if (packet[5] != get_crc8(packet, 5)) {
        return MELODY_HANDLED;
    }

    const uint8_t command = packet[1];
    const uint16_t frequency = (uint16_t)((uint16_t)packet[2] << 8 | packet[3]);
    const uint8_t length = packet[4];
    if (command == ENTER && !active && idle && !frequency && !length) {
        active = true;
        session_start = last_packet = now;
        return MELODY_ENTERED;
    }
    if (!active || stop != MELODY_UNCHANGED) {
        return MELODY_HANDLED;
    }
    if ((command == EXIT || command == RESET) && !frequency && !length) {
        stop = command == RESET ? MELODY_RESET : MELODY_EXIT;
        reset_at = now;
    } else if ((command & 0xF0) == NOTE && (command & 0x0F) <= 5 && !pending.duration &&
               length >= 1 && length <= 25 &&
               ((frequency >= 220 && frequency <= 4000) || (!frequency && command == NOTE))) {
        pending.frequency = frequency;
        pending.duration = (uint16_t)(length * 10);
        pending.volume = command & 0x0F;
        last_packet = now;
    }
    return MELODY_HANDLED;
}

melody_output_t dshotMelodyPoll(uint32_t now)
{
    if (!active) {
        return (melody_output_t){MELODY_UNCHANGED, 0, 0};
    }
    if (stop == MELODY_RESET && now - reset_at < RESET_DELAY_MS) {
        const melody_action_t action = duration ? MELODY_NOTE : MELODY_UNCHANGED;
        pending.duration = duration = 0;
        return (melody_output_t){action, 0, 0};
    }
    if (stop != MELODY_UNCHANGED || now - last_packet >= IDLE_TIMEOUT_MS ||
        now - session_start >= SESSION_TIMEOUT_MS) {
        const melody_action_t action = stop == MELODY_RESET ? MELODY_RESET : MELODY_EXIT;
        active = false;
        pending.duration = duration = 0;
        shift = bits = 0;
        stop = MELODY_UNCHANGED;
        return (melody_output_t){action, 0, 0};
    }
    if (duration && now - note_start < duration) {
        return (melody_output_t){MELODY_UNCHANGED, 0, 0};
    }
    if (pending.duration) {
        note_start = now;
        duration = pending.duration;
        pending.duration = 0;
        return (melody_output_t){MELODY_NOTE, pending.frequency, pending.volume};
    }
    if (duration) {
        duration = 0;
        return (melody_output_t){MELODY_NOTE, 0, 0};
    }
    return (melody_output_t){MELODY_UNCHANGED, 0, 0};
}
#endif
