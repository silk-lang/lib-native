"""A FileChooser portal on a private session bus, for the Linux smoke test.

It answers OpenFile and SaveFile as the smoke's SILK_WINDOW_DIALOG command does, with
/tmp/<open|save>-<title>.svg, sent as a percent-encoded file:// URI. A call that names no
X11 parent, or an open call without the expected filter, is answered as failed (response 2).
Runs the command after its arguments and exits with its status.
"""
import subprocess
import sys

import dbus
import dbus.service
from dbus.mainloop.glib import DBusGMainLoop
from gi.repository import GLib

NAME = "org.freedesktop.portal.Desktop"
PATH = "/org/freedesktop/portal/desktop"
CHOOSER = "org.freedesktop.portal.FileChooser"


class Request(dbus.service.Object):
    @dbus.service.signal("org.freedesktop.portal.Request", signature="ua{sv}")
    def Response(self, response, results):
        pass


class Portal(dbus.service.Object):
    def __init__(self, bus):
        super().__init__(bus, PATH)
        self.bus = bus

    @dbus.service.method("org.freedesktop.DBus.Properties", in_signature="ss", out_signature="v")
    def Get(self, interface, name):
        if interface == CHOOSER and name == "version":
            return dbus.UInt32(4, variant_level=1)
        raise dbus.exceptions.DBusException("no such property", name="org.freedesktop.DBus.Error.UnknownProperty")

    def answer(self, kind, sender, parent, title, options):
        token = str(options.get("handle_token", "t"))
        path = "%s/request/%s/%s" % (PATH, sender[1:].replace(".", "_"), token)
        request = Request(self.bus, path)
        ok = parent.startswith("x11:")
        if kind == "open":
            patterns = [str(p[1]) for f in options.get("filters", []) for p in f[1]]
            ok = ok and "*.[sS][vV][gG]" in patterns and not bool(options.get("multiple", False))
        else:
            ok = ok and str(options.get("current_name", "")) != ""
        uri = "file:///tmp/%s%%2D%s.svg" % (kind, title)

        def respond():
            if ok:
                request.Response(dbus.UInt32(0), {"uris": dbus.Array([uri], signature="s", variant_level=1)})
            else:
                request.Response(dbus.UInt32(2), dbus.Dictionary({}, signature="sv"))
            return False

        GLib.timeout_add(50, respond)
        return dbus.ObjectPath(path)

    @dbus.service.method(CHOOSER, in_signature="ssa{sv}", out_signature="o", sender_keyword="sender")
    def OpenFile(self, parent, title, options, sender=None):
        return self.answer("open", sender, parent, title, options)

    @dbus.service.method(CHOOSER, in_signature="ssa{sv}", out_signature="o", sender_keyword="sender")
    def SaveFile(self, parent, title, options, sender=None):
        return self.answer("save", sender, parent, title, options)


def main():
    DBusGMainLoop(set_as_default=True)
    bus = dbus.SessionBus()
    name = dbus.service.BusName(NAME, bus)
    portal = Portal(bus)
    loop = GLib.MainLoop()
    child = subprocess.Popen(sys.argv[1:])
    status = []

    def poll():
        code = child.poll()
        if code is None:
            return True
        status.append(code)
        loop.quit()
        return False

    GLib.timeout_add(20, poll)
    loop.run()
    del name, portal
    sys.exit(status[0])


main()
