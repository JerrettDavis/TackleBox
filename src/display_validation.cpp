#include "display_validation.h"

namespace {

static void add_issue(
    DisplayValidationReport *report,
    DisplayValidationIssueKind kind,
    int16_t rect_index,
    int16_t other_rect_index)
{
    if ((report == 0) || (report->issueCount >= DISPLAY_VALIDATION_MAX_ISSUES))
    {
        return;
    }

    report->issues[report->issueCount++] = {kind, rect_index, other_rect_index};
}

static uint8_t rect_intersects(const DisplayValidationRect &lhs, const DisplayValidationRect &rhs)
{
    const uint16_t lhs_right = (uint16_t)lhs.x + (uint16_t)lhs.width;
    const uint16_t lhs_bottom = (uint16_t)lhs.y + (uint16_t)lhs.height;
    const uint16_t rhs_right = (uint16_t)rhs.x + (uint16_t)rhs.width;
    const uint16_t rhs_bottom = (uint16_t)rhs.y + (uint16_t)rhs.height;

    if ((lhs.width == 0U) || (lhs.height == 0U) || (rhs.width == 0U) || (rhs.height == 0U))
    {
        return 0U;
    }

    return (lhs.x < rhs_right) && (rhs.x < lhs_right) && (lhs.y < rhs_bottom) && (rhs.y < lhs_bottom) ? 1U : 0U;
}

static uint8_t rect_contains(const DisplayValidationRect &outer, const DisplayValidationRect &inner)
{
    const uint16_t outer_right = (uint16_t)outer.x + (uint16_t)outer.width;
    const uint16_t outer_bottom = (uint16_t)outer.y + (uint16_t)outer.height;
    const uint16_t inner_right = (uint16_t)inner.x + (uint16_t)inner.width;
    const uint16_t inner_bottom = (uint16_t)inner.y + (uint16_t)inner.height;

    return (inner.x >= outer.x) && (inner.y >= outer.y) && (inner_right <= outer_right) && (inner_bottom <= outer_bottom) ? 1U : 0U;
}

static uint8_t rect_is_ancestor(const DisplayValidationReport *report, int16_t ancestor_index, int16_t rect_index)
{
    if ((report == 0) || (ancestor_index < 0) || (rect_index < 0))
    {
        return 0U;
    }

    while (rect_index >= 0)
    {
        if (report->rects[rect_index].parentIndex == ancestor_index)
        {
            return 1U;
        }
        rect_index = report->rects[rect_index].parentIndex;
    }

    return 0U;
}

}

void display_validation_reset(DisplayValidationReport *report)
{
    if (report == 0)
    {
        return;
    }

    report->rectCount = 0U;
    report->issueCount = 0U;
}

int16_t display_validation_add_rect(
    DisplayValidationReport *report,
    const char *name,
    uint8_t x,
    uint8_t y,
    uint8_t width,
    uint8_t height,
    int16_t parentIndex,
    uint8_t allowOverlap)
{
    if ((report == 0) || (report->rectCount >= DISPLAY_VALIDATION_MAX_RECTS))
    {
        return -1;
    }

    const int16_t rect_index = (int16_t)report->rectCount;
    const DisplayValidationRect rect = {name, x, y, width, height, parentIndex, allowOverlap};
    report->rects[report->rectCount++] = rect;

    if (((uint16_t)x + (uint16_t)width) > DISPLAY_VALIDATION_WIDTH ||
        ((uint16_t)y + (uint16_t)height) > DISPLAY_VALIDATION_HEIGHT)
    {
        add_issue(report, DisplayValidationIssueKind::OutOfBounds, rect_index, -1);
    }

    if (parentIndex >= 0)
    {
        if ((uint16_t)parentIndex >= (report->rectCount - 1U))
        {
            add_issue(report, DisplayValidationIssueKind::InvalidNesting, rect_index, parentIndex);
        }
        else if (rect_contains(report->rects[parentIndex], rect) == 0U)
        {
            add_issue(report, DisplayValidationIssueKind::InvalidNesting, rect_index, parentIndex);
        }
    }

    for (int16_t index = 0; index < rect_index; ++index)
    {
        const DisplayValidationRect &other = report->rects[index];
        if (rect_intersects(rect, other) == 0U)
        {
            continue;
        }

        if ((rect.allowOverlap != 0U) || (other.allowOverlap != 0U))
        {
            continue;
        }

        if ((rect_is_ancestor(report, index, rect_index) != 0U) || (rect_is_ancestor(report, rect_index, index) != 0U))
        {
            continue;
        }

        add_issue(report, DisplayValidationIssueKind::Overlap, rect_index, index);
    }

    return rect_index;
}