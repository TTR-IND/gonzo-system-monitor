/* Procman - main window
 * Copyright (C) 2001 Kevin Vandersloot
 * Copyright (C) 2012-2021 MATE Developers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */


#include <config.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <gdk/gdkkeysyms.h>
#include <math.h>

#include "procman.h"
#include "callbacks.h"
#include "interface.h"
#include "proctable.h"
#include "procactions.h"
#include "load-graph.h"
#include "util.h"
#include "disks.h"
#include "sysinfo.h"
#include "gsm_color_button.h"

static void    cb_toggle_tree (GtkAction *action, gpointer data);
static void    cb_proc_goto_tab (gint tab);
static gboolean cb_mem_two_color_picker_clicked (GtkWidget *widget, GdkEventButton *event, gpointer data);
static gboolean cb_zram_two_color_picker_clicked (GtkWidget *widget, GdkEventButton *event, gpointer data);

static const GtkActionEntry menu_entries[] =
{
    // xgettext: noun, top level menu.
    // "File" did not make sense for system-monitor
    { "Monitor", NULL, N_("_Monitor") },
    { "Edit", NULL, N_("_Edit") },
    { "View", NULL, N_("_View") },
    { "Help", NULL, N_("_Help") },

    { "Lsof", "edit-find", N_("Search for _Open Files"), "<control>O",
      N_("Search for open files"), G_CALLBACK(cb_show_lsof) },
    { "Quit", "application-exit", N_("_Quit"), "<control>Q",
      N_("Quit the program"), G_CALLBACK (cb_app_exit) },


    { "StopProcess", NULL, N_("_Stop Process"), "<control>S",
      N_("Stop process"), G_CALLBACK(cb_kill_sigstop) },
    { "ContProcess", NULL, N_("_Continue Process"), "<control>C",
      N_("Continue process if stopped"), G_CALLBACK(cb_kill_sigcont) },

    { "EndProcess", NULL, N_("_End Process"), "<control>E",
      N_("Force process to finish normally"), G_CALLBACK (cb_end_process) },
    { "KillProcess", NULL, N_("_Kill Process"), "<control>K",
      N_("Force process to finish immediately"), G_CALLBACK (cb_kill_process) },
    { "ChangePriority", NULL, N_("_Change Priority"), NULL,
      N_("Change the order of priority of process"), NULL },
    { "Preferences", "preferences-desktop", N_("_Preferences"), NULL,
      N_("Configure the application"), G_CALLBACK (cb_edit_preferences) },

    { "Refresh", "view-refresh", N_("_Refresh"), "<control>R",
      N_("Refresh the process list"), G_CALLBACK(cb_user_refresh) },

    { "MemoryMaps", NULL, N_("_Memory Maps"), "<control>M",
      N_("Open the memory maps associated with a process"), G_CALLBACK (cb_show_memory_maps) },
    // Translators: this means 'Files that are open' (open is no verb here
    { "OpenFiles", NULL, N_("Open _Files"), "<control>F",
      N_("View the files opened by a process"), G_CALLBACK (cb_show_open_files) },
    { "ProcessProperties", NULL, N_("_Properties"), NULL,
      N_("View additional information about a process"), G_CALLBACK (cb_show_process_properties) },


    { "HelpContents", "help-browser", N_("_Contents"), "F1",
      N_("Open the manual"), G_CALLBACK (cb_help_contents) },
    { "About", "help-about", N_("_About"), NULL,
      N_("About this application"), G_CALLBACK (cb_about) }
};

static const GtkToggleActionEntry toggle_menu_entries[] =
{
    { "ShowDependencies", NULL, N_("_Dependencies"), "<control>D",
      N_("Show parent/child relationship between processes"),
      G_CALLBACK (cb_toggle_tree), TRUE },
};


static const GtkRadioActionEntry radio_menu_entries[] =
{
  { "ShowActiveProcesses", NULL, N_("_Active Processes"), NULL,
    N_("Show active processes"), ACTIVE_PROCESSES },
  { "ShowAllProcesses", NULL, N_("A_ll Processes"), NULL,
    N_("Show all processes"), ALL_PROCESSES },
  { "ShowMyProcesses", NULL, N_("M_y Processes"), NULL,
    N_("Show only user-owned processes"), MY_PROCESSES }
};

static const GtkRadioActionEntry priority_menu_entries[] =
{
    { "VeryHigh", NULL, N_("Very High"), NULL,
      N_("Set process priority to very high"), VERY_HIGH_PRIORITY },
    { "High", NULL, N_("High"), NULL,
      N_("Set process priority to high"), HIGH_PRIORITY },
    { "Normal", NULL, N_("Normal"), NULL,
      N_("Set process priority to normal"), NORMAL_PRIORITY },
    { "Low", NULL, N_("Low"), NULL,
      N_("Set process priority to low"), LOW_PRIORITY },
    { "VeryLow", NULL, N_("Very Low"), NULL,
      N_("Set process priority to very low"), VERY_LOW_PRIORITY },
    { "Custom", NULL, N_("Custom"), NULL,
      N_("Set process priority manually"), CUSTOM_PRIORITY }
};


static const char ui_info[] =
    "  <menubar name=\"MenuBar\">"
    "    <menu name=\"MonitorMenu\" action=\"Monitor\">"
    "      <menuitem name=\"MonitorLsofMenu\" action=\"Lsof\" />"
    "      <menuitem name=\"MonitorQuitMenu\" action=\"Quit\" />"
    "    </menu>"
    "    <menu name=\"EditMenu\" action=\"Edit\">"
    "      <menuitem name=\"EditStopProcessMenu\" action=\"StopProcess\" />"
    "      <menuitem name=\"EditContProcessMenu\" action=\"ContProcess\" />"
    "      <separator />"
    "      <menuitem name=\"EditEndProcessMenu\" action=\"EndProcess\" />"
    "      <menuitem name=\"EditKillProcessMenu\" action=\"KillProcess\" />"
    "      <separator />"
    "      <menu name=\"EditChangePriorityMenu\" action=\"ChangePriority\" >"
    "        <menuitem action=\"VeryHigh\" />"
    "        <menuitem action=\"High\" />"
    "        <menuitem action=\"Normal\" />"
    "        <menuitem action=\"Low\" />"
    "        <menuitem action=\"VeryLow\" />"
    "        <separator />"
    "        <menuitem action=\"Custom\"/>"
    "      </menu>"
    "      <separator />"
    "      <menuitem name=\"EditPreferencesMenu\" action=\"Preferences\" />"
    "    </menu>"
    "    <menu name=\"ViewMenu\" action=\"View\">"
    "      <menuitem name=\"ViewActiveProcesses\" action=\"ShowActiveProcesses\" />"
    "      <menuitem name=\"ViewAllProcesses\" action=\"ShowAllProcesses\" />"
    "      <menuitem name=\"ViewMyProcesses\" action=\"ShowMyProcesses\" />"
    "      <separator />"
    "      <menuitem name=\"ViewDependenciesMenu\" action=\"ShowDependencies\" />"
    "      <separator />"
    "      <menuitem name=\"ViewMemoryMapsMenu\" action=\"MemoryMaps\" />"
    "      <menuitem name=\"ViewOpenFilesMenu\" action=\"OpenFiles\" />"
    "      <separator />"
    "      <menuitem name=\"ViewProcessPropertiesMenu\" action=\"ProcessProperties\" />"
    "      <separator />"
    "      <menuitem name=\"ViewRefresh\" action=\"Refresh\" />"
    "    </menu>"
    "    <menu name=\"HelpMenu\" action=\"Help\">"
    "      <menuitem name=\"HelpContentsMenu\" action=\"HelpContents\" />"
    "      <menuitem name=\"HelpAboutMenu\" action=\"About\" />"
    "    </menu>"
    "  </menubar>"
    "  <popup name=\"PopupMenu\" action=\"Popup\">"
    "    <menuitem action=\"StopProcess\" />"
    "    <menuitem action=\"ContProcess\" />"
    "    <separator />"
    "    <menuitem action=\"EndProcess\" />"
    "    <menuitem action=\"KillProcess\" />"
    "    <separator />"
    "    <menu name=\"ChangePriorityMenu\" action=\"ChangePriority\" >"
    "      <menuitem action=\"VeryHigh\" />"
    "      <menuitem action=\"High\" />"
    "      <menuitem action=\"Normal\" />"
    "      <menuitem action=\"Low\" />"
    "      <menuitem action=\"VeryLow\" />"
    "      <separator />"
    "      <menuitem action=\"Custom\"/>"
    "    </menu>"
    "    <separator />"
    "    <menuitem action=\"MemoryMaps\" />"
    "    <menuitem action=\"OpenFiles\" />"
    "    <separator />"
    "    <menuitem action=\"ProcessProperties\" />"

    "  </popup>";


static void
create_proc_view(ProcData *procdata, GtkBuilder * builder)
{
    GtkWidget *proctree;
    GtkWidget *scrolled;
    char* string;

    /* create the processes tab */
    string = make_loadavg_string ();
    procdata->loadavg = GTK_WIDGET (gtk_builder_get_object (builder, "load_avg_label"));
    gtk_label_set_text (GTK_LABEL (procdata->loadavg), string);
    g_free (string);

    proctree = proctable_new (procdata);
    scrolled = GTK_WIDGET (gtk_builder_get_object (builder, "processes_scrolled"));

    gtk_container_add (GTK_CONTAINER (scrolled), proctree);

    procdata->endprocessbutton = GTK_WIDGET (gtk_builder_get_object (builder, "endprocessbutton"));
    g_signal_connect (G_OBJECT (procdata->endprocessbutton), "clicked",
                      G_CALLBACK (cb_end_process_button_pressed), procdata);

    /* create popup_menu for the processes tab */
    procdata->popup_menu = gtk_ui_manager_get_widget (procdata->uimanager, "/PopupMenu");
}


GtkWidget *
make_title_label (const char *text)
{
    GtkWidget *label;
    char *full;

    full = g_strdup_printf ("<span weight=\"bold\">%s</span>", text);
    label = gtk_label_new (full);
    g_free (full);

    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    gtk_label_set_use_markup (GTK_LABEL (label), TRUE);

    return label;
}


/* Gonzo: two-color picker dialog for the memory pie (Color 1 =
 * memory itself, Color 2 = GonzoCache usage second wiper). Moved here
 * from the pressure pie after direct correction -- the second wiper
 * was originally built on pressure by mistake (an ambiguous "it's
 * pie" in the original request was resolved toward whichever pie was
 * being discussed at that moment, without confirming). Not built on
 * the stock GtkColorChooserDialog, which only has one color slot and
 * cannot represent two colors in a single dialog at all -- this is a
 * small custom GtkDialog containing a Color 1/Color 2 toggle and one
 * embedded GtkColorChooserWidget (the same picker widget the stock
 * dialog wraps, used standalone here) whose displayed color swaps to
 * match whichever slot is currently selected. */
struct MemTwoColorDialogState {
    GtkWidget *chooser;       /* the embedded GtkColorChooserWidget */
    GdkRGBA    color1;
    GdkRGBA    color2;
    gboolean   editing_color2; /* which slot the chooser currently reflects */
};

static void
two_color_dialog_toggle_changed(GtkToggleButton *btn, gpointer user_data)
{
    MemTwoColorDialogState *state = static_cast<MemTwoColorDialogState*>(user_data);
    if (!gtk_toggle_button_get_active(btn)) return;  /* fires for both buttons; only act on the one becoming active */

    /* Save whatever's currently showing back into the slot it belonged
     * to before switching, so toggling back and forth doesn't lose an
     * in-progress edit that was never confirmed via a full dialog
     * close/reopen. */
    GdkRGBA current;
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(state->chooser), &current);
    if (state->editing_color2)
        state->color2 = current;
    else
        state->color1 = current;

    state->editing_color2 = !state->editing_color2;
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(state->chooser),
        state->editing_color2 ? &state->color2 : &state->color1);
}

/* Opens the two-color dialog for the pressure pie, blocks until closed,
 * and applies both colors to the widget + fires both GSettings-writing
 * callbacks on OK. Returns TRUE (stop propagation) unconditionally,
 * since this fully replaces GsmColorButton's own single-color click
 * handling for this specific widget. */
static gboolean
cb_mem_two_color_picker_clicked(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
    (void)event;
    ProcData * const procdata = static_cast<ProcData*>(data);
    GSMColorButton *cb = GSM_COLOR_BUTTON(widget);

    MemTwoColorDialogState state;
    state.color1 = procdata->config.mem_color;
    state.color2 = procdata->config.gonzocache_color;
    state.editing_color2 = FALSE;

    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        _("Pick Memory Colors"),
        GTK_WINDOW(procdata->app),
        GTK_DIALOG_MODAL,
        _("_Cancel"), GTK_RESPONSE_CANCEL,
        _("_OK"), GTK_RESPONSE_OK,
        NULL);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);
    gtk_container_add(GTK_CONTAINER(content), vbox);

    GtkWidget *toggle_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *radio1 = gtk_radio_button_new_with_label(NULL, _("Color 1 (Memory)"));
    GtkWidget *radio2 = gtk_radio_button_new_with_label_from_widget(
        GTK_RADIO_BUTTON(radio1), _("Color 2 (GonzoCache)"));
    gtk_box_pack_start(GTK_BOX(toggle_box), radio1, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(toggle_box), radio2, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), toggle_box, FALSE, FALSE, 0);

    GtkWidget *chooser = gtk_color_chooser_widget_new();
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(chooser), &state.color1);
    gtk_box_pack_start(GTK_BOX(vbox), chooser, TRUE, TRUE, 0);
    state.chooser = chooser;

    g_signal_connect(radio1, "toggled", G_CALLBACK(two_color_dialog_toggle_changed), &state);
    g_signal_connect(radio2, "toggled", G_CALLBACK(two_color_dialog_toggle_changed), &state);

    gtk_widget_show_all(dialog);
    gint response = gtk_dialog_run(GTK_DIALOG(dialog));

    if (response == GTK_RESPONSE_OK) {
        /* Capture whatever's currently displayed into its slot before
         * reading both back out -- the toggle handler already does
         * this on every switch, but the currently-visible slot was
         * never round-tripped through a toggle if the user never
         * touched the radio buttons at all. */
        GdkRGBA current;
        gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(chooser), &current);
        if (state.editing_color2) state.color2 = current;
        else                      state.color1 = current;

        procdata->config.mem_color        = state.color1;
        procdata->config.gonzocache_color = state.color2;
        gsm_color_button_set_color(cb, &state.color1);
        gsm_color_button_set_color2(cb, &state.color2);

        /* Persist both to GSettings via the same shared helper every
         * other configurable color in this codebase uses -- keeps
         * this consistent with mem/swap/net rather than inventing a
         * separate persistence path. cb_mem_color_changed writes
         * "mem-color" (Color 1's real meaning now), not a
         * pressure-specific key. */
        cb_mem_color_changed(cb, procdata);
        cb_gonzocache_color_changed(cb, procdata);
    }

    gtk_widget_destroy(dialog);
    return TRUE;  /* stop propagation -- do not let GsmColorButton's
                   * own class-level click handler also fire */
}

/* ZRAM pie's equivalent of cb_mem_two_color_picker_clicked, above:
 * Color 1 = ZRAM (swap-color), Color 2 = Pressure. Reuses
 * MemTwoColorDialogState (a generic {chooser, color1, color2,
 * editing_color2} bag despite its name -- nothing in it is actually
 * memory-specific) and the shared two_color_dialog_toggle_changed
 * handler rather than duplicating either. */
static gboolean
cb_zram_two_color_picker_clicked(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
    (void)event;
    ProcData * const procdata = static_cast<ProcData*>(data);
    GSMColorButton *cb = GSM_COLOR_BUTTON(widget);

    MemTwoColorDialogState state;
    state.color1 = procdata->config.swap_color;
    state.color2 = procdata->config.pressure_color;
    state.editing_color2 = FALSE;

    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        _("Pick ZRAM Colors"),
        GTK_WINDOW(procdata->app),
        GTK_DIALOG_MODAL,
        _("_Cancel"), GTK_RESPONSE_CANCEL,
        _("_OK"), GTK_RESPONSE_OK,
        NULL);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);
    gtk_container_add(GTK_CONTAINER(content), vbox);

    GtkWidget *toggle_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *radio1 = gtk_radio_button_new_with_label(NULL, _("Color 1 (ZRAM)"));
    GtkWidget *radio2 = gtk_radio_button_new_with_label_from_widget(
        GTK_RADIO_BUTTON(radio1), _("Color 2 (Pressure)"));
    gtk_box_pack_start(GTK_BOX(toggle_box), radio1, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(toggle_box), radio2, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), toggle_box, FALSE, FALSE, 0);

    GtkWidget *chooser = gtk_color_chooser_widget_new();
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(chooser), &state.color1);
    gtk_box_pack_start(GTK_BOX(vbox), chooser, TRUE, TRUE, 0);
    state.chooser = chooser;

    g_signal_connect(radio1, "toggled", G_CALLBACK(two_color_dialog_toggle_changed), &state);
    g_signal_connect(radio2, "toggled", G_CALLBACK(two_color_dialog_toggle_changed), &state);

    gtk_widget_show_all(dialog);
    gint response = gtk_dialog_run(GTK_DIALOG(dialog));

    if (response == GTK_RESPONSE_OK) {
        GdkRGBA current;
        gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(chooser), &current);
        if (state.editing_color2) state.color2 = current;
        else                      state.color1 = current;

        procdata->config.swap_color     = state.color1;
        procdata->config.pressure_color = state.color2;
        gsm_color_button_set_color(cb, &state.color1);
        gsm_color_button_set_color2(cb, &state.color2);

        cb_swap_color_changed(cb, procdata);
        cb_pressure_color_changed(cb, procdata);
    }

    gtk_widget_destroy(dialog);
    return TRUE;
}


static void
create_sys_view (ProcData *procdata, GtkBuilder * builder)
{
    GtkWidget *cpu_graph_box, *mem_graph_box, *net_graph_box;
    GtkWidget *label,*cpu_label;
    GtkWidget *table;
    GtkWidget *color_picker;
    GtkWidget *picker_alignment;
    LoadGraph *cpu_graph, *mem_graph, *net_graph;

    gint i;
    gchar *title_text;
    gchar *label_text;
    gchar *title_template;

    // Translators: color picker title, %s is CPU, Memory, Swap, Receiving, Sending
    title_template = g_strdup(_("Pick a Color for '%s'"));

    /* The CPU BOX */

    cpu_graph_box = GTK_WIDGET (gtk_builder_get_object (builder, "cpu_graph_box"));

    cpu_graph = new LoadGraph(LOAD_GRAPH_CPU);
    gtk_box_pack_start (GTK_BOX (cpu_graph_box),
                        load_graph_get_widget(cpu_graph),
                        TRUE,
                        TRUE,
                         0);

    GtkWidget* flowbox = GTK_WIDGET (gtk_builder_get_object (builder, "cpu_flow_box"));
    gtk_box_reorder_child (GTK_BOX (cpu_graph_box), flowbox, 1);
    for (i=0;i<procdata->config.num_cpus; i++) {
        GtkWidget *temp_hbox;

        temp_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

        gtk_container_add (GTK_CONTAINER (flowbox), temp_hbox);

        color_picker = gsm_color_button_new (&cpu_graph->colors.at(i), GSMCP_TYPE_CPU);
        g_signal_connect (G_OBJECT (color_picker), "color_set",
                          G_CALLBACK (cb_cpu_color_changed), GINT_TO_POINTER (i));
        gtk_box_pack_start (GTK_BOX (temp_hbox), color_picker, FALSE, TRUE, 0);
        gtk_widget_set_size_request(GTK_WIDGET(color_picker), 32, -1);
        if(procdata->config.num_cpus == 1) {
            label_text = g_strdup (_("CPU"));
        } else {
            label_text = g_strdup_printf (_("CPU%d"), i+1);
        }
        title_text = g_strdup_printf(title_template, label_text);
        label = gtk_label_new (label_text);
        gsm_color_button_set_title(GSM_COLOR_BUTTON(color_picker), title_text);
        g_free(title_text);
        gtk_box_pack_start (GTK_BOX (temp_hbox), label, FALSE, FALSE, 6);
        g_free (label_text);

        cpu_label = gtk_label_new (NULL);
        gtk_label_set_width_chars (GTK_LABEL (cpu_label), 7);
        gtk_label_set_xalign (GTK_LABEL (cpu_label), 0.0);

        gtk_box_pack_start (GTK_BOX (temp_hbox), cpu_label, TRUE, TRUE, 0);
        load_graph_get_labels(cpu_graph)->cpu[i] = cpu_label;

    }

    procdata->cpu_graph = cpu_graph;

    /** The memory box */
    mem_graph_box = GTK_WIDGET (gtk_builder_get_object (builder, "mem_graph_box"));

    mem_graph = new LoadGraph(LOAD_GRAPH_MEM);
    gtk_box_pack_start (GTK_BOX (mem_graph_box),
                        load_graph_get_widget(mem_graph),
                        TRUE,
                        TRUE,
                        0);

    table = GTK_WIDGET (gtk_builder_get_object (builder, "mem_table"));

    /* Gonzo: memory pie, now with the GonzoCache usage second wiper
     * (dark blue) layered on top of the primary memory color -- per
     * direct correction, the second wiper belongs here, not on the
     * pressure pie. Uses the same custom two-color dialog
     * (Color 1 = memory, Color 2 = GonzoCache) as the click handler,
     * replacing the plain single-color "color_set" binding mem/swap
     * otherwise share. */
    color_picker = load_graph_get_mem_color_picker(mem_graph);
    title_text = g_strdup_printf(title_template, _("Memory & Cache"));
    gsm_color_button_set_title(GSM_COLOR_BUTTON(color_picker), title_text);
    g_free(title_text);
    gsm_color_button_set_color2(GSM_COLOR_BUTTON(color_picker),
                                &procdata->config.gonzocache_color);
    g_signal_connect(G_OBJECT(color_picker), "button-press-event",
                     G_CALLBACK(cb_mem_two_color_picker_clicked), procdata);

    gtk_grid_attach (GTK_GRID (table), color_picker, 0, 0, 1, 2);

    label = load_graph_get_labels(mem_graph)->memory;
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_grid_attach (GTK_GRID (table), label, 1, 1, 1, 1);

    /* Gonzo: ZRAM pie, now with a Pressure second wiper layered on top
     * of the primary ZRAM color -- per direct correction, collapsing
     * from 3 pies (Memory, Pressure, ZRAM) back to 2 (Memory & Cache,
     * ZRAM+Pressure). Pressure's rate-based fraction and the ZRAM
     * used/total fraction are genuinely different units (a rate vs. a
     * level), which is why they're two separate wipers on one pie
     * rather than one blended number -- that was an explicit choice,
     * not an oversight (see the earlier decision to keep GonzoCache's
     * residency and pressure's rate as two distinct wipers on Memory
     * for the same reason). Uses the same custom two-color dialog
     * pattern as the memory pie (Color 1 = ZRAM, Color 2 = Pressure).
     * The pressure status label (rate text, "Paused: appname") keeps
     * its own column at 3 -- that's supplementary text a pie wiper
     * can't represent on its own, so it stays alongside its pie
     * rather than disappearing. */
    color_picker = load_graph_get_swap_color_picker(mem_graph);
    title_text = g_strdup_printf(title_template, _("ZRAM & Pressure"));
    gsm_color_button_set_title(GSM_COLOR_BUTTON(color_picker), title_text);
    g_free(title_text);
    gsm_color_button_set_color2(GSM_COLOR_BUTTON(color_picker),
                                &procdata->config.pressure_color);
    g_signal_connect(G_OBJECT(color_picker), "button-press-event",
                     G_CALLBACK(cb_zram_two_color_picker_clicked), procdata);

    gtk_grid_attach (GTK_GRID (table), color_picker, 2, 0, 1, 2);

    label = load_graph_get_labels(mem_graph)->swap;
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_grid_attach (GTK_GRID (table), label, 3, 1, 1, 1);

    procdata->mem_graph = mem_graph;

    /* The net box */
    net_graph_box = GTK_WIDGET (gtk_builder_get_object (builder, "net_graph_box"));

    net_graph = new LoadGraph(LOAD_GRAPH_NET);
    gtk_box_pack_start (GTK_BOX (net_graph_box),
                        load_graph_get_widget(net_graph),
                        TRUE,
                        TRUE,
                        0);

    table = GTK_WIDGET (gtk_builder_get_object (builder, "net_table"));

    color_picker = gsm_color_button_new (
        &net_graph->colors.at(0), GSMCP_TYPE_NETWORK_IN);
    g_signal_connect (G_OBJECT (color_picker), "color_set",
                G_CALLBACK (cb_net_in_color_changed), procdata);
    title_text = g_strdup_printf(title_template, _("Receiving"));
    gsm_color_button_set_title(GSM_COLOR_BUTTON(color_picker), title_text);
    g_free(title_text);

    picker_alignment = GTK_WIDGET (gtk_builder_get_object (builder, "receiving_picker_alignment"));
    gtk_container_add (GTK_CONTAINER (picker_alignment), color_picker);

    label = load_graph_get_labels(net_graph)->net_in;
    gtk_label_set_xalign(GTK_LABEL(label), 1);
    gtk_grid_attach (GTK_GRID (table), label, 2, 0, 1, 1);

    label = load_graph_get_labels(net_graph)->net_in_total;
    gtk_label_set_xalign(GTK_LABEL(label), 1);
    gtk_grid_attach (GTK_GRID (table), label, 2, 1, 1, 1);

    color_picker = gsm_color_button_new (
        &net_graph->colors.at(1), GSMCP_TYPE_NETWORK_OUT);
    g_signal_connect (G_OBJECT (color_picker), "color_set",
                G_CALLBACK (cb_net_out_color_changed), procdata);
    title_text = g_strdup_printf(title_template, _("Sending"));
    gsm_color_button_set_title(GSM_COLOR_BUTTON(color_picker), title_text);
    g_free(title_text);

    picker_alignment = GTK_WIDGET (gtk_builder_get_object (builder, "sending_picker_alignment"));
    gtk_container_add (GTK_CONTAINER (picker_alignment), color_picker);

    label = load_graph_get_labels(net_graph)->net_out;
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_grid_attach (GTK_GRID (table), label, 6, 0, 1, 1);

    label = load_graph_get_labels(net_graph)->net_out_total;
    gtk_label_set_xalign(GTK_LABEL(label), 1);
    gtk_grid_attach (GTK_GRID (table), label, 6, 1, 1, 1);

    procdata->net_graph = net_graph;
    g_free(title_template);
}

static void
menu_item_select_cb (GtkMenuItem *proxy,
                     ProcData *procdata)
{
    GtkAction *action;
    char *message;

    action = gtk_activatable_get_related_action (GTK_ACTIVATABLE(proxy));
    g_assert(action);

    g_object_get (G_OBJECT (action), "tooltip", &message, NULL);
    if (message)
    {
        gtk_statusbar_push (GTK_STATUSBAR (procdata->statusbar),
                    procdata->tip_message_cid, message);
        g_free (message);
    }
}

static void
menu_item_deselect_cb (GtkMenuItem *proxy,
                       ProcData *procdata)
{
    gtk_statusbar_pop (GTK_STATUSBAR (procdata->statusbar),
               procdata->tip_message_cid);
}

static void
connect_proxy_cb (GtkUIManager *manager,
                  GtkAction *action,
                  GtkWidget *proxy,
                  ProcData *procdata)
{
    if (GTK_IS_MENU_ITEM (proxy)) {
        g_signal_connect (proxy, "select",
                          G_CALLBACK (menu_item_select_cb), procdata);
        g_signal_connect (proxy, "deselect",
                          G_CALLBACK (menu_item_deselect_cb), procdata);
    }
}

static void
disconnect_proxy_cb (GtkUIManager *manager,
                     GtkAction *action,
                     GtkWidget *proxy,
                     ProcData *procdata)
{
    if (GTK_IS_MENU_ITEM (proxy)) {
        g_signal_handlers_disconnect_by_func
            (proxy, (void*)(G_CALLBACK(menu_item_select_cb)), procdata);
        g_signal_handlers_disconnect_by_func
            (proxy, (void*)(G_CALLBACK(menu_item_deselect_cb)), procdata);
    }
}

void
create_main_window (ProcData *procdata)
{
    gint i;
    gint width, height, xpos, ypos;
    GtkWidget *app;
    GtkAction *action;
    GtkWidget *menubar;
    GtkWidget *main_box;
    GtkWidget *notebook;
    GtkBuilder *builder;

    builder = gtk_builder_new_from_resource("/org/mate/mate-system-monitor/interface.ui");

    app = GTK_WIDGET (gtk_builder_get_object (builder, "main_window"));
    main_box = GTK_WIDGET (gtk_builder_get_object (builder, "main_box"));

    GdkScreen* screen = gtk_widget_get_screen(app);
    /* use visual, if available */
    GdkVisual* visual = gdk_screen_get_rgba_visual(screen);
    if (visual)
        gtk_widget_set_visual(app, visual);

    width = procdata->config.width;
    height = procdata->config.height;
    xpos = procdata->config.xpos;
    ypos = procdata->config.ypos;
    gtk_window_set_default_size (GTK_WINDOW (app), width, height);
    gtk_window_move(GTK_WINDOW (app), xpos, ypos);

    if (procdata->config.maximized) {
        gtk_window_maximize(GTK_WINDOW(app));
    }

    /* create the menubar */
    procdata->uimanager = gtk_ui_manager_new ();

    /* show tooltips in the statusbar */
    g_signal_connect (procdata->uimanager, "connect_proxy",
                      G_CALLBACK (connect_proxy_cb), procdata);
    g_signal_connect (procdata->uimanager, "disconnect_proxy",
                      G_CALLBACK (disconnect_proxy_cb), procdata);

    gtk_window_add_accel_group (GTK_WINDOW (app),
                                gtk_ui_manager_get_accel_group (procdata->uimanager));

    if (!gtk_ui_manager_add_ui_from_string (procdata->uimanager,
                                            ui_info,
                                            -1,
                                            NULL)) {
        g_error("building menus failed");
    }

    procdata->action_group = gtk_action_group_new ("ProcmanActions");
    gtk_action_group_set_translation_domain (procdata->action_group, NULL);
    gtk_action_group_add_actions (procdata->action_group,
                                  menu_entries,
                                  G_N_ELEMENTS (menu_entries),
                                  procdata);
    gtk_action_group_add_toggle_actions (procdata->action_group,
                                         toggle_menu_entries,
                                         G_N_ELEMENTS (toggle_menu_entries),
                                         procdata);

    gtk_action_group_add_radio_actions (procdata->action_group,
                        radio_menu_entries,
                        G_N_ELEMENTS (radio_menu_entries),
                        procdata->config.whose_process,
                        G_CALLBACK(cb_radio_processes),
                        procdata);

    gtk_action_group_add_radio_actions (procdata->action_group,
                                        priority_menu_entries,
                                        G_N_ELEMENTS (priority_menu_entries),
                                        NORMAL_PRIORITY,
                                        G_CALLBACK(cb_renice),
                                        procdata);

    gtk_ui_manager_insert_action_group (procdata->uimanager,
                                        procdata->action_group,
                                        0);

    menubar = gtk_ui_manager_get_widget (procdata->uimanager, "/MenuBar");
    gtk_box_pack_start (GTK_BOX (main_box), menubar, FALSE, FALSE, 0);
    gtk_box_reorder_child (GTK_BOX (main_box), menubar, 0);

    /* create the main notebook */
    procdata->notebook = notebook = GTK_WIDGET (gtk_builder_get_object (builder, "notebook"));

    create_proc_view(procdata, builder);
    create_sys_view (procdata, builder);
    create_disk_view (procdata, builder);

    g_signal_connect (G_OBJECT (notebook), "switch-page",
              G_CALLBACK (cb_switch_page), procdata);
    g_signal_connect (G_OBJECT (notebook), "change-current-page",
              G_CALLBACK (cb_change_current_page), procdata);

    gtk_widget_show_all(notebook); // need to make page switch work
    gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), procdata->config.current_tab);
    cb_change_current_page (GTK_NOTEBOOK (notebook), procdata->config.current_tab, procdata);
    g_signal_connect (G_OBJECT (app), "delete_event",
                      G_CALLBACK (cb_app_delete),
                      procdata);

    GtkAccelGroup *accel_group;
    GClosure *goto_tab_closure[4];
    accel_group = gtk_accel_group_new ();
    gtk_window_add_accel_group (GTK_WINDOW(app), accel_group);
    for (i = 0; i < 4; ++i) {
        goto_tab_closure[i] = g_cclosure_new_swap (G_CALLBACK (cb_proc_goto_tab),
                                                   GINT_TO_POINTER (i), NULL);
        gtk_accel_group_connect (accel_group, '0'+(i+1),
                                 GDK_MOD1_MASK, GTK_ACCEL_VISIBLE,
                                 goto_tab_closure[i]);
    }

    /* create the statusbar */
    procdata->statusbar = GTK_WIDGET (gtk_builder_get_object (builder, "statusbar"));
    procdata->tip_message_cid = gtk_statusbar_get_context_id
        (GTK_STATUSBAR (procdata->statusbar), "tip_message");

    action = gtk_action_group_get_action (procdata->action_group, "ShowDependencies");
    gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
                      procdata->config.show_tree);

    gtk_builder_connect_signals (builder, NULL);

    gtk_widget_show_all(app);
    procdata->app = app;

    g_object_unref (G_OBJECT (builder));
}

void
do_popup_menu (ProcData *procdata, GdkEventButton *event)
{
    gtk_menu_popup_at_pointer (GTK_MENU (procdata->popup_menu), NULL);
}

void
update_sensitivity(ProcData *data)
{
    const char * const selected_actions[] = { "StopProcess",
                                              "ContProcess",
                                              "EndProcess",
                                              "KillProcess",
                                              "ChangePriority",
                                              "MemoryMaps",
                                              "OpenFiles",
                                              "ProcessProperties" };

    const char * const processes_actions[] = { "ShowActiveProcesses",
                                               "ShowAllProcesses",
                                               "ShowMyProcesses",
                                               "ShowDependencies",
                                               "Refresh"
    };

    size_t i;
    gboolean processes_sensitivity, selected_sensitivity;
    GtkAction *action;

    processes_sensitivity = (data->config.current_tab == PROCMAN_TAB_PROCESSES);
    selected_sensitivity = (processes_sensitivity && data->selection && gtk_tree_selection_count_selected_rows (data->selection) > 0);

    if(data->endprocessbutton) {
        /* avoid error on startup if endprocessbutton
           has not been built yet */
        gtk_widget_set_sensitive(data->endprocessbutton, selected_sensitivity);
    }

    for (i = 0; i != G_N_ELEMENTS(processes_actions); ++i) {
        action = gtk_action_group_get_action(data->action_group,
                                             processes_actions[i]);
        gtk_action_set_sensitive(action, processes_sensitivity);
    }

    for (i = 0; i != G_N_ELEMENTS(selected_actions); ++i) {
        action = gtk_action_group_get_action(data->action_group,
                                             selected_actions[i]);
        gtk_action_set_sensitive(action, selected_sensitivity);
    }
}

void
block_priority_changed_handlers(ProcData *data, bool block)
{
    gint i;
    if (block) {
        for (i = 0; i != G_N_ELEMENTS(priority_menu_entries); ++i) {
            GtkRadioAction *action = GTK_RADIO_ACTION(gtk_action_group_get_action(data->action_group,
                                             priority_menu_entries[i].name));
            g_signal_handlers_block_by_func(action, (gpointer)cb_renice, data);
        }
    } else {
        for (i = 0; i != G_N_ELEMENTS(priority_menu_entries); ++i) {
            GtkRadioAction *action = GTK_RADIO_ACTION(gtk_action_group_get_action(data->action_group,
                                             priority_menu_entries[i].name));
            g_signal_handlers_unblock_by_func(action, (gpointer)cb_renice, data);
        }
    }
}

static void
cb_toggle_tree (GtkAction *action, gpointer data)
{
    ProcData *procdata = static_cast<ProcData*>(data);
    GSettings *settings = procdata->settings;
    gboolean show;

    show = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
    if (show == procdata->config.show_tree)
        return;

    g_settings_set_boolean (settings, "show-tree", show);
}

static void
cb_proc_goto_tab (gint tab)
{
    ProcData *data = ProcData::get_instance ();
    gtk_notebook_set_current_page (GTK_NOTEBOOK (data->notebook), tab);
}
