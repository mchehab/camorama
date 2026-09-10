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
    GtkWidget *dialog;
    int test;

    dialog = gtk_message_dialog_new(NULL,
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_ERROR,
                                    GTK_BUTTONS_CLOSE, "%s", message);

    test = gtk_common_dialog_run(GTK_DIALOG(dialog));
    gtk_common_destroy_widget(dialog);
    return test;
}
