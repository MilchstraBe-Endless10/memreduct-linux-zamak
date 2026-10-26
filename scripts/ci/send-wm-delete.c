/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <X11/Xlib.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    char *end = NULL;
    unsigned long long raw_window;
    Display *display;
    XEvent event;

    if (argc != 2) {
        fprintf(stderr, "usage: %s X11_WINDOW_ID\n", argv[0]);
        return 2;
    }

    errno = 0;
    raw_window = strtoull(argv[1], &end, 0);
    if (errno != 0 || end == argv[1] || *end != '\0' || raw_window == 0) {
        fprintf(stderr, "invalid X11 window ID: %s\n", argv[1]);
        return 2;
    }

    display = XOpenDisplay(NULL);
    if (display == NULL) {
        fputs("cannot connect to the isolated X server\n", stderr);
        return 1;
    }

    memset(&event, 0, sizeof(event));
    event.xclient.type = ClientMessage;
    event.xclient.window = (Window)raw_window;
    event.xclient.message_type = XInternAtom(display, "WM_PROTOCOLS", False);
    event.xclient.format = 32;
    event.xclient.data.l[0] = (long)XInternAtom(display, "WM_DELETE_WINDOW", False);
    event.xclient.data.l[1] = CurrentTime;

    if (!XSendEvent(display, (Window)raw_window, False, NoEventMask, &event)) {
        fputs("X server rejected WM_DELETE_WINDOW request\n", stderr);
        XCloseDisplay(display);
        return 1;
    }
    XSync(display, False);
    printf("CI_X11_WM_DELETE_SENT window=0x%llx protocol=WM_PROTOCOLS/WM_DELETE_WINDOW\n",
           raw_window);
    XCloseDisplay(display);
    return 0;
}
