#include "skylandex_feedback.h"

#include <notification/notification_messages.h>
#include <notification/notification_messages_notes.h>
#include <string.h>

// ascending major arpeggio - success chime
static const NotificationSequence sequence_scan_success_chime = {
    &message_note_c5,
    &message_delay_25,
    &message_note_e5,
    &message_delay_25,
    &message_note_g5,
    &message_delay_25,
    &message_note_c6,
    &message_delay_100,
    &message_sound_off,
    NULL,
};

static const NotificationSequence sequence_pause_100 = {
    &message_delay_100,
    NULL,
};

static const NotificationSequence sequence_blink_start_white = {
    &message_blink_set_color_white,
    &message_blink_start_100,
    NULL,
};

static const NotificationSequence sequence_solid_magenta = {
    &message_red_255,
    &message_green_0,
    &message_blue_255,
    NULL,
};

static const NotificationSequence sequence_solid_cyan = {
    &message_red_0,
    &message_green_255,
    &message_blue_255,
    NULL,
};

static const NotificationSequence sequence_solid_white = {
    &message_red_255,
    &message_green_255,
    &message_blue_255,
    NULL,
};

// reset all active notification effects
void skylandex_feedback_reset(NotificationApp* notifications) {
    if(notifications == NULL) return;
    notification_message(notifications, &sequence_blink_stop);
    notification_message(notifications, &sequence_reset_rgb);
    notification_message(notifications, &sequence_reset_vibro);
    notification_message(notifications, &sequence_reset_sound);
}

// select LED colors/effects based on Skylander element
SkylandexFeedbackElement skylandex_feedback_element_pick(const char* element) {
    SkylandexFeedbackElement out = {
        .solid = &sequence_solid_yellow,
        .blink_start = &sequence_blink_start_yellow,
        .blink_10 = &sequence_blink_yellow_10,
    };

    if(element == NULL) return out;

    if(strcmp(element, "Fire") == 0) {
        out.solid = &sequence_set_only_red_255;
        out.blink_start = &sequence_blink_start_red;
        out.blink_10 = &sequence_blink_red_10;
    } else if(strcmp(element, "Water") == 0) {
        out.solid = &sequence_set_only_blue_255;
        out.blink_start = &sequence_blink_start_blue;
        out.blink_10 = &sequence_blink_blue_10;
    } else if(strcmp(element, "Life") == 0) {
        out.solid = &sequence_set_only_green_255;
        out.blink_start = &sequence_blink_start_green;
        out.blink_10 = &sequence_blink_green_10;
    } else if(strcmp(element, "Air") == 0) {
        out.solid = &sequence_solid_yellow;
        out.blink_start = &sequence_blink_start_yellow;
        out.blink_10 = &sequence_blink_yellow_10;
    } else if(strcmp(element, "Earth") == 0) {
        out.solid = &sequence_solid_yellow;
        out.blink_start = &sequence_blink_start_yellow;
        out.blink_10 = &sequence_blink_yellow_10;
    } else if(strcmp(element, "Magic") == 0) {
        out.solid = &sequence_solid_magenta;
        out.blink_start = &sequence_blink_start_magenta;
        out.blink_10 = &sequence_blink_magenta_10;
    } else if(strcmp(element, "Undead") == 0) {
        out.solid = &sequence_solid_white;
        out.blink_start = &sequence_blink_start_white;
        out.blink_10 = &sequence_blink_white_100;
    } else if(strcmp(element, "Tech") == 0) {
        out.solid = &sequence_solid_cyan;
        out.blink_start = &sequence_blink_start_cyan;
        out.blink_10 = &sequence_blink_cyan_10;
    }

    return out;
}

// scanning idle animation
void skylandex_feedback_scan_waiting(NotificationApp* notifications) {
    if(notifications == NULL) return;
    skylandex_feedback_reset(notifications);
    // LED blink runs until feedback_reset (tag read or leave scan scene)
    notification_message(notifications, &sequence_blink_start_cyan);
}

// play successful scan feedback
void skylandex_feedback_scan_success(NotificationApp* notifications, const char* element) {
    if(notifications == NULL) return;

    skylandex_feedback_reset(notifications);

    SkylandexFeedbackElement el = skylandex_feedback_element_pick(element);

    notification_message(notifications, &sequence_double_vibro);
    notification_message(notifications, &sequence_scan_success_chime);

    if(el.solid != NULL) {
        notification_message(notifications, el.solid);
    }

    for(uint8_t i = 0; i < 3; i++) {
        if(el.blink_10 != NULL) {
            notification_message(notifications, el.blink_10);
        }
        notification_message(notifications, &sequence_pause_100);
    }

    notification_message(notifications, &sequence_blink_white_100);
    notification_message(notifications, &sequence_reset_rgb);
}

// play feedback for unknown Skylander data
void skylandex_feedback_scan_unknown(NotificationApp* notifications) {
    if(notifications == NULL) return;
    skylandex_feedback_reset(notifications);
    notification_message(notifications, &sequence_semi_success);
    notification_message(notifications, &sequence_single_vibro);
    notification_message(notifications, &sequence_blink_yellow_10);
    notification_message(notifications, &sequence_reset_rgb);
}

// play failed scan feedback
void skylandex_feedback_scan_fail(NotificationApp* notifications) {
    if(notifications == NULL) return;
    skylandex_feedback_reset(notifications);
    notification_message(notifications, &sequence_error);
    notification_message(notifications, &sequence_single_vibro);
    notification_message(notifications, &sequence_blink_red_10);
    notification_message(notifications, &sequence_reset_rgb);
}

// play successful save feedback
void skylandex_feedback_save_ok(NotificationApp* notifications) {
    if(notifications == NULL) return;
    skylandex_feedback_reset(notifications);
    notification_message(notifications, &sequence_scan_success_chime);
    notification_message(notifications, &sequence_blink_green_10);
    notification_message(notifications, &sequence_reset_rgb);
}

// play failed save feedback
void skylandex_feedback_save_fail(NotificationApp* notifications) {
    if(notifications == NULL) return;
    skylandex_feedback_reset(notifications);
    notification_message(notifications, &sequence_error);
}

// start emulation feedback effects
void skylandex_feedback_emulate_start(NotificationApp* notifications, const char* element) {
    if(notifications == NULL) return;
    skylandex_feedback_reset(notifications);

    SkylandexFeedbackElement el = skylandex_feedback_element_pick(element);
    if(el.blink_start != NULL) {
        notification_message(notifications, el.blink_start);
    }
}

// stop emulation feedback effects
void skylandex_feedback_emulate_stop(NotificationApp* notifications) {
    skylandex_feedback_reset(notifications);
}
