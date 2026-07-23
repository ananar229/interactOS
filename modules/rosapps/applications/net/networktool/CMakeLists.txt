list(APPEND SOURCE
    src/main.c
    src/MainWindow.c
    src/AboutDialog.c
    src/SettingsDialog.c
    src/common/OutputEdit.c
    src/common/ResolveIPv4.c
    src/common/TcpQuery.c
    src/common/Translations.c
    src/common/OuiVendors.c
    src/common/DialogResize.c
    src/tabs/InfoTab.c
    src/tabs/NetstatTab.c
    src/tabs/PingTab.c
    src/tabs/LookupTab.c
    src/tabs/TracerouteTab.c
    src/tabs/WhoisTab.c
    src/tabs/FingerTab.c
    src/tabs/PortScanTab.c
    src/tabs/SpeedTab.c
    src/tabs/LanScanTab.c)

add_executable(networktool ${SOURCE} resources/app.rc)
target_include_directories(networktool PRIVATE src src/common)
set_module_type(networktool win32gui UNICODE)
add_importlibs(networktool comctl32 advapi32 user32 gdi32 comdlg32 ws2_32 iphlpapi dnsapi winhttp msvcrt kernel32)
add_cd_file(TARGET networktool DESTINATION reactos/system32 FOR all)
