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
            /* EWMH 最大化：应用发 _NET_WM_STATE 到 root */
            Atom net_wm_state = XInternAtom(d, "_NET_WM_STATE", False);
            Atom max_h = XInternAtom(d, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
            Atom max_v = XInternAtom(d, "_NET_WM_STATE_MAXIMIZED_VERT", False);
            if (e.xclient.message_type == net_wm_state &&
                e.xclient.message_type != wm_protocols) {
                /* l[0]=0 remove, 1 add, 2 toggle; l[1],l[2] 是两个 property atom */
                long action = e.xclient.data.l[0];
                Atom p1 = (Atom)e.xclient.data.l[1];
                Atom p2 = (Atom)e.xclient.data.l[2];
                Window w = e.xclient.window;
                int want_max = 0;
                if (action == 1) want_max = 1;
                else if (action == 0) want_max = 0;
                else if (action == 2) want_max = -1; /* toggle: 简化，直接最大化 */
                int is_max = 0;
                if (p1 == max_h || p1 == max_v || p2 == max_h || p2 == max_v) {
                    printf("miniwm: _NET_WM_STATE action=%ld max_h=%d max_v=%d\n",
                           action, p1 == max_h || p2 == max_h,
                           p1 == max_v || p2 == max_v);
                    fflush(stdout);
                    is_max = (want_max >= 0) ? want_max : 1;
                    if (is_max) {
                        XMoveResizeWindow(d, w, 0, 0,
                                          DisplayWidth(d, 0), DisplayHeight(d, 0));
                        /* 关键：把 _NET_WM_STATE 写成最大化，GTK 才能读到真实状态，
                         * 下次点击 toggle 才会发 action=0 而不是又一次 action=1 */
                        Atom states[2] = { max_h, max_v };
                        XChangeProperty(d, w, net_wm_state, XA_ATOM, 32,
                                        PropModeReplace, (unsigned char *)states, 2);
                        printf("miniwm:   -> maximized to %dx%d, _NET_WM_STATE set\n",
                               DisplayWidth(d, 0), DisplayHeight(d, 0));
                    } else {
                        XMoveResizeWindow(d, w, 60, 40, 337, 415);
                        XChangeProperty(d, w, net_wm_state, XA_ATOM, 32,
                                        PropModeReplace, NULL, 0);
                        printf("miniwm:   -> restored to 337x415, _NET_WM_STATE cleared\n");
                    }
                    fflush(stdout);
                }
            }
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
