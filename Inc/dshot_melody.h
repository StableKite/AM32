#ifndef DSHOT_MELODY_H
#define DSHOT_MELODY_H

#include <stdbool.h>
#include <stdint.h>

typedef enum { MELODY_IGNORED, MELODY_HANDLED, MELODY_ENTERED } melody_input_t;

typedef enum { MELODY_UNCHANGED, MELODY_NOTE, MELODY_EXIT, MELODY_RESET } melody_action_t;

typedef struct {
    melody_action_t action;
    uint16_t frequency;
    uint8_t volume;
} melody_output_t;

bool dshotMelodyActive(void);
melody_input_t dshotMelodyReceive(uint16_t value, bool telemetry, bool idle, uint32_t now);
melody_output_t dshotMelodyPoll(uint32_t now);

#endif
