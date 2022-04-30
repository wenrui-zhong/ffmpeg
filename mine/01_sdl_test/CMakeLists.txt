cmake_minimum_required(VERSION 3.5)

project(test_sdl)

# Add an executable with the above sources
add_executable(hello
    main.cpp
)

# link the library target
target_link_libraries( hello
    PRIVATE 
        SDL2
)


