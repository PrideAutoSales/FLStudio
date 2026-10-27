# Adds every plugins/<Name>/ folder that has its own CMakeLists.txt. Folders
# without one (shared code such as CruzCompShared) are skipped.
file(GLOB children LIST_DIRECTORIES true RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/*")
list(SORT children)

foreach(child IN LISTS children)
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${child}" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${child}/CMakeLists.txt")
        add_subdirectory("${child}")
    endif()
endforeach()
