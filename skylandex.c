#include <furi.h>
#include <furi/core/timer.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>

#include "character_db.h"
#include "collection.h"
#include "skylandex_feedback.h"
#include "skylander_nfc.h"
#include "skylander_reader.h"
#include <nfc/nfc_device.h>
#include <nfc/nfc_listener.h>
#include <nfc/protocols/mf_classic/mf_classic.h>

// scenes
typedef enum {
    SceneMenu,
    SceneScan,
    SceneCollection,
    SceneDetail,
    SceneEmulate,
    SceneCount,
} SceneId;

// views
typedef enum {
    ViewMenu,
    ViewScan,
    ViewCollectionEmpty,
    ViewCollectionList,
    ViewDetail,
    ViewEmulate,
} ViewId;

// events
typedef enum {
    AppEventTagDetected = 100,
    AppEventSaveFigure,
    AppEventScanBack,
    AppEventCollectionSelect,
    AppEventEmulateToggle,
    AppEventEmulateStop,
    AppEventEmulateTick,
} AppEventType;

// app state
typedef struct {
    Gui* gui;
    NotificationApp* notifications;
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Submenu* menu;
    Submenu* collection_menu;
    Widget* scan_widget;
    Widget* collection_empty_widget;
    Widget* detail_widget;
    Widget* emulate_widget;
    SkylanderReader* reader;
    Collection* collection;
    NfcDevice* emulate_device;
    NfcListener* emulator;
    ScanDetectionInfo detection_info;
    ScanResult last_scan;
    char matched_name[32];
    char matched_element[16];
    char status_message[32];
    bool has_character_match;
    bool has_scan_result;
    bool has_detection_info;
    bool skylander_read_attempted;
    bool scan_scene_active;
    bool is_emulating;
    uint8_t emulate_anim_frame;
    FuriTimer* emulate_timer;
    uint16_t selected_collection_index;
} SkylandexApp;

// forward scene handlers
void scene_menu_on_enter(void* context);
bool scene_menu_on_event(void* context, SceneManagerEvent event);
void scene_menu_on_exit(void* context);

void scene_scan_on_enter(void* context);
bool scene_scan_on_event(void* context, SceneManagerEvent event);
void scene_scan_on_exit(void* context);

void scene_collection_on_enter(void* context);
bool scene_collection_on_event(void* context, SceneManagerEvent event);
void scene_collection_on_exit(void* context);

void scene_detail_on_enter(void* context);
bool scene_detail_on_event(void* context, SceneManagerEvent event);
void scene_detail_on_exit(void* context);

void scene_emulate_on_enter(void* context);
bool scene_emulate_on_event(void* context, SceneManagerEvent event);
void scene_emulate_on_exit(void* context);

// scene table
static void (*const on_enter_handlers[])(void*) = {
    scene_menu_on_enter,
    scene_scan_on_enter,
    scene_collection_on_enter,
    scene_detail_on_enter,
    scene_emulate_on_enter,
};

static bool (*const on_event_handlers[])(void*, SceneManagerEvent) = {
    scene_menu_on_event,
    scene_scan_on_event,
    scene_collection_on_event,
    scene_detail_on_event,
    scene_emulate_on_event,
};

static void (*const on_exit_handlers[])(void*) = {
    scene_menu_on_exit,
    scene_scan_on_exit,
    scene_collection_on_exit,
    scene_detail_on_exit,
    scene_emulate_on_exit,
};

static const SceneManagerHandlers scene_handlers = {
    .on_enter_handlers = on_enter_handlers,
    .on_event_handlers = on_event_handlers,
    .on_exit_handlers = on_exit_handlers,
    .scene_num = SceneCount,
};

// reader init
static bool skylandex_ensure_reader(SkylandexApp* app) {
    if(app->reader != NULL) return true;
    app->reader = skylander_reader_alloc();
    return app->reader != NULL;
}

// stop Skylander NFC emulation
static void skylandex_stop_emulation(SkylandexApp* app) {
    if(app->emulator != NULL) {
        nfc_listener_stop(app->emulator);
        nfc_listener_free(app->emulator);
        app->emulator = NULL;
    }
    if(app->emulate_device != NULL) {
        nfc_device_free(app->emulate_device);
        app->emulate_device = NULL;
    }
    app->is_emulating = false;
}

// display constants
// flipper display is 128x64; hardware buttons occupy the bottom band
#define SKYLANDEX_SCREEN_W 128
#define SKYLANDEX_HEADER_Y 2
#define SKYLANDEX_BODY_Y 14
#define SKYLANDEX_BODY_H 38
#define SKYLANDEX_DETAIL_ELEMENT_Y 14
#define SKYLANDEX_DETAIL_BODY_Y 26
#define SKYLANDEX_DETAIL_BODY_H 32
#define SKYLANDEX_EMULATE_STATUS_Y 32
#define SKYLANDEX_EMULATE_TICK_MS 400

// UI helpers
static void skylandex_widget_add_actions(
    Widget* widget,
    const char* left_label,
    const char* right_label,
    ButtonCallback callback,
    void* context) {
    if(left_label != NULL) {
        widget_add_button_element(widget, GuiButtonTypeLeft, left_label, callback, context);
    }
    if(right_label != NULL) {
        widget_add_button_element(widget, GuiButtonTypeRight, right_label, callback, context);
    }
}

static void skylandex_widget_set_screen(
    Widget* widget,
    const char* header,
    const char* body,
    const char* left_label,
    const char* right_label,
    ButtonCallback callback,
    void* context) {
    widget_reset(widget);
    widget_add_string_element(
        widget, 64, SKYLANDEX_HEADER_Y, AlignCenter, AlignTop, FontPrimary, header);
    widget_add_text_scroll_element(
        widget, 0, SKYLANDEX_BODY_Y, SKYLANDEX_SCREEN_W, SKYLANDEX_BODY_H, body);
    skylandex_widget_add_actions(widget, left_label, right_label, callback, context);
}

static void skylandex_widget_set_screen_with_element(
    Widget* widget,
    const char* header,
    const char* element,
    const char* body,
    const char* left_label,
    const char* right_label,
    ButtonCallback callback,
    void* context) {
    widget_reset(widget);
    widget_add_string_element(
        widget, 64, SKYLANDEX_HEADER_Y, AlignCenter, AlignTop, FontPrimary, header);
    widget_add_string_element(
        widget, 2, SKYLANDEX_DETAIL_ELEMENT_Y, AlignLeft, AlignTop, FontPrimary, element);
    widget_add_text_scroll_element(
        widget, 0, SKYLANDEX_DETAIL_BODY_Y, SKYLANDEX_SCREEN_W, SKYLANDEX_DETAIL_BODY_H, body);
    skylandex_widget_add_actions(widget, left_label, right_label, callback, context);
}

typedef enum {
    MenuItemScan,
    MenuItemCollection,
} MenuItems;

static void menu_item_callback(void* context, uint32_t index) {
    SkylandexApp* app = (SkylandexApp*)context;
    scene_manager_handle_custom_event(app->scene_manager, index);
}

// menu
void scene_menu_on_enter(void* context) {
    SkylandexApp* app = (SkylandexApp*)context;
    submenu_reset(app->menu);
    submenu_set_header(app->menu, "Skylandex");
    submenu_add_item(app->menu, "Scan Skylander", MenuItemScan, menu_item_callback, app);
    submenu_add_item(app->menu, "My Skylandex", MenuItemCollection, menu_item_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewMenu);
}

bool scene_menu_on_event(void* context, SceneManagerEvent event) {
    SkylandexApp* app = (SkylandexApp*)context;
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case MenuItemScan:
            scene_manager_next_scene(app->scene_manager, SceneScan);
            return true;
        case MenuItemCollection:
            scene_manager_next_scene(app->scene_manager, SceneCollection);
            return true;
        }
    }
    return false;
}

void scene_menu_on_exit(void* context) {
    SkylandexApp* app = (SkylandexApp*)context;
    submenu_reset(app->menu);
}

// NOTE: remaining scenes unchanged for brevity (scan/collection/detail/emulate)
// They follow same pattern: input -> NFC read -> UI update -> feedback

// NFC scan callback: triggers scene event
static void scan_reader_callback(void* context) {
    SkylandexApp* app = (SkylandexApp*)context;
    if(!app->scan_scene_active) return;
    view_dispatcher_send_custom_event(app->view_dispatcher, AppEventTagDetected);
}

static void scan_button_callback(GuiButtonType type, InputType input_type, void* context) {
    if(input_type != InputTypePress) return;
    SkylandexApp* app = (SkylandexApp*)context;
    if(type == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, AppEventScanBack);
    } else if(type == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, AppEventSaveFigure);
    }
}

static void scan_draw_waiting(SkylandexApp* app) {
    widget_reset(app->scan_widget);
    widget_add_string_element(
        app->scan_widget, 64, SKYLANDEX_HEADER_Y, AlignCenter, AlignTop, FontPrimary, "Skylandex");
    widget_add_string_multiline_element(
        app->scan_widget,
        64,
        SKYLANDEX_BODY_Y + 4,
        AlignCenter,
        AlignTop,
        FontSecondary,
        "Hold Skylander near\n"
        "the Flipper's NFC coil to scan");
    skylandex_widget_add_actions(app->scan_widget, "Back", NULL, scan_button_callback, app);
}

static void scan_draw_result(SkylandexApp* app) {
    const char* uid_text = app->last_scan.uid_available ? app->last_scan.uid_hex : "--------";
    const char* name = app->has_character_match ? app->matched_name : "Unknown";
    const char* element = app->has_character_match ? app->matched_element : "Unknown";

    char header[32];
    char body[384];

    if(app->last_scan.read_ok) {
        snprintf(header, sizeof(header), "%s", name);
        if(app->status_message[0] != '\0') {
            snprintf(
                body,
                sizeof(body),
                "%s\n"
                "\n"
                "ID 0x%04X\n"
                "UID %s",
                app->status_message,
                app->last_scan.character_id,
                uid_text);
        } else {
            snprintf(
                body,
                sizeof(body),
                "ID 0x%04X\n"
                "UID %s",
                app->last_scan.character_id,
                uid_text);
        }
        skylandex_widget_set_screen_with_element(
            app->scan_widget, header, element, body, "Back", "Save", scan_button_callback, app);
    } else if(app->skylander_read_attempted) {
        snprintf(
            body,
            sizeof(body),
            "Sector 0 read failed\n"
            "Try repositioning\n"
            "the figure on the coil.");
        skylandex_widget_set_screen(
            app->scan_widget, "Tag Detected", body, "Back", NULL, scan_button_callback, app);
    } else {
        skylandex_widget_set_screen(
            app->scan_widget,
            "Tag Detected",
            "Reading tag...",
            "Back",
            NULL,
            scan_button_callback,
            app);
    }
}

void scene_scan_on_enter(void* context) {
    SkylandexApp* app = (SkylandexApp*)context;
    if(!skylandex_ensure_reader(app)) {
        scene_manager_previous_scene(app->scene_manager);
        return;
    }
    app->scan_scene_active = true;
    app->has_scan_result = false;
    app->has_detection_info = false;
    app->skylander_read_attempted = false;
    app->has_character_match = false;
    strlcpy(app->matched_name, "", sizeof(app->matched_name));
    strlcpy(app->matched_element, "", sizeof(app->matched_element));
    strlcpy(app->status_message, "", sizeof(app->status_message));
    memset(&app->detection_info, 0, sizeof(app->detection_info));
    memset(&app->last_scan, 0, sizeof(app->last_scan));
    scan_draw_waiting(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewScan);
    skylander_reader_start(app->reader, scan_reader_callback, app);
    skylandex_feedback_scan_waiting(app->notifications);
}

static bool scan_save_figure(SkylandexApp* app) {
    if(!app->last_scan.read_ok || !app->last_scan.uid_available) return false;

    if(collection_contains_uid(app->collection, app->last_scan.uid_hex)) {
        strlcpy(app->status_message, "Already in collection", sizeof(app->status_message));
        return false;
    }

    NfcDevice* device = nfc_device_alloc();
    if(!skylander_reader_build_nfc_device(app->reader, device)) {
        nfc_device_free(device);
        strlcpy(app->status_message, "NFC dump failed", sizeof(app->status_message));
        return false;
    }

    char nfc_path[64];
    const char* save_name = app->has_character_match ? app->matched_name : "Unknown";
    if(!skylander_nfc_build_path(
           nfc_path,
           sizeof(nfc_path),
           app->last_scan.uid_hex,
           app->last_scan.character_id,
           save_name)) {
        nfc_device_free(device);
        strlcpy(app->status_message, "Path error", sizeof(app->status_message));
        return false;
    }

    if(!skylander_nfc_save_device(device, nfc_path)) {
        nfc_device_free(device);
        strlcpy(app->status_message, "Save .nfc failed", sizeof(app->status_message));
        return false;
    }
    nfc_device_free(device);

    const char* element = app->has_character_match ? app->matched_element : "Unknown";
    if(!collection_add(
           app->collection,
           app->last_scan.character_id,
           save_name,
           element,
           app->last_scan.uid_hex,
           nfc_path)) {
        strlcpy(app->status_message, "Collection add failed", sizeof(app->status_message));
        return false;
    }

    if(!collection_save(app->collection)) {
        strlcpy(app->status_message, "Collection save failed", sizeof(app->status_message));
        return false;
    }

    strlcpy(app->status_message, "Saved!", sizeof(app->status_message));
    return true;
}

bool scene_scan_on_event(void* context, SceneManagerEvent event) {
    SkylandexApp* app = (SkylandexApp*)context;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == AppEventTagDetected) {
            if(!app->scan_scene_active) return true;
            skylandex_feedback_reset(app->notifications);
            app->has_detection_info =
                skylander_reader_get_detected_info(app->reader, &app->detection_info);
            app->skylander_read_attempted = true;
            skylander_reader_read_sector0(app->reader, &app->last_scan);
            app->has_character_match = false;
            if(app->last_scan.read_ok && app->last_scan.has_character_id) {
                const SkylanderInfo* info = character_db_lookup(app->last_scan.character_id);
                if(info != NULL) {
                    app->has_character_match = true;
                    strlcpy(app->matched_name, info->name, sizeof(app->matched_name));
                    strlcpy(app->matched_element, info->element, sizeof(app->matched_element));
                }
            }
            app->has_scan_result = app->last_scan.read_ok;
            strlcpy(app->status_message, "", sizeof(app->status_message));
            if(!app->last_scan.read_ok) {
                skylandex_feedback_scan_fail(app->notifications);
            } else if(app->has_character_match) {
                skylandex_feedback_scan_success(app->notifications, app->matched_element);
            } else {
                skylandex_feedback_scan_unknown(app->notifications);
            }
            scan_draw_result(app);
            return true;
        }
        if(event.event == AppEventSaveFigure) {
            if(app->last_scan.read_ok) {
                scan_save_figure(app);
                scan_draw_result(app);
                if(strcmp(app->status_message, "Saved!") == 0) {
                    skylandex_feedback_save_ok(app->notifications);
                    scene_manager_previous_scene(app->scene_manager);
                } else {
                    skylandex_feedback_save_fail(app->notifications);
                }
            }
            return true;
        }
        if(event.event == AppEventScanBack) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void scene_scan_on_exit(void* context) {
    SkylandexApp* app = (SkylandexApp*)context;
    app->scan_scene_active = false;
    skylandex_feedback_reset(app->notifications);
    if(app->reader != NULL) {
        skylander_reader_stop(app->reader);
    }
    widget_reset(app->scan_widget);
}

static void collection_item_callback(void* context, uint32_t index) {
    SkylandexApp* app = (SkylandexApp*)context;
    app->selected_collection_index = (uint16_t)index;
    view_dispatcher_send_custom_event(app->view_dispatcher, AppEventCollectionSelect);
}

void scene_collection_on_enter(void* context) {
    SkylandexApp* app = (SkylandexApp*)context;
    collection_load(app->collection);

    if(app->collection->count == 0) {
        widget_reset(app->collection_empty_widget);
        widget_add_string_element(
            app->collection_empty_widget, 64, 10, AlignCenter, AlignCenter, FontPrimary, "No Skylanders yet");
        widget_add_string_element(
            app->collection_empty_widget,
            64,
            32,
            AlignCenter,
            AlignCenter,
            FontSecondary,
            "Scan a Skylander to start");
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewCollectionEmpty);
        return;
    }

    submenu_reset(app->collection_menu);
    submenu_set_header(app->collection_menu, "My Skylandex");
    for(uint16_t i = 0; i < app->collection->count; i++) {
        char label[40];
        snprintf(
            label,
            sizeof(label),
            "%s - %s",
            app->collection->entries[i].name,
            app->collection->entries[i].element);
        submenu_add_item(app->collection_menu, label, i, collection_item_callback, app);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewCollectionList);
}

bool scene_collection_on_event(void* context, SceneManagerEvent event) {
    SkylandexApp* app = (SkylandexApp*)context;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == AppEventCollectionSelect) {
            scene_manager_next_scene(app->scene_manager, SceneDetail);
            return true;
        }
    }
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void scene_collection_on_exit(void* context) {
    SkylandexApp* app = (SkylandexApp*)context;
    submenu_reset(app->collection_menu);
    widget_reset(app->collection_empty_widget);
}

static void detail_button_callback(GuiButtonType type, InputType input_type, void* context) {
    if(input_type != InputTypePress) return;
    SkylandexApp* app = (SkylandexApp*)context;
    if(type == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, AppEventScanBack);
    } else if(type == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, AppEventEmulateToggle);
    }
}

static void detail_draw(SkylandexApp* app, const CollectionEntry* entry) {
    char header[20];
    char body[384];

    snprintf(header, sizeof(header), "%.18s", entry->name);
    snprintf(
        body,
        sizeof(body),
        "ID 0x%04X\n"
        "UID %s\n"
        "\n"
        "Scanned %s\n",
        entry->character_id,
        entry->uid_hex,
        entry->date_scanned);

    skylandex_widget_set_screen_with_element(
        app->detail_widget,
        header,
        entry->element,
        body,
        "Back",
        "Emulate",
        detail_button_callback,
        app);
}

static void emulate_stop_button_callback(GuiButtonType type, InputType input_type, void* context) {
    UNUSED(type);
    if(input_type != InputTypePress) return;
    SkylandexApp* app = (SkylandexApp*)context;
    view_dispatcher_send_custom_event(app->view_dispatcher, AppEventEmulateStop);
}

static void emulate_redraw(SkylandexApp* app, const CollectionEntry* entry) {
    char header[32];
    char status[24];
    static const char* const dots[] = {"", ".", "..", "..."};

    snprintf(header, sizeof(header), "%.18s", entry->name);
    snprintf(
        status,
        sizeof(status),
        "Emulating%s",
        dots[app->emulate_anim_frame % COUNT_OF(dots)]);

    widget_reset(app->emulate_widget);
    widget_add_string_element(
        app->emulate_widget, 64, SKYLANDEX_HEADER_Y, AlignCenter, AlignTop, FontPrimary, header);
    widget_add_string_element(
        app->emulate_widget,
        2,
        SKYLANDEX_DETAIL_ELEMENT_Y,
        AlignLeft,
        AlignTop,
        FontPrimary,
        entry->element);
    widget_add_string_element(
        app->emulate_widget,
        64,
        SKYLANDEX_EMULATE_STATUS_Y,
        AlignCenter,
        AlignTop,
        FontSecondary,
        status);
    widget_add_button_element(
        app->emulate_widget, GuiButtonTypeCenter, "Stop", emulate_stop_button_callback, app);
}

static void emulate_timer_callback(void* context) {
    SkylandexApp* app = (SkylandexApp*)context;
    view_dispatcher_send_custom_event(app->view_dispatcher, AppEventEmulateTick);
}

static void emulate_timer_start(SkylandexApp* app) {
    if(app->emulate_timer == NULL) {
        app->emulate_timer =
            furi_timer_alloc(emulate_timer_callback, FuriTimerTypePeriodic, app);
    }
    furi_timer_start(app->emulate_timer, furi_ms_to_ticks(SKYLANDEX_EMULATE_TICK_MS));
}

static void emulate_timer_stop(SkylandexApp* app) {
    if(app->emulate_timer != NULL) {
        furi_timer_stop(app->emulate_timer);
    }
}

void scene_detail_on_enter(void* context) {
    SkylandexApp* app = (SkylandexApp*)context;
    const CollectionEntry* entry = collection_get_entry(app->collection, app->selected_collection_index);
    if(entry == NULL) {
        scene_manager_previous_scene(app->scene_manager);
        return;
    }
    detail_draw(app, entry);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewDetail);
}

static bool skylandex_start_emulation(SkylandexApp* app, const CollectionEntry* entry) {
    if(!skylandex_ensure_reader(app)) return false;

    skylander_reader_stop(app->reader);

    app->emulate_device = nfc_device_alloc();
    if(!nfc_device_load(app->emulate_device, entry->nfc_path)) {
        nfc_device_free(app->emulate_device);
        app->emulate_device = NULL;
        return false;
    }

    const NfcDeviceData* mf_data =
        nfc_device_get_data(app->emulate_device, NfcProtocolMfClassic);
    if(mf_data == NULL) {
        skylandex_stop_emulation(app);
        return false;
    }

    app->emulator = nfc_listener_alloc(app->reader->nfc, NfcProtocolMfClassic, mf_data);
    if(app->emulator == NULL) {
        skylandex_stop_emulation(app);
        return false;
    }
    nfc_listener_start(app->emulator, NULL, NULL);
    app->is_emulating = true;
    skylandex_feedback_emulate_start(app->notifications, entry->element);
    return true;
}

static void skylandex_stop_emulation_scene(SkylandexApp* app) {
    emulate_timer_stop(app);
    skylandex_feedback_emulate_stop(app->notifications);
    skylandex_stop_emulation(app);
    app->emulate_anim_frame = 0;
}

bool scene_detail_on_event(void* context, SceneManagerEvent event) {
    SkylandexApp* app = (SkylandexApp*)context;
    const CollectionEntry* entry = collection_get_entry(app->collection, app->selected_collection_index);

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == AppEventEmulateToggle) {
            if(entry != NULL && skylandex_start_emulation(app, entry)) {
                scene_manager_next_scene(app->scene_manager, SceneEmulate);
            }
            return true;
        }
        if(event.event == AppEventScanBack) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void scene_detail_on_exit(void* context) {
    SkylandexApp* app = (SkylandexApp*)context;
    widget_reset(app->detail_widget);
}

void scene_emulate_on_enter(void* context) {
    SkylandexApp* app = (SkylandexApp*)context;
    const CollectionEntry* entry =
        collection_get_entry(app->collection, app->selected_collection_index);

    if(entry == NULL || !app->is_emulating) {
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    app->emulate_anim_frame = 0;
    emulate_redraw(app, entry);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewEmulate);
    emulate_timer_start(app);
}

bool scene_emulate_on_event(void* context, SceneManagerEvent event) {
    SkylandexApp* app = (SkylandexApp*)context;
    const CollectionEntry* entry =
        collection_get_entry(app->collection, app->selected_collection_index);

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == AppEventEmulateTick) {
            if(entry != NULL && app->is_emulating) {
                app->emulate_anim_frame++;
                emulate_redraw(app, entry);
            }
            return true;
        }
        if(event.event == AppEventEmulateStop) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void scene_emulate_on_exit(void* context) {
    SkylandexApp* app = (SkylandexApp*)context;
    skylandex_stop_emulation_scene(app);
    widget_reset(app->emulate_widget);
}

static bool app_back_handler(void* context) {
    SkylandexApp* app = (SkylandexApp*)context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static bool app_custom_event_handler(void* context, uint32_t event) {
    SkylandexApp* app = (SkylandexApp*)context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static void skylandex_app_free(SkylandexApp* app);

static SkylandexApp* skylandex_app_alloc() {
    SkylandexApp* app = malloc(sizeof(SkylandexApp));
    memset(app, 0, sizeof(SkylandexApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->scene_manager = scene_manager_alloc(&scene_handlers, app);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, app_custom_event_handler);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, app_back_handler);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->menu = submenu_alloc();
    app->collection_menu = submenu_alloc();
    app->scan_widget = widget_alloc();
    app->collection_empty_widget = widget_alloc();
    app->detail_widget = widget_alloc();
    app->emulate_widget = widget_alloc();

    view_dispatcher_add_view(app->view_dispatcher, ViewMenu, submenu_get_view(app->menu));
    view_dispatcher_add_view(app->view_dispatcher, ViewScan, widget_get_view(app->scan_widget));
    view_dispatcher_add_view(
        app->view_dispatcher, ViewCollectionEmpty, widget_get_view(app->collection_empty_widget));
    view_dispatcher_add_view(
        app->view_dispatcher, ViewCollectionList, submenu_get_view(app->collection_menu));
    view_dispatcher_add_view(app->view_dispatcher, ViewDetail, widget_get_view(app->detail_widget));
    view_dispatcher_add_view(
        app->view_dispatcher, ViewEmulate, widget_get_view(app->emulate_widget));

    app->collection = collection_alloc();
    if(app->collection == NULL) {
        skylandex_app_free(app);
        return NULL;
    }

    return app;
}

static void skylandex_app_free(SkylandexApp* app) {
    if(app == NULL) return;

    skylandex_stop_emulation(app);
    if(app->reader != NULL) {
        skylander_reader_stop(app->reader);
        skylander_reader_free(app->reader);
        app->reader = NULL;
    }

    view_dispatcher_remove_view(app->view_dispatcher, ViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, ViewScan);
    view_dispatcher_remove_view(app->view_dispatcher, ViewCollectionEmpty);
    view_dispatcher_remove_view(app->view_dispatcher, ViewCollectionList);
    view_dispatcher_remove_view(app->view_dispatcher, ViewDetail);
    view_dispatcher_remove_view(app->view_dispatcher, ViewEmulate);

    submenu_free(app->menu);
    submenu_free(app->collection_menu);
    widget_free(app->scan_widget);
    widget_free(app->collection_empty_widget);
    widget_free(app->detail_widget);
    widget_free(app->emulate_widget);

    if(app->collection != NULL) {
        collection_free(app->collection);
    }

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    if(app->notifications != NULL) {
        skylandex_feedback_reset(app->notifications);
        furi_record_close(RECORD_NOTIFICATION);
        app->notifications = NULL;
    }

    if(app->emulate_timer != NULL) {
        furi_timer_free(app->emulate_timer);
        app->emulate_timer = NULL;
    }

    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t skylandex_app(void* p) {
    UNUSED(p);
    SkylandexApp* app = skylandex_app_alloc();
    if(app == NULL) return -1;
    collection_load(app->collection);
    scene_manager_next_scene(app->scene_manager, SceneMenu);
    view_dispatcher_run(app->view_dispatcher);
    skylandex_app_free(app);
    return 0;
}
