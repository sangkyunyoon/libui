// 23 april 2015
#include "uipriv_unix.h"

static GArray *menus = NULL;
static gboolean menusFinalized = FALSE;

struct menu {
	uiMenu m;
	char *name;
	GArray *items;					// []struct menuItem
};

struct menuItem {
	uiMenuItem mi;
	char *name;
	int type;
	void (*onClicked)(uiMenuItem *, uiWindow *, void *);
	void *onClickedData;
	GtkWidget *baseItem;			// template for new instances; kept in sync with everything else
	GHashTable *uiWindows;			// map[GtkMenuItem]uiWindow
};

enum {
	typeRegular,
	typeCheckbox,
	typeQuit,
	typePreferences,
	typeAbout,
	typeSeparator,
};

#define NEWHASH() g_hash_table_new(g_direct_hash, g_direct_equal)

static void onClicked(GtkMenuItem *menuitem, gpointer data)
{
	struct menuItem *item = (struct menuItem *) data;
	uiWindow *w;

	w = uiWindow(g_hash_table_lookup(item->uiWindows, menuitem));
	(*(item->onClicked))(uiMenuItem(item), w, item->onClickedData);
}

static void defaultOnClicked(uiMenuItem *item, uiWindow *w, void *data)
{
	// do nothing
}

static void menuItemEnableDisable(struct menuItem *item, gboolean enabled)
{
	GHashTableIter iter;
	gpointer widget;

	gtk_widget_set_sensitive(item->baseItem, enabled);
	g_hash_table_iter_init(&iter, item->uiWindows);
	while (g_hash_table_iter_next(&iter, &widget, NULL))
		gtk_widget_set_sensitive(GTK_WIDGET(widget), enabled);
}

static void menuItemEnable(uiMenuItem *ii)
{
	struct menuItem *item = (struct menuItem *) ii;

	menuItemEnableDisable(item, TRUE);
}

static void menuItemDisable(uiMenuItem *ii)
{
	struct menuItem *item = (struct menuItem *) ii;

	menuItemEnableDisable(item, FALSE);
}

static void menuItemOnClicked(uiMenuItem *ii, void (*f)(uiMenuItem *, uiWindow *, void *), void *data)
{
	struct menuItem *item = (struct menuItem *) ii;

	item->onClicked = f;
	item->onClickedData = data;
}

static int menuItemChecked(uiMenuItem *ii)
{
	struct menuItem *item = (struct menuItem *) ii;

	return gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item->baseItem)) != FALSE;
}

static void menuItemSetChecked(uiMenuItem *ii, int checked)
{
	struct menuItem *item = (struct menuItem *) ii;
	gboolean c;

	// use explicit values
	c = FALSE;
	if (checked)
		c = TRUE;
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item->baseItem), c);
	// the bindings will make this take effect on all instances
}

static uiMenuItem *newItem(struct menu *m, int type, const char *name)
{
	struct menuItem *item;

	if (menusFinalized)
		complain("attempt to create a new menu item after menus have been finalized");

	g_array_set_size(m->items, m->items->len + 1);
	item = &g_array_index(m->items, struct menuItem, m->items->len - 1);

	item->type = type;
	switch (item->type) {
	case typeQuit:
		item->name = g_strdup("Quit");
		break;
	case typePreferences:
		item->name = g_strdup("Preferences...");
		break;
	case typeAbout:
		item->name = g_strdup("About");
		break;
	case typeSeparator:
		break;
	default:
		item->name = g_strdup(name);
		break;
	}

	item->onClicked = defaultOnClicked;

	switch (item->type) {
	case typeCheckbox:
		item->baseItem = gtk_check_menu_item_new_with_label(item->name);
		break;
	case typeSeparator:
		item->baseItem = gtk_separator_menu_item_new();
		break;
	default:
		item->baseItem = gtk_menu_item_new_with_label(item->name);
		break;
	}

	item->uiWindows = NEWHASH();

	uiMenuItem(item)->Enable = menuItemEnable;
	uiMenuItem(item)->Disable = menuItemDisable;
	uiMenuItem(item)->OnClicked = menuItemOnClicked;
	uiMenuItem(item)->Checked = menuItemChecked;
	uiMenuItem(item)->SetChecked = menuItemSetChecked;

	return uiMenuItem(item);
}

uiMenuItem *menuAppendItem(uiMenu *mm, const char *name)
{
	return newItem((struct menu *) mm, typeRegular, name);
}

uiMenuItem *menuAppendCheckItem(uiMenu *mm, const char *name)
{
	return newItem((struct menu *) mm, typeCheckbox, name);
}

uiMenuItem *menuAppendQuitItem(uiMenu *mm)
{
	// TODO check multiple quit items
	// TODO conditionally add separator
	newItem((struct menu *) mm, typeSeparator, NULL);
	return newItem((struct menu *) mm, typeQuit, NULL);
}

uiMenuItem *menuAppendPreferencesItem(uiMenu *mm)
{
	// TODO check multiple preferences items
	// TODO conditionally add separator
	newItem((struct menu *) mm, typeSeparator, NULL);
	return newItem((struct menu *) mm, typePreferences, NULL);
}

uiMenuItem *menuAppendAboutItem(uiMenu *mm)
{
	// TODO check multiple about items
	// TODO conditionally add separator
	newItem((struct menu *) mm, typeSeparator, NULL);
	return newItem((struct menu *) mm, typeAbout, NULL);
}

void menuAppendSeparator(uiMenu *mm)
{
	// TODO check multiple about items
	newItem((struct menu *) mm, typeSeparator, NULL);
}

uiMenu *uiNewMenu(const char *name)
{
	struct menu *m;

	if (menusFinalized)
		complain("attempt to create a new menu after menus have been finalized");
	if (menus == NULL)
		menus = g_array_new(FALSE, TRUE, sizeof (struct menu));

	// thanks Company in irc.gimp.net/#gtk+
	g_array_set_size(menus, menus->len + 1);
	m = &g_array_index(menus, struct menu, menus->len - 1);

	m->name = g_strdup(name);
	m->items = g_array_new(FALSE, TRUE, sizeof (struct menuItem));

	uiMenu(m)->AppendItem = menuAppendItem;
	uiMenu(m)->AppendCheckItem = menuAppendCheckItem;
	uiMenu(m)->AppendQuitItem = menuAppendQuitItem;
	uiMenu(m)->AppendPreferencesItem = menuAppendPreferencesItem;
	uiMenu(m)->AppendAboutItem = menuAppendAboutItem;
	uiMenu(m)->AppendSeparator = menuAppendSeparator;

	return uiMenu(m);
}

/*
void menuItemDestroy(struct menuItem *item)
{
	// TODO checck that item->uiWindows is empty
	g_hash_table_destroy(item->uiWindows);
	gtk_widget_destroy(item->baseItem);
	if (item->name != NULL)
		g_free(item->name);
}

void menuDestroy(void)
{
	guint i, j;
	struct menu *m;

	for (i = 0; i < menus->len; i++) {
		m = &g_array_index(menus, struct menu, i);
		for (j = 0; j < m->items->len; j++)
			menuItemDestroy(&g_array_index(m->items, struct menuItem, j));
		g_array_free(m->items, TRUE);
		g_free(m->name);
	}
	g_array_free(menus, TRUE);
}
*/

static void appendMenuItem(GtkMenuShell *submenu, struct menuItem *item, uiWindow *w)
{
	GtkWidget *menuitem;

	menuitem = g_object_new(G_OBJECT_TYPE(item->baseItem), NULL);
	if (item->name != NULL)
		gtk_menu_item_set_label(GTK_MENU_ITEM(menuitem), item->name);
	if (item->type != typeSeparator) {
		g_signal_connect(menuitem, "activate", G_CALLBACK(onClicked), item);

		// this binding does two things:
		// 1) makes it so that when one instance of the menu item is checked, the rest are too
		// 2) makes it so that the implementation of  uiMenuItemChecked() and uiMenuItemSetChecked() only needs to get/set the checked state from item->baseItem
		if (item->type == typeCheckbox)
			g_object_bind_property(item->baseItem, "active",
				menuitem, "active",
				G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
	}
	gtk_menu_shell_append(submenu, menuitem);
	g_hash_table_insert(item->uiWindows, menuitem, w);
}

// TODO should this return a zero-height widget (or NULL) if there are no menus defined?
GtkWidget *makeMenubar(uiWindow *w)
{
	GtkWidget *menubar;
	guint i, j;
	struct menu *m;
	GtkWidget *menuitem;
	GtkWidget *submenu;

	menusFinalized = TRUE;

	menubar = gtk_menu_bar_new();

	for (i = 0; i < menus->len; i++) {
		m = &g_array_index(menus, struct menu, i);
		menuitem = gtk_menu_item_new_with_label(m->name);
		submenu = gtk_menu_new();
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);
		for (j = 0; j < m->items->len; j++)
			appendMenuItem(GTK_MENU_SHELL(submenu), &g_array_index(m->items, struct menuItem, j), w);
		gtk_menu_shell_append(GTK_MENU_SHELL(menubar), menuitem);
	}

	gtk_widget_set_hexpand(menubar, TRUE);
	gtk_widget_set_halign(menubar, GTK_ALIGN_FILL);
	return menubar;
}
