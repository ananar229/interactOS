
add_library(themeui MODULE themeui.rc)
set_target_properties(themeui PROPERTIES LINKER_LANGUAGE C)
add_dependencies(themeui psdk)
set_module_type(themeui win32dll ENTRYPOINT 0)
add_cd_file(TARGET themeui DESTINATION reactos/system32 FOR all)
