project(embedded_some_ip_tp)

find_package(LTTngUST REQUIRED)

include_directories(include)

add_library(${PROJECT_NAME} STATIC src/someip_tp.c)

target_link_libraries(${PROJECT_NAME} PRIVATE
    LTTng::UST
)
