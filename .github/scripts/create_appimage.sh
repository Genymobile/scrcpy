#!/bin/bash

rm -rf scrcpy_dir
rm scrcpy*.AppImage
rm linuxdeploy-x86_64.AppImage

trap "{ echo script failed; exit; }" ERR

# download linuxdeploy.appimage
wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
chmod +x linuxdeploy-x86_64.AppImage

# create AppRun
cat << EOF > AppRun
#!/bin/sh
SELF=\$(readlink -f "\$0")
HERE=\${SELF%/*}
export PATH="\${HERE}/usr/bin/:\${HERE}/usr/sbin/:\${HERE}/usr/games/:\${HERE}/bin/:\${HERE}/sbin/\${PATH:+:\$PATH}"
export LD_LIBRARY_PATH="\${HERE}/usr/lib/:\${HERE}/usr/lib/i386-linux-gnu/:\${HERE}/usr/lib/x86_64-linux-gnu/:\${HERE}/usr/lib32/:\${HERE}/usr/lib64/:\${HERE}/lib/:\${HERE}/lib/i386-linux-gnu/:\${HERE}/lib/x86_64-linux-gnu/:\${HERE}/lib32/:\${HERE}/lib64/\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}"
export PYTHONPATH="\${HERE}/usr/share/pyshared/\${PYTHONPATH:+:\$PYTHONPATH}"
export XDG_DATA_DIRS="\${HERE}/usr/share/\${XDG_DATA_DIRS:+:\$XDG_DATA_DIRS}"
export PERLLIB="\${HERE}/usr/share/perl5/:\${HERE}/usr/lib/perl5/\${PERLLIB:+:\$PERLLIB}"
export GSETTINGS_SCHEMA_DIR="\${HERE}/usr/share/glib-2.0/schemas/\${GSETTINGS_SCHEMA_DIR:+:\$GSETTINGS_SCHEMA_DIR}"
export QT_PLUGIN_PATH="\${HERE}/usr/lib/qt4/plugins/:\${HERE}/usr/lib/i386-linux-gnu/qt4/plugins/:\${HERE}/usr/lib/x86_64-linux-gnu/qt4/plugins/:\${HERE}/usr/lib32/qt4/plugins/:\${HERE}/usr/lib64/qt4/plugins/:\${HERE}/usr/lib/qt5/plugins/:\${HERE}/usr/lib/i386-linux-gnu/qt5/plugins/:\${HERE}/usr/lib/x86_64-linux-gnu/qt5/plugins/:\${HERE}/usr/lib32/qt5/plugins/:\${HERE}/usr/lib64/qt5/plugins/\${QT_PLUGIN_PATH:+:\$QT_PLUGIN_PATH}"


export ADB="\${HERE}/usr/bin/adb"
export SCRCPY_ICON_PATH="\${HERE}/scrcpy.png"
export SCRCPY_SERVER_PATH="\${HERE}/usr/share/scrcpy/scrcpy-server"


EXEC=\${HERE}/usr/bin/scrcpy
exec "\${EXEC}" "\$@"
EOF

echo "making appdir"
./linuxdeploy-x86_64.AppImage --appdir scrcpy_dir 1> /dev/null

mkdir -p scrcpy_dir/usr/share/scrcpy/ 1> /dev/null
cp scrcpy/scrcpy-server scrcpy_dir/usr/share/scrcpy/scrcpy-server 1> /dev/null


./linuxdeploy-x86_64.AppImage --appdir scrcpy_dir -e scrcpy/x/app/scrcpy -i scrcpy/app/data/scrcpy.png --create-desktop-file --custom-apprun=AppRun \
-e /usr/bin/ffmpeg -e /usr/bin/ffplay  -e /usr/bin/ffprobe  -e /usr/bin/qt-faststart \
-l /usr/lib/x86_64-linux-gnu/libSDL2-2.0.so.0.10.0 -l /usr/lib/x86_64-linux-gnu/libSDL2-2.0.so.0 \
-l /usr/lib/android-sdk/platform-tools/adb -e /usr/bin/adb \
-l /lib/x86_64-linux-gnu/libusb-1.0.so.0.2.0 -l /lib/x86_64-linux-gnu/libusb-1.0.so.0 \
-l /lib/x86_64-linux-gnu/libpango-1.0.so.0 -l /lib/x86_64-linux-gnu/libpangoft2-1.0.so.0 -l /lib/x86_64-linux-gnu/libgobject-2.0.so.0 \
--output appimage

echo "Done"



