add_executable(z_matrixprecisiontest)
set_target_properties(z_matrixprecisiontest PROPERTIES OUTPUT_NAME matrixprecisiontest)

target_sources(z_matrixprecisiontest PRIVATE
    "main.cpp"
)

target_link_libraries(z_matrixprecisiontest PRIVATE
    core_config
    core_utility
    core_wwstub # avoid linking GameEngine
    z_wwvegas
)
