add_executable(z_gamestate_pipe_writer)

set_target_properties(z_gamestate_pipe_writer PROPERTIES OUTPUT_NAME z_gamestate_pipe_writer)

target_sources(z_gamestate_pipe_writer PRIVATE
    PipeWriterMain.cpp
)

target_link_libraries(z_gamestate_pipe_writer PRIVATE
    zi_always
)
