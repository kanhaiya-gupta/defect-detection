# CompilerWarnings.cmake - World-class warning and diagnostic setup
cmake_minimum_required(VERSION 3.21)

function(normitri_enable_warnings target_name)
  if(MSVC)
    target_compile_options("${target_name}" PRIVATE
      /W4
      /w14640  # conversion, possible loss of data
      /permissive-
    )
    if(NORMITRI_WARNINGS_AS_ERRORS)
      target_compile_options("${target_name}" PRIVATE /WX)
    endif()
  else()
    target_compile_options("${target_name}" PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Wconversion
      -Wsign-conversion
      -Wold-style-cast
      -Wnull-dereference
      -Wdouble-promotion
    )
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_compile_options("${target_name}" PRIVATE -Wlogical-op)
    endif()
    if(NORMITRI_WARNINGS_AS_ERRORS)
      target_compile_options("${target_name}" PRIVATE -Werror)
    endif()
  endif()
endfunction()
