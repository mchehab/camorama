#include "interface.h"
#include "support.h"
#if GTK_MAJOR_VERSION < 4
#include "gtk3_callbacks.h"
#else
#include "gtk4_callbacks.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <config.h>

int error_dialog(char *message)
{
#if GTK_MAJOR_VERSION >= 4
    return gtk4_error_dialog(message);
#else
    GtkApplication *app;
    GtkWindow *parent = NULL;
    GtkWidget *dialog;
    int test;

    app = GTK_APPLICATION(g_application_get_default());
    if (GTK_IS_APPLICATION(app))
        parent = gtk_application_get_active_window(app);

    /*
     * Startup errors can happen before the main window has been presented.
     * GTK warns when a dialog is mapped without a transient parent, so keep
     * those errors on stderr instead of creating an orphaned dialog.
     */
    if (!parent) {
        g_printerr("Camorama: %s\n", message);
        return 0;
    }

    dialog = gtk_message_dialog_new(parent,
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_ERROR,
                                    GTK_BUTTONS_CLOSE, "%s", message);

    test = gtk_common_dialog_run(GTK_DIALOG(dialog));
    gtk_common_destroy_widget(dialog);
    return test;
#endif
}
