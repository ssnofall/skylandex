#pragma once

#include <notification/notification.h>

typedef struct {
    const NotificationSequence* solid;
    const NotificationSequence* blink_start;
    const NotificationSequence* blink_10;
} SkylandexFeedbackElement;

void skylandex_feedback_reset(NotificationApp* notifications);

void skylandex_feedback_scan_waiting(NotificationApp* notifications);
void skylandex_feedback_scan_success(NotificationApp* notifications, const char* element);
void skylandex_feedback_scan_unknown(NotificationApp* notifications);
void skylandex_feedback_scan_fail(NotificationApp* notifications);

void skylandex_feedback_save_ok(NotificationApp* notifications);
void skylandex_feedback_save_fail(NotificationApp* notifications);

void skylandex_feedback_emulate_start(NotificationApp* notifications, const char* element);
void skylandex_feedback_emulate_stop(NotificationApp* notifications);

SkylandexFeedbackElement skylandex_feedback_element_pick(const char* element);
