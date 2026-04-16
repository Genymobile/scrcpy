# Get app icon (`--get-app-icon`)

The `--get-app-icon` option allows you to extract the icon of one or more installed applications from the Android device.

## Syntax

```
scrcpy --get-app-icon=<packageID>[:path]
```

- `<packageID>`: Required. One or more package IDs, separated by a comma (`,`) (e.g., `com.android.settings,com.android.chrome`). If the special value `all` is used, icons for all installed applications are extracted.
- `[:path]`: Optional. Destination path to save the extracted icons.

## Path behavior

- If `path` is an existing folder, each icon is saved as `<packageID>.png` inside that folder.
- If `path` is an existing file, the extracted icon will overwrite that file, so be careful.
- If `path` does not exist, the containing folder is created and the file is named with the last segment of the path (regardless of extension).
- If `path` is `tmpDir`, the files are stored in the user's temporary folder: `scrcpy/icons/<deviceId>`.

## Examples

- `scrcpy --get-app-icon=com.android.settings`
- `scrcpy --get-app-icon=com.android.settings:~/icon.png`
- `scrcpy --get-app-icon=com.android.settings,com.android.chrome:$HOME/icons/`
- `scrcpy --get-app-icon=all:/home/user/all_icons/`
- `scrcpy --get-app-icon=com.android.chrome,com.android.settings:tmpDir`

## Notes

- If multiple `packageID`s are specified, all indicated icons are extracted.
- For `all`, the `--list-apps` functionality is reused to obtain all package IDs and extract their icons.
- If `:path` is not specified, icons are saved in the current directory.

