# engine/core/meson.build
core_inc = include_directories('.')

order_book_lib = static_library('order_book', 'order_book.cpp', include_directories: core_inc)
declare_dependency(link_with: order_book_lib, include_directories: core_inc)
