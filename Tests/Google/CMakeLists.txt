add_subdirectory(Core)

if(RTS_BUILD_ZEROHOUR)
    add_executable(z_googletest)
    target_link_libraries(z_googletest PRIVATE
        corei_googletest
        z_gameenginedevice
        z_wwaudio
        zi_always
    )
    add_test(NAME z_googletest COMMAND z_googletest)
endif()
