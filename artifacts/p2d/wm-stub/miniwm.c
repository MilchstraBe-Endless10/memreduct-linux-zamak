/* 最小 WM 桩：只为让 Xvfb 下的最小化/恢复可测。不接管装饰，只处理
 * WM_PROTOCOLS 与 unmapped/mapped 的 ICONIFY 请求。 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    Display *d = XOpenDisplay(NULL);
    if (!d) { fprintf(stderr, "no display\n"); return 1; }
    Window root = DefaultRootWindow(d);
    Atom wm_protocols = XInternAtom(d, "WM_PROTOCOLS", False);
    Atom wm_delete    = XInternAtom(d, "WM_DELETE_WINDOW", False);
    Atom wm_state     = XInternAtom(d, "WM_STATE", False);
    Atom net_wm_state = XInternAtom(d, "_NET_WM_STATE", False);
    Atom hidden_a     = XInternAtom(d, "_NET_WM_STATE_HIDDEN", False);

    XSelectInput(d, root, SubstructureRedirectMask | SubstructureNotifyMask);
    printf("miniwm: ready\n"); fflush(stdout);

    for (;;) {
        XEvent e;
        XNextEvent(d, &e);
        if (e.type == MapRequest) {
            XMapWindow(d, e.xmaprequest.window);
            printf("miniwm: mapped 0x%lx\n", e.xmaprequest.window); fflush(stdout);
        } else if (e.type == UnmapNotify) {
            /* 窗口自己撤出：记录，但不去重映射（否则无法保持最小化） */
            printf("miniwm: unmapped 0x%lx\n", e.xunmap.window); fflush(stdout);
        } else if (e.type == ConfigureRequest) {
            XConfigureRequestEvent *c = &e.xconfigurerequest;
            XWindowChanges wc;
            wc.x = c->x; wc.y = c->y; wc.width = c->width; wc.height = c->height;
            wc.border_width = c->border_width;
            wc.sibling = c->above;
            wc.stack_mode = c->detail;
            XConfigureWindow(d, c->window, c->value_mask, &wc);
        } else if (e.type == ClientMessage) {
            /* WM_CHANGE_STATE: 图标化/取消图标化请求（gtk_window_iconify 走这条） */
            Atom wm_change_state = XInternAtom(d, "WM_CHANGE_STATE", False);
            if (e.xclient.message_type == wm_protocols) {
                if ((Atom)e.xclient.data.l[0] == wm_delete) {
                    printf("miniwm: WM_DELETE_WINDOW 0x%lx\n", e.xclient.window);
                    fflush(stdout);
                    XDestroyWindow(d, e.xclient.window);
                }
            } else if (e.xclient.message_type == wm_change_state) {
                long state = e.xclient.data.l[0];
                printf("miniwm: WM_CHANGE_STATE 0x%lx state=%ld\n",
                       e.xclient.window, state);
                fflush(stdout);
                if (state == IconicState) {
                    XUnmapWindow(d, e.xclient.window);
                    printf("miniwm:   -> unmapped for iconify\n"); fflush(stdout);
                } else if (state == NormalState) {
                    XMapWindow(d, e.xclient.window);
                    printf("miniwm:   -> mapped for deiconify\n"); fflush(stdout);
                }
            }
        }
    }
    return 0;
}
