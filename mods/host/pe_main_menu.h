#ifndef PE_MAIN_MENU_H
#define PE_MAIN_MENU_H

/* Aya's main menu (the column of icons) and screens built from the game's menu widgets.
 *
 * Extra rows: mods add rows after the game's last one. The host gives the list its rows
 * (PE_HOOK_LIST_ROWS), their icons (PE_HOOK_MAIN_MENU_ICON), their title-bar names
 * (PE_HOOK_HELP_TEXT) and opens the one confirmed (PE_HOOK_MAIN_MENU_ACTION); rows are shown in
 * `order`, lowest first, outside battle with a game loaded.
 *
 * Screens: a mod's screen is a panel node with its own input handler and a list node with its own
 * row drawer, the way the game's screens are. The helpers below build one in the options menu's
 * layout, hide Aya's status panels behind it as the game's own screens do, and keep scrolling
 * inside the list. */

#include "pe_hooks.h"

struct MenuWidgetNode;

typedef struct PeMainMenuRow {
    const char *title;    /* in the title bar while the row is highlighted */
    int icon_item;        /* item whose inventory icon the row shows; 0 = the game's spare icon */
    int (*shown)(void);   /* NULL = always */
    void (*open)(struct MenuWidgetNode *main_list);   /* confirmed */
    /* Optional, instead of icon_item: draw extra sprites at the icon's spot (the text cursor) and/or
     * set icon->sprite, which the game then draws there. */
    void (*icon)(PeHookMainMenuIcon *icon);
} PeMainMenuRow;

/* The main menu's own icons: action + PE_MAIN_MENU_SPRITE (0 Use Item .. 3 Equip Armor .. 8). */
#define PE_MAIN_MENU_SPRITE 0x56
#define PE_ACTION_EQUIP_ARMOR 3

void pe_main_menu_add(const PeMainMenuRow *row, int order);   /* row must outlive the game */

/* A screen: panel at (x, y), `width` wide, list of `rows_visible` rows (longer lists scroll).
 * The list's draw and the panel's update are the caller's; the list is made current. */
typedef struct PeScreen {
    struct MenuWidgetNode *panel, *list, *from;
} PeScreen;

void pe_screen_open(PeScreen *s, struct MenuWidgetNode *from, int x, int y, int width, int rows_visible,
                    void (*draw)(struct MenuWidgetNode *), int (*input)(struct MenuWidgetNode *, unsigned int));
/* The same, flush where the game's own screens sit (the inventory panel's layout, descriptor 1). */
void pe_screen_open_game(PeScreen *s, struct MenuWidgetNode *from, int rows_visible,
                         void (*draw)(struct MenuWidgetNode *), int (*input)(struct MenuWidgetNode *, unsigned int));
void pe_screen_close(PeScreen *s);   /* back to `from` */
void pe_screen_rows(PeScreen *s, int rows, int cursor);   /* the list's row count and cursor */
int pe_screen_title_base(void);     /* a selected_base whose title bar always prints */

void pe_main_menu_init(void);       /* the host, at start-up */

#define PE_MENU_CROSS  0x10000u
#define PE_MENU_CIRCLE 0x40u

#endif
