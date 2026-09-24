
[VR]
; Restart Skyrim after changing these settings. Merge into your existing custom INI.
; Optional starting name: blank = leave the normal name/keyboard workflow unchanged.
; Applied once at character creation after New Game; it does NOT finish/close RaceMenu.
; The confirmed main-menu New Game choice is tracked through internal starting-save loads.
; Load/Continue cancels that choice; reopening showracemenu does not reapply the starting name.
; Existing saved-character names are protected. You can still edit the name in the menu.
; Use plain ASCII or UTF-8 (without BOM); 1..255 bytes, no newlines/control characters.
; Invalid/overlength names are ignored, never truncated. Example: sPlayerName=Dragonborn
sPlayerName=
; 1 = also apply sPlayerName each time showracemenu opens for an existing character.
; This explicitly renames saved characters; leave at 0 unless that is what you want.
bOverrideExistingPlayerName=0
; Native quill defaults depend on your active runtime, not the headset brand.
; SteamVR: 1 = quill (default); 0 = no quill, e.g. when using your own laser provider.
bUseQuillSteamVR=1
; OpenComposite/OCU: 0 = no quill (default, OCU supplies lasers); 1 = native quill.
; Enable this if OCU lasers are disabled and you want Skyrim's quill instead.
bUseQuillOCU=0
; The two options are independent. Only the active runtime's option is used.
; Does not change other menus, controller bindings, poses or pointer angle.
; Native cursor movement follows your VR controller bindings; Trigger/Accept clicks remain
; RaceMenu-local. Restart Skyrim after edits.
; Legacy bUseQuill (0/1) is honoured ONLY when the active runtime's new option
; is absent/blank/invalid. New options take precedence; remove the old key when migrating.
; An unrecognised runtime retains the legacy setting, or no quill if none is set.

[VR Lighting]
; Restart Skyrim after edits. All three temporary lights follow the edited avatar,
; NOT the headset, menu placement or face/normal view. Light On/Off toggles the rig.
; Enabled by default; 0 restores the original RaceMenu Papyrus light instead.
; Lights are non-shadow-casting, neutral white and restricted to the avatar's 3D.
; They are removed on menu close/revert, never saved or added to other menus.
bEnableFrontRig=1
; Coordinates are avatar-local Skyrim units, scaled with the avatar:
; Right: positive=avatar's right, negative=left (-300..300).
; Front: distance in front of the avatar (20..400).
; Key/Fill Height: vertical offset from the head node (-200..400), +=above.
; Body Height: height above the avatar's root/feet (-200..400), not from the head.
; Radius: influence range (50..800). Brightness: 0..3; 0 disables that source.
; Defaults provide a high face key, lower opposite-side fill and broad body fill.
; Missing/blank/invalid values use the defaults below; lower brightness if washed out.
fKeyRight=-35
fKeyFront=85
fKeyHeight=15
fKeyRadius=220
fKeyBrightness=1.0
fFillRight=35
fFillFront=80
fFillHeight=-35
fFillRadius=190
fFillBrightness=0.45
fBodyRight=0
fBodyFront=125
fBodyHeight=90
fBodyRadius=300
fBodyBrightness=0.8

[Menu Profile Flat]
; Original framing / engine camera behavior.
bConsolidatePanel=0


[Menu Profile VR Normal]
; 1 = move controls into left panel; 0 = restore original framing.
bConsolidatePanel=1
; Restart Skyrim after INI changes. Personal overrides: SKSE/Plugins/skee64_custom.ini.

; --- 2D artwork overrides (movie coords, NOT PNG pixels; separate from fSurfaceScale below) ---
; fPanelX/Y: absolute origin (not offset), range -2048..2048. Empty = stock origin.
; On the 1024x1024 logical canvas, X increases right and Y increases down.
; fPanelScale: 25-300%, 100 = stock size.
; fColorPickerScale: 50-400% of picker art; empty = current 225% default.
fPanelScale=
fPanelX=
fPanelY=
fColorPickerScale=

; --- Polar world placement: main menu ---
; Reference heading = viewer-to-avatar direction when RaceMenu opens, recaptured on each
; Normal/Face switch. Menu stays fixed in world space between switches (not head-locked).
; Surface faces the captured viewer; wand interaction moves with it.
; bForceVertical: 1 = upright (default), no tilt from elevation/height; 0 = tilt toward viewer.
; Applies to main menu and color picker in both views. Horizontal facing is always retained.
; Opt into 0 for extreme elevations (e.g. +/-60 deg), where tilt makes the menu easier to see.
bForceVertical=1
; fAzimuth: -85..85 deg. 0=ahead, +=left, -=right.
fAzimuth=30
; fElevation: -60..60 deg. 0=captured eye height, +=up, -=down (angular, not world units).
fElevation=0
; fHeightOffset: -150..150 world units. Added after elevation; +=up, -=down.
; At elevation 0, default 0 aligns menu center to the player's captured eye line.
; Adjustments are relative to that baseline, not absolute height above ground.
fHeightOffset=0
; fDistance: 30..300 world units. Radius from viewer to menu center (also affects offset at angles).
fDistance=100
; fSurfaceScale: 25..300%. Uniform VR surface size at same center/distance; PNG proportions kept.
fSurfaceScale=100

; --- Sculpt workspace ---
; Wider workspace: controls/history/head parts left, sculpt canvas right beside avatar.
; Independent horizontal angle, same reference heading/range/sign as fAzimuth.
; Empty or omitted = 48 degrees LEFT. Other placement settings stay shared with main menu.
fSculptAzimuth=48

; --- Polar world placement: color picker (same units/conventions, independent of main menu) ---
fColorPickerAzimuth=38
fColorPickerElevation=0
; At elevation 0, default 0 aligns picker center to captured player eye line; independent height.
fColorPickerHeightOffset=0
fColorPickerDistance=100
fColorPickerSurfaceScale=100


[Menu Profile VR Face]
; Shares world placement with VR Normal (placement keys live there only).
; Reanchors once on view switch, using the same relative placement; not head-locked.
bConsolidatePanel=1
; fFaceDistance: 25..150 world units. Viewing separation from avatar head; smaller = closer.
fFaceDistance=45
; fFaceEyeHeight: -20..30 world units. Vertical offset above head bone; +up/-down (not angular).
fFaceEyeHeight=5
; Optional face-view artwork overrides (same meaning as VR Normal above). Empty = shared/default.
fPanelScale=
fPanelX=
fPanelY=
fColorPickerScale=


; --- Reference examples (inactive) ---
; [Menu Category 4]
; sLabel=Body
; bVisible=1
; iOrder=-950
;
; [Menu Provider example.mod Section appearance]
; sLabel=Appearance
; bVisible=1
; iOrder=-925
; sControlIds=height,width
