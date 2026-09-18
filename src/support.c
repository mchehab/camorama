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

#if GTK_MAJOR_VERSION < 4
    return gtk3_error_dialog(message);
#else
    return gtk4_error_dialog(message);
#endif
}
