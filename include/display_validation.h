#ifndef DISPLAY_VALIDATION_H
#define DISPLAY_VALIDATION_H

#include <stdint.h>

static const uint8_t DISPLAY_VALIDATION_WIDTH = 128U;
static const uint8_t DISPLAY_VALIDATION_HEIGHT = 64U;
static const uint16_t DISPLAY_VALIDATION_MAX_RECTS = 96U;
static const uint16_t DISPLAY_VALIDATION_MAX_ISSUES = 96U;

enum class DisplayValidationIssueKind : uint8_t {
    None = 0,
    OutOfBounds,
    Overlap,
    InvalidNesting,
};

struct DisplayValidationRect {
    const char *name;
    uint8_t x;
    uint8_t y;
    uint8_t width;
    uint8_t height;
    int16_t parentIndex;
    uint8_t allowOverlap;
};

struct DisplayValidationIssue {
    DisplayValidationIssueKind kind;
    int16_t rectIndex;
    int16_t otherRectIndex;
};

struct DisplayValidationReport {
    DisplayValidationRect rects[DISPLAY_VALIDATION_MAX_RECTS];
    uint16_t rectCount;
    DisplayValidationIssue issues[DISPLAY_VALIDATION_MAX_ISSUES];
    uint16_t issueCount;
};

void display_validation_reset(DisplayValidationReport *report);
int16_t display_validation_add_rect(
    DisplayValidationReport *report,
    const char *name,
    uint8_t x,
    uint8_t y,
    uint8_t width,
    uint8_t height,
    int16_t parentIndex,
    uint8_t allowOverlap);

#endif