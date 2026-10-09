#pragma once
#include "Extension/Profile/runtime_internal.h"

namespace dingosdk::profile_runtime {
struct BuildKitLabel { std::string_view id, text; };

// Wardrobe text for slots a mod adds, which the game's string tables cannot have.
// The Misc 1 accessory slot mod names its Accessories tab with this key.
inline constexpr BuildKitLabel cosmetic_labels[]{
    {"ID_CUSTOMIZE_SKATER_ACCESSORIES_MISC1", "Misc 1"},
};

inline constexpr BuildKitLabel buildkit_labels[]{
    {"ID_BK_1", "1"}, {"ID_BK_2", "2"}, {"ID_BK_3", "3"},
    {"ID_BK_15", "15"}, {"ID_BK_30", "30"}, {"ID_BK_45", "45"},
    {"ID_BK_60", "60"}, {"ID_BK_90", "90"},
    {"ID_BK_ACTIVE_AXIS", "Active axis"}, {"ID_BK_ANGLE", "Angle"},
    {"ID_BK_ANGLE_ROTATION_DISABLE_DUE_TO_ANGLE_SNAP", "Turn off angle snap to change the rotation interval."},
    {"ID_BK_ANGLE_SNAP", "Angle snap"}, {"ID_BK_AXIS", "Axis"},
    {"ID_BK_BUILD_KIT_ONLY", "Build Kit only"}, {"ID_BK_CAMERA", "Camera"},
    {"ID_BK_CONFIRM_DELETE_ALL_HEADER", "Delete all objects?"},
    {"ID_BK_CONFIRM_DELETE_ALL_OBJECTS", "Remove all of your placed objects?"},
    {"ID_BK_CONFIRM_DELETE_ALL_OK_BUTTON", "Delete all"},
    {"ID_BK_COPY_OBJECT", "Copy object"}, {"ID_BK_DELETE", "Delete"},
    {"ID_BK_DELETE_ALL_OBJECTS", "Delete all"}, {"ID_BK_DETAIL_MODE", "Detail mode"},
    {"ID_BK_DROP_OBJECT", "Drop object"}, {"ID_BK_EXIT", "Exit"},
    {"ID_BK_FOLLOW_CAMERA", "Follow camera"}, {"ID_BK_GRAB", "Grab"},
    {"ID_BK_HOLD_COPY", "Hold to copy"}, {"ID_BK_HOLD_RESET_ROTATION", "Hold to reset rotation"},
    {"ID_BK_INVERT_AXIS", "Invert axis"}, {"ID_BK_INVERT_ROTATE_AXIS", "Invert rotation axis"},
    {"ID_BK_INVERT_X_AXIS", "Invert X axis"}, {"ID_BK_INVERT_Y_AXIS", "Invert Y axis"},
    {"ID_BK_LOWER_RAISE", "Lower / Raise"}, {"ID_BK_NEW_OBJECT", "New object"},
    {"ID_BK_NOTHING", "None"}, {"ID_BK_OBJECTS", "Objects"},
    {"ID_BK_OBJECT_BROWSER", "Object browser"}, {"ID_BK_OFF", "Off"}, {"ID_BK_ON", "On"},
    {"ID_BK_OTHER", "Other"}, {"ID_BK_PLACE", "Place"},
    {"ID_BK_QD_SWITCH_MESSAGE", "Use Quick Drop to place objects while exploring."},
    {"ID_BK_QD_SWITCH_QUESTION", "Switch to Quick Drop?"},
    {"ID_BK_QUICK_DROP_ONLY", "Quick Drop only"}, {"ID_BK_RECENTER_CURSOR", "Recenter cursor"},
    {"ID_BK_RELATIVE", "Relative"}, {"ID_BK_REPLACE_ALWAYS", "Always replace"},
    {"ID_BK_REPLACE_OLDEST", "Replace oldest object"}, {"ID_BK_REPLACE_PROMPT", "Ask before replacing"},
    {"ID_BK_ROTATE", "Rotate"}, {"ID_BK_SELECT_OBJECT", "Select object"}, {"ID_BK_SNAP", "Snap"},
    {"ID_BK_SNAPPING_SENSITIVITY", "Snapping sensitivity"}, {"ID_BK_SNAP_DOWN", "Snap to ground"},
    {"ID_BK_STAMP", "Stamp"},
    {"ID_BK_SWITCH_MESSAGE", "Use Build Kit for precise object placement and editing."},
    {"ID_BK_SWITCH_QUESTION", "Switch to Build Kit?"},
    {"ID_BK_SWITCH_TO_BK", "Switch to Build Kit"}, {"ID_BK_SWITCH_TO_QUICKDROP", "Switch to Quick Drop"},
    {"ID_BK_WORLD", "World"}, {"ID_BK_ZOOM_IN_OUT", "Zoom in / out"},
    {"ID_SETTINGS_BK_ACTIVEAXIS", "Active axis"},
    {"ID_SETTINGS_BK_ANGLEROTATIONINTERVAL", "Rotation interval"},
    {"ID_SETTINGS_BK_ANGLEROTATIONINTERVAL_DESC", "Choose the angle used for each rotation step."},
    {"ID_SETTINGS_BK_DETAILMODEONOFF", "Detail mode"},
    {"ID_SETTINGS_BK_DETAILMODEONOFF_DESC", "Use finer controls for precise object placement."},
    {"ID_SETTINGS_BK_FOLLOWCAMERA", "Follow camera"},
    {"ID_SETTINGS_BK_FOLLOWCAMERA_DESC", "Move the placement cursor with the camera."},
    {"ID_SETTINGS_BK_INVERTAXIS", "Invert axis"},
    {"ID_SETTINGS_BK_INVERTAXIS_DESC", "Reverse the direction of the active control axis."},
    {"ID_SETTINGS_BK_QUICKDROPACTIVE", "Quick Drop"},
    {"ID_SETTINGS_BK_QUICKDROPACTIVE_DESC", "Use Quick Drop placement controls."},
    {"ID_SETTINGS_BK_ROTATERELATIVETOOBJECT", "Rotate relative to object"},
    {"ID_SETTINGS_BK_ROTATERELATIVETOOBJECT_DESC", "Use the object's local axes for rotation."},
    {"ID_SETTINGS_BK_ROTATIONDISCRETE", "Stepped rotation"},
    {"ID_SETTINGS_BK_ROTATIONDISCRETE_DESC", "Rotate in fixed increments."},
    {"ID_SETTINGS_BK_SNAPPINGLEVEL", "Snapping sensitivity"},
    {"ID_SETTINGS_BK_SNAPPINGLEVEL_DESC", "Adjust how strongly placement snaps to nearby objects."},
};

struct BuildKitTextFunctions {
    std::uintptr_t (*exists)(const char*){};
    std::uintptr_t (*translate)(const char*, char*, std::uint32_t){};
};

BuildKitTextFunctions& buildkit_text_functions();

std::string_view buildkit_fallback(const char* key);

std::uintptr_t buildkit_text_exists(const char* key);

std::uintptr_t buildkit_text_translate(const char* key, char* output, std::uint32_t capacity);
}
