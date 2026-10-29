/* Aya's main menu: extra rows, and screens from the game's widgets. See pe_main_menu.h. */
#include <string.h>

#include "pe_game.h"
#include "pe_gametext.h"
#include "pe_main_menu.h"
#include "pe_mod_host.h"
#include "pe1/inventory_slots.h"
#include "pe1/menu_scroll_cursor.h"
#include "pe1/menu_state.h"
#include "pe1/menu_widget.h"

void Menu_DrawItemList(MenuWidgetNode *node);

#define PE_HOST_ID      "host"
#define PE_ROWS_MAX     8
#define PE_DESC_PANEL   0x20   /* the options menu's panel and list layout */
#define PE_TITLE_BASE   17     /* a selected_base whose title-bar case always prints */

/* ---- extra rows ---- */

static struct { const PeMainMenuRow *row; int order; const char *mod_id; } s_rows[PE_ROWS_MAX];
static int s_row_count;
static u8 s_row_title[64];

void pe_main_menu_add(const PeMainMenuRow *row, int order) {
    int i;
    if (!row || s_row_count == PE_ROWS_MAX) return;
    for (i = s_row_count; i > 0 && s_rows[i - 1].order > order; --i) s_rows[i] = s_rows[i - 1];
    s_rows[i].row = row;
    s_rows[i].order = order;
    s_rows[i].mod_id = pe_mod_current();          /* an inactive mod's row isn't shown */
    ++s_row_count;
}

static int pe_menu_open_here(void) { return pe_game_loaded() && !pe_in_battle() && !Menu_GetBattleEquipMode(); }

static int pe_shown(int i) { return pe_mod_active(s_rows[i].mod_id) && (!s_rows[i].row->shown || s_rows[i].row->shown()); }

/* The game's rows: one per set action bit. */
static int pe_game_rows(void) {
    int bits = g_MenuItemContextFlag & 0x1EF, n = 0;
    while (bits) { n += bits & 1; bits >>= 1; }
    return n;
}

/* The extra row at list row `row`, or NULL. */
static const PeMainMenuRow *pe_row_at(int row) {
    int k = row - pe_game_rows(), i;
    if (k < 0) return NULL;
    for (i = 0; i < s_row_count; ++i)
        if (pe_shown(i) && k-- == 0) return s_rows[i].row;
    return NULL;
}

static int pe_is_main_list(const MenuWidgetNode *node) { return node && node->draw == (void (*)())Menu_DrawItemList; }

static int pe_on_list_rows(void *ctx, void *user) {
    const PeHookListRows *r = (const PeHookListRows *)ctx;
    int i, added = 0;
    (void)user;
    if (!pe_is_main_list(r->node) || !pe_menu_open_here()) return 0;
    for (i = 0; i < s_row_count; ++i) added += pe_shown(i);
    if (!added) return 0;
    *r->count += added;
    /* The main list shows at most visible_rows_mirror rows (8, its layout's cap): with every game
     * row present ours would sit past it. Raise the cap; the game grows the panel. */
    if (r->node->visible_rows_mirror < *r->count) r->node->visible_rows_mirror = *r->count;
    return 0;
}

static int pe_on_icon(void *ctx, void *user) {
    PeHookMainMenuIcon *icon = (PeHookMainMenuIcon *)ctx;
    const PeMainMenuRow *row;
    (void)user;
    if (icon->action != 9 || !pe_menu_open_here() || !(row = pe_row_at(icon->row))) return 0;
    if (row->icon) { row->icon(icon); return 0; }
    if (row->icon_item) {
        const ItemDataRecord *rec = Inv_LookupItemRecordScaled(row->icon_item);
        if (rec && rec->iconId) icon->sprite = rec->iconId - 1;
    }
    return 0;
}

static int pe_on_row_title(void *ctx, void *user) {
    u8 **text = (u8 **)ctx;
    MenuWidgetNode *current = MenuWidget_GetCurrentNode();
    const PeMainMenuRow *row;
    (void)user;
    if (!pe_is_main_list(current) || !pe_menu_open_here()) return 0;
    if ((row = pe_row_at(MenuWidget_GridCellIndex(current)))) *text = pe_text_from_ascii(s_row_title, sizeof s_row_title, row->title);
    return 0;
}

static int pe_on_main_action(void *ctx, void *user) {
    const PeHookMenuAction *a = (const PeHookMenuAction *)ctx;
    const PeMainMenuRow *row;
    (void)user;
    if (a->action != 9 || !pe_menu_open_here() || !(row = pe_row_at(MenuWidget_GridCellIndex(a->list)))) return 0;
    pe_mod_log(PE_HOST_ID, "main menu: %s", row->title);
    row->open(a->list);
    return 1;                                              /* handled: the game plays the confirm sound */
}

/* ---- screens ----
 * Like the game's own screens, a screen hides Aya's status panels (mode-1 panels 0x12, 0x18, 0x2D,
 * hidden while their cursor_y is set: MenuWidget_UpdateAndDraw) and the HP overlay, and puts them
 * back when it closes. */
extern int g_MenuBattleStatusOverlayActive;
enum { PE_HIDDEN_MAX = 4 };
static struct { MenuWidgetNode *node[PE_HIDDEN_MAX]; int selected[PE_HIDDEN_MAX]; int overlay; } s_hidden;

static void pe_hide_panels(void) {
    MenuWidgetNode *node = g_MenuWidgetActiveListHead;
    int n = 0, guard;
    memset(&s_hidden, 0, sizeof s_hidden);
    for (guard = 0; node && guard < 64 && n < PE_HIDDEN_MAX; ++guard, node = node->next) {
        const int sel = node->selected_base;
        if (node->mode == 1 && (sel == 0x12 || sel == 0x18 || sel == 0x2D) && node->cursor_y == 0) {
            node->cursor_y = 1;
            s_hidden.node[n] = node;
            s_hidden.selected[n++] = sel;
        }
    }
    s_hidden.overlay = g_MenuBattleStatusOverlayActive;
    g_MenuBattleStatusOverlayActive = 0;
}

static void pe_show_panels(void) {
    int i;
    for (i = 0; i < PE_HIDDEN_MAX; ++i) {
        MenuWidgetNode *node = s_hidden.node[i];
        if (node && node->mode == 1 && node->selected_base == s_hidden.selected[i]) node->cursor_y = 0;
    }
    g_MenuBattleStatusOverlayActive = s_hidden.overlay;
    memset(&s_hidden, 0, sizeof s_hidden);
}

/* The stock grid handler advances the viewport by one row, even when its Options layout wraps
 * the cursor to the opposite end. Keep those jumps (and input arriving during a scroll animation)
 * inside the list's viewport. */
static int pe_scroll_input(MenuWidgetNode *list, unsigned int buttons) {
    int handled = Menu_StepScrollCursor(list, buttons);
    int top = list->scroll_y;
    if (list->cursor_y < top) top = list->cursor_y;
    else if (list->cursor_y >= top + list->visible_rows) top = list->cursor_y - list->visible_rows + 1;
    if (top != list->scroll_y) {
        list->scroll_y = top;
        list->scroll_adjust = 0;
    }
    return handled;
}

void pe_screen_open(PeScreen *s, MenuWidgetNode *from, int x, int y, int width, int rows_visible,
                    void (*draw)(MenuWidgetNode *), int (*input)(MenuWidgetNode *, unsigned int)) {
    pe_hide_panels();
    s->from = from;
    s->panel = MenuWidget_CreateSimpleNode(PE_DESC_PANEL, from, 0, 0);
    s->list = MenuWidget_CreateNode(PE_DESC_PANEL, s->panel, s->panel);
    s->panel->update = input;
    s->panel->x = x;
    s->panel->y = y;
    s->panel->grid_width = width;
    /* The list clips to cell width, independently of the panel's background. Replace the borrowed
     * Options layout's 68-pixel cell with the panel's inner width (the list's x offset supplies
     * the margin on either side). */
    s->list->draw_state = width - 2 * s->list->x;
    s->list->draw = (void (*)())draw;
    s->list->update = pe_scroll_input;
    s->list->selected_base = PE_TITLE_BASE;
    s->list->visible_rows_mirror = rows_visible;
    MenuWidget_SetCurrentNode(s->list);
}

#define PE_DESC_GAME_SCREEN 1   /* Aya's inventory panel (Menu_CreateEquipScreen) */

void pe_screen_open_game(PeScreen *s, MenuWidgetNode *from, int rows_visible,
                         void (*draw)(MenuWidgetNode *), int (*input)(MenuWidgetNode *, unsigned int)) {
    const MenuWidgetSimpleDescriptor *d = MenuWidget_LookupSimpleDescriptor(PE_DESC_GAME_SCREEN);
    const int x = d && d->x > 0 ? d->x : 28, y = d && d->y > 0 ? d->y : 24, width = d && d->width > 0 ? d->width : 252;
    pe_screen_open(s, from, x, y, width, rows_visible, draw, input);
}

void pe_screen_close(PeScreen *s) {
    if (s->panel) MenuWidget_DestroyNode(s->panel);
    s->panel = s->list = NULL;
    pe_show_panels();
    if (s->from) MenuWidget_SetCurrentNode(s->from);
}

void pe_screen_rows(PeScreen *s, int rows, int cursor) {
    const int visible = s->list->visible_rows_mirror;
    s->list->scroll_adjust = 0;
    s->list->cursor_x = 0;
    s->list->cursor_y = cursor >= 0 && cursor < rows ? cursor : 0;
    s->list->scroll_y = s->list->cursor_y >= visible ? s->list->cursor_y - visible + 1 : 0;
    Draw_SetPrimCallback(s->list, rows);
}

int pe_screen_title_base(void) { return PE_TITLE_BASE; }

void pe_main_menu_init(void) {
    pe_hook_add(PE_HOOK_LIST_ROWS, pe_on_list_rows, NULL, PE_PRIORITY_NORMAL, PE_HOST_ID);
    pe_hook_add(PE_HOOK_MAIN_MENU_ICON, pe_on_icon, NULL, PE_PRIORITY_NORMAL, PE_HOST_ID);
    pe_hook_add(PE_HOOK_HELP_TEXT, pe_on_row_title, NULL, PE_PRIORITY_LATE, PE_HOST_ID);
    pe_hook_add(PE_HOOK_MAIN_MENU_ACTION, pe_on_main_action, NULL, PE_PRIORITY_NORMAL, PE_HOST_ID);
}
