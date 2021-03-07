cmake_minimum_required(VERSION 3.10)
project(Monochord)

add_executable(monochord src/monochord.c)
target_link_libraries(monochord m rt)

add_executable(recorder src/recorder.c)

add_executable(info_recorder src/info_recorder.c)
