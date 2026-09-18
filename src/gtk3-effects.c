#include "gtk3-effects.h"

#include "camorama-filter-chain.h"
#include "filter.h"

#include <glib/gi18n.h>

#define EFFECT_REMOVE_DELAY 100
#define EFFECT_TRANSITION_DURATION 400

typedef struct {
    GType type;
    gchar *name;
} EffectInfo;

typedef struct {
    cam_t *cam;
    GtkListBox *list;
    GPtrArray *effects;
} EffectsPane;

typedef struct {
    EffectsPane *pane;
    GtkWidget *row;
    GtkRevealer *revealer;
    GtkComboBoxText *choice;
    GArray *choices;
    GType selected_type;
    gulong changed_handler;
    gboolean removing;
} EffectRow;

static void append_effect_row(EffectsPane *pane, gboolean animate);

static void effect_info_free(EffectInfo *effect)
{
    g_free(effect->name);
    g_free(effect);
}

static void effects_pane_free(EffectsPane *pane)
{
    g_ptr_array_unref(pane->effects);
    g_free(pane);
}

static void effect_row_free(EffectRow *effect_row)
{
    g_array_unref(effect_row->choices);
    g_free(effect_row);
}

static void append_choice(EffectRow *effect_row, GType type,
                          const gchar *name)
{
    gtk_combo_box_text_append_text(effect_row->choice, name);
    g_array_append_val(effect_row->choices, type);
}

static void show_available_effects(EffectRow *effect_row)
{
    guint i;
    GType no_effect = G_TYPE_INVALID;

    append_choice(effect_row, no_effect, _("<No effect>"));
    for (i = 0; i < effect_row->pane->effects->len; i++) {
        EffectInfo *effect = g_ptr_array_index(effect_row->pane->effects, i);

        append_choice(effect_row, effect->type, effect->name);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(effect_row->choice), 0);
}

static void show_active_effect(EffectRow *effect_row)
{
    guint i;
    GType no_effect = G_TYPE_INVALID;

    g_signal_handler_block(effect_row->choice,
                           effect_row->changed_handler);
    gtk_combo_box_text_remove_all(effect_row->choice);
    g_array_set_size(effect_row->choices, 0);

    for (i = 0; i < effect_row->pane->effects->len; i++) {
        EffectInfo *effect = g_ptr_array_index(effect_row->pane->effects, i);

        if (effect->type == effect_row->selected_type) {
            append_choice(effect_row, effect->type, effect->name);
            break;
        }
    }

    append_choice(effect_row, no_effect, _("<Disable>"));
    for (i = 0; i < effect_row->pane->effects->len; i++) {
        EffectInfo *effect = g_ptr_array_index(effect_row->pane->effects, i);

        if (effect->type != effect_row->selected_type)
            append_choice(effect_row, effect->type, effect->name);
    }

    gtk_combo_box_set_active(GTK_COMBO_BOX(effect_row->choice), 0);
    g_signal_handler_unblock(effect_row->choice,
                             effect_row->changed_handler);
}

static void remove_effect_row(GtkRevealer *revealer, GParamSpec *,
                              EffectRow *effect_row)
{
    if (!effect_row->removing ||
        gtk_revealer_get_child_revealed(revealer))
        return;

    gtk_container_remove(GTK_CONTAINER(effect_row->pane->list),
                         effect_row->row);
}

static gboolean reveal_effect_row(gpointer data)
{
    gtk_revealer_set_reveal_child(GTK_REVEALER(data), TRUE);
    return G_SOURCE_REMOVE;
}

static gboolean conceal_effect_row(gpointer data)
{
    gtk_revealer_set_reveal_child(GTK_REVEALER(data), FALSE);
    return G_SOURCE_REMOVE;
}

static void effect_changed(GtkComboBox *choice, EffectRow *effect_row)
{
    gint selected = gtk_combo_box_get_active(choice);
    guint position;
    GType selected_type;

    if (selected < 0 || selected >= (gint)effect_row->choices->len)
        return;

    selected_type = g_array_index(effect_row->choices, GType, selected);
    if (selected_type == effect_row->selected_type)
        return;

    position = gtk_list_box_row_get_index(
        GTK_LIST_BOX_ROW(effect_row->row));

    if (selected_type == G_TYPE_INVALID) {
        camorama_filter_chain_remove(effect_row->pane->cam->filter_chain,
                                     position);
        effect_row->selected_type = G_TYPE_INVALID;
        effect_row->removing = TRUE;
        gtk_widget_set_sensitive(GTK_WIDGET(effect_row->choice), FALSE);
        g_timeout_add_full(G_PRIORITY_DEFAULT, EFFECT_REMOVE_DELAY,
                           conceal_effect_row,
                           g_object_ref(effect_row->revealer),
                           g_object_unref);
        return;
    }

    if (effect_row->selected_type == G_TYPE_INVALID) {
        camorama_filter_chain_insert(effect_row->pane->cam->filter_chain,
                                     position, selected_type);
        effect_row->selected_type = selected_type;
        show_active_effect(effect_row);
        append_effect_row(effect_row->pane, TRUE);
        return;
    }

    camorama_filter_chain_replace(effect_row->pane->cam->filter_chain,
                                  position, selected_type);
    effect_row->selected_type = selected_type;
    show_active_effect(effect_row);
}

static void append_effect_row(EffectsPane *pane, gboolean animate)
{
    EffectRow *effect_row = g_new0(EffectRow, 1);
    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    effect_row->pane = pane;
    effect_row->row = gtk_list_box_row_new();
    effect_row->revealer = GTK_REVEALER(gtk_revealer_new());
    effect_row->choice = GTK_COMBO_BOX_TEXT(gtk_combo_box_text_new());
    effect_row->choices = g_array_new(FALSE, FALSE, sizeof(GType));

    gtk_widget_set_margin_top(content, 6);
    gtk_widget_set_margin_bottom(content, 6);
    gtk_widget_set_margin_start(content, 6);
    gtk_widget_set_margin_end(content, 6);
    gtk_widget_set_hexpand(GTK_WIDGET(effect_row->choice), TRUE);

    show_available_effects(effect_row);
    effect_row->changed_handler =
        g_signal_connect(effect_row->choice, "changed",
                         G_CALLBACK(effect_changed), effect_row);

    gtk_revealer_set_transition_type(
        effect_row->revealer, GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
    gtk_revealer_set_transition_duration(effect_row->revealer,
                                         EFFECT_TRANSITION_DURATION);
    gtk_revealer_set_reveal_child(effect_row->revealer, !animate);

    gtk_container_add(GTK_CONTAINER(content),
                      GTK_WIDGET(effect_row->choice));
    gtk_container_add(GTK_CONTAINER(effect_row->revealer), content);
    gtk_container_add(GTK_CONTAINER(effect_row->row),
                      GTK_WIDGET(effect_row->revealer));
    gtk_list_box_insert(pane->list, effect_row->row, -1);
    gtk_widget_show_all(effect_row->row);

    g_object_set_data_full(G_OBJECT(effect_row->row), "effect-row",
                           effect_row, (GDestroyNotify)effect_row_free);
    g_signal_connect(effect_row->revealer, "notify::child-revealed",
                     G_CALLBACK(remove_effect_row), effect_row);

    if (animate)
        g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, reveal_effect_row,
                        g_object_ref(effect_row->revealer), g_object_unref);
}

void gtk3_effects_setup(cam_t *cam)
{
    EffectsPane *pane = g_new0(EffectsPane, 1);
    GType *filter_types;
    guint n_filters;
    guint i;

    pane->cam = cam;
    pane->list = GTK_LIST_BOX(gtk_builder_get_object(cam->xml,
                                                      "effects_list"));
    pane->effects = g_ptr_array_new_with_free_func(
        (GDestroyNotify)effect_info_free);

    filter_types = g_type_children(CAMORAMA_TYPE_FILTER, &n_filters);
    for (i = 0; i < n_filters; i++) {
        CamoramaFilterClass *filter_class = g_type_class_ref(filter_types[i]);
        EffectInfo *effect = g_new0(EffectInfo, 1);

        effect->type = filter_types[i];
        effect->name = g_strdup(filter_class->name ? filter_class->name :
                                g_type_name(filter_types[i]));
        g_ptr_array_add(pane->effects, effect);
        g_type_class_unref(filter_class);
    }
    g_free(filter_types);

    cam->filter_chain = camorama_filter_chain_new();
    camorama_filter_chain_set_data(cam->filter_chain, cam);
    g_object_set_data_full(G_OBJECT(pane->list), "effects-pane", pane,
                           (GDestroyNotify)effects_pane_free);
    g_object_set_data_full(G_OBJECT(pane->list), "filter-chain",
                           cam->filter_chain, g_object_unref);

    append_effect_row(pane, FALSE);
}
