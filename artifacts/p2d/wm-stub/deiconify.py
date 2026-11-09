import sys, ctypes
w = int(sys.argv[1])
x11 = ctypes.CDLL("libX11.so.6")
x11.XOpenDisplay.restype = ctypes.c_void_p
x11.XInternAtom.restype = ctypes.c_ulong
x11.XDefaultRootWindow.restype = ctypes.c_ulong
d = x11.XOpenDisplay(None)
if not d:
    print("  no display"); sys.exit(1)
class XClientMessageEvent(ctypes.Structure):
    _fields_ = [("type", ctypes.c_int), ("serial", ctypes.c_ulong),
                ("send_event", ctypes.c_int), ("display", ctypes.c_void_p),
                ("window", ctypes.c_ulong), ("message_type", ctypes.c_ulong),
                ("format", ctypes.c_int), ("l", ctypes.c_long * 5)]
class XEvent(ctypes.Union):
    _fields_ = [("xclient", XClientMessageEvent), ("pad", ctypes.c_long * 24)]
x11.XInternAtom.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
wm_change = x11.XInternAtom(d, b"WM_CHANGE_STATE", 0)
x11.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
root = x11.XDefaultRootWindow(d)
ev = XEvent()
ev.xclient.type = 33
ev.xclient.window = w
ev.xclient.message_type = wm_change
ev.xclient.format = 32
ev.xclient.l[0] = 1
x11.XSendEvent.restype = ctypes.c_int
x11.XSendEvent.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int,
                           ctypes.c_long, ctypes.POINTER(XEvent)]
x11.XSendEvent(d, root, 0, (1<<20) | (1<<19), ctypes.byref(ev))
x11.XFlush.argtypes = [ctypes.c_void_p]
x11.XFlush(d)
print("   已发送 WM_CHANGE_STATE state=NormalState")
