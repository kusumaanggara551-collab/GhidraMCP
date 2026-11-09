#!/usr/bin/env python3
"""Patch a PS2Recomp checkout: app icon + name, on-screen touch controls.

usage: patch_runtime.py <PS2Recomp dir> <app name> <true|false for touch controls>
"""
import html
import pathlib
import re
import shutil
import sys

root = pathlib.Path(sys.argv[1])
app_name = sys.argv[2]
touch = sys.argv[3].strip().lower() == "true"
here = pathlib.Path(__file__).resolve().parent

rt = root / "ps2xRuntime"
app = root / "android" / "app" / "src" / "main"


def write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


# ---------------------------------------------------------------- icon + name
write(app / "res" / "values" / "ic_launcher_colors.xml", """<?xml version="1.0" encoding="utf-8"?>
<resources>
    <color name="ic_launcher_background">#0B1D3A</color>
</resources>
""")

write(app / "res" / "drawable" / "ic_launcher_foreground.xml", """<?xml version="1.0" encoding="utf-8"?>
<vector xmlns:android="http://schemas.android.com/apk/res/android"
    android:width="108dp"
    android:height="108dp"
    android:viewportWidth="108"
    android:viewportHeight="108">
    <path
        android:fillColor="#FFFFFF"
        android:pathData="M54,24 L59.5,44 L83,61 L83,67 L59,60 L57.5,73 L65,79 L65,83 L54,80.5 L43,83 L43,79 L50.5,73 L49,60 L25,67 L25,61 L48.5,44 Z" />
    <path
        android:fillColor="#E5484D"
        android:pathData="M52,36 L56,36 L56,48 L52,48 Z" />
</vector>
""")

write(app / "res" / "mipmap-anydpi-v26" / "ic_launcher.xml", """<?xml version="1.0" encoding="utf-8"?>
<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">
    <background android:drawable="@color/ic_launcher_background" />
    <foreground android:drawable="@drawable/ic_launcher_foreground" />
</adaptive-icon>
""")

manifest = app / "AndroidManifest.xml"
s = manifest.read_text(encoding="utf-8")
old = 'android:label="PS2 Recomp"'
assert s.count(old) == 1, "label not found in AndroidManifest.xml"
new = ('android:label="%s"\n'
       '        android:icon="@mipmap/ic_launcher"\n'
       '        android:roundIcon="@mipmap/ic_launcher"') % html.escape(app_name, quote=True)
manifest.write_text(s.replace(old, new), encoding="utf-8")
print("icon + name patched")

# ------------------------------------------------------------ touch controls
if touch:
    # 1) build the overlay as part of the ps2_runtime library
    cm = rt / "CMakeLists.txt"
    s = cm.read_text(encoding="utf-8")
    anchor = "    src/lib/ps2_pad.cpp\n"
    assert s.count(anchor) == 1, "ps2_pad.cpp not found in CMakeLists.txt"
    cm.write_text(s.replace(anchor, anchor + "    src/lib/ps2_touch_overlay.cpp\n"),
                  encoding="utf-8")

    # 2) let the pad backend read the touch state
    pad = rt / "src" / "lib" / "ps2_pad.cpp"
    s = pad.read_text(encoding="utf-8")
    inc = "#include <cstring>\n"
    line = "    data[2] = static_cast<uint8_t>(btns & 0xFF);\n"
    assert s.count(inc) == 1 and s.count(line) == 1, "ps2_pad.cpp anchors not found"
    s = s.replace(inc, inc + "#if defined(__ANDROID__)\n"
                  "void ps2x_touch_apply(uint16_t &btns, uint8_t *data);\n#endif\n")
    s = s.replace(line, "#if defined(__ANDROID__)\n    ps2x_touch_apply(btns, data);\n#endif\n" + line)
    pad.write_text(s, encoding="utf-8")

    # 3) draw the overlay each frame, right after the game frame is drawn
    rc = rt / "src" / "lib" / "ps2_runtime.cpp"
    s = rc.read_text(encoding="utf-8")
    found = re.findall(r"^[ \t]*DrawTexturePro\(frameTex[^;]*;[ \t]*\r?\n", s, re.M)
    assert len(found) == 1, \
        "expected exactly one DrawTexturePro(frameTex...) in ps2_runtime.cpp, found %d" % len(found)
    s = s.replace(found[0], found[0] +
                  "#if defined(__ANDROID__)\n        ps2x_touch_overlay_draw();\n#endif\n")
    s = "#if defined(__ANDROID__)\nvoid ps2x_touch_overlay_draw();\n#endif\n" + s
    rc.write_text(s, encoding="utf-8")

    shutil.copy(here / "ps2_touch_overlay.cpp", rt / "src" / "lib" / "ps2_touch_overlay.cpp")
    print("touch controls patched")
else:
    print("touch controls skipped")
