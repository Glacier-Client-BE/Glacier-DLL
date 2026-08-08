#pragma once

// Win32 resource IDs. Kept in its own header because Glacier.rc is compiled by
// the resource compiler, which understands #define and little else.

// Font Awesome 6 Free (Solid), embedded so the menu's icons are the same on
// every machine. Windows' own symbol fonts differ between 10 and 11 — both in
// name and in coverage — and this client is tested across both. SIL OFL 1.1;
// see assets/fonts/LICENSE-fontawesome.txt and docs/acknowledgements.md.
#define IDR_FONT_ICONS 101
