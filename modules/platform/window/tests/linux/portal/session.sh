#!/bin/sh
# Runs "$@" with the fake FileChooser portal on a private session bus, so the smoke's dialog
# check goes through the portal. Without dbus-run-session or Python's dbus bindings it runs
# "$@" with SILK_WINDOW_DIALOG instead, as on any machine without a portal.
here=$(dirname "$0")
if command -v dbus-run-session >/dev/null 2>&1; then
  for python in python3.12 python3; do
    if "$python" -c "import dbus, gi" >/dev/null 2>&1; then
      echo "dialogs: through the fake FileChooser portal"
      exec dbus-run-session -- "$python" "$here/fake_portal.py" "$@"
    fi
  done
fi
echo "dialogs: through SILK_WINDOW_DIALOG (no dbus-run-session or Python dbus)"
exec env SILK_WINDOW_DIALOG='printf "/tmp/%s-%s.svg\n" "$1" "$2"' "$@"
