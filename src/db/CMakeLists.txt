# DB 模块:Statement + Database(header-only),链接 libsqlite3
add_library(db INTERFACE)
target_include_directories(db INTERFACE .)
find_package(SQLite3 REQUIRED)
target_link_libraries(db INTERFACE SQLite::SQLite3)
