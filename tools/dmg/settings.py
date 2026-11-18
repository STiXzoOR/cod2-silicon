# dmgbuild settings for the macOS disk image: the app, a link to Applications, and the window
# people see when they open it. scripts/make-dmg.sh runs:
#
#   dmgbuild -s tools/dmg/settings.py -D app="<path>/CoD2 Unbound.app" \
#       -D background=tools/dmg/background.png "CoD2 Unbound" out.dmg
#
# dmgbuild writes Finder's own view settings into the image, so nothing here drives Finder.
# background.png and background@2x.png are drawn by background.py and rendered by render.sh; the
# positions below are the ones that picture is drawn around.
import os.path
import plistlib

app = defines["app"]  # noqa: F821
name = os.path.basename(app)

with open(os.path.join(app, "Contents", "Info.plist"), "rb") as file:
    icon_name = plistlib.load(file)["CFBundleIconFile"]
if not icon_name.endswith(".icns"):
    icon_name += ".icns"

format = "ULMO"  # LZMA: the smallest image, readable by every macOS this app runs on
files = [app]
symlinks = {"Applications": "/Applications"}
hide_extension = [name]
icon = os.path.join(app, "Contents", "Resources", icon_name)

background = os.path.abspath(defines["background"])  # noqa: F821
# 660 wide; 400 for the picture plus the title bar. The picture is 420 tall, so no edge shows.
window_rect = ((240, 240), (660, 432))
default_view = "icon-view"
show_status_bar = False
show_tab_view = False
show_toolbar = False
show_pathbar = False
show_sidebar = False
show_icon_preview = False
show_item_info = False
arrange_by = None
label_pos = "bottom"
icon_size = 128
text_size = 13
icon_locations = {name: (180, 170), "Applications": (480, 170)}
