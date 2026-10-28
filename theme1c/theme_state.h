#pragma once

/* Frozen Theme1C shared-memory layout.
 * Usable from freestanding DLL — plain C, no CRT types. */

/* Legacy named section — unused (crashed under late inject). IPC is state.bin. */
#define THEME1C_STATE_MAPPING_NAME L"Local\\Theme1C_State"
#define THEME1C_STATE_FILE_NAME L"state.bin"
#define THEME1C_STATE_MAGIC 0x31433454u
#define THEME1C_STATE_VERSION 1u

typedef struct Theme1C_Rgb {
  unsigned char r, g, b, a; /* a unused, 0 */
} Theme1C_Rgb;

typedef struct Theme1C_State {
  unsigned int magic;
  unsigned int version;
  unsigned int generation; /* seqlock: odd=write in progress */
  unsigned int enabled;    /* 0=pass-through hooks */
  /* syntax match (1C source fingerprints) */
  Theme1C_Rgb matchKeyword, matchString, matchComment, matchIdentifier, matchDirective;
  /* syntax output */
  Theme1C_Rgb outKeyword, outString, outComment, outIdentifier, outDirective;
  /* chrome */
  Theme1C_Rgb editorBg, sidebar, menu, text;
  /* extras used by MapSys / core palette */
  Theme1C_Rgb captionGray; /* near 150,150,150 retarget */
  Theme1C_Rgb legacyEditorBg; /* near 0,43,54 retarget uses editorBg; keep for future */
} Theme1C_State;
