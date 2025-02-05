IF(NOT EXISTS "/home/vipul/ossim-svn/src/ossim_package_support/cmake/build/install_manifest.txt")
    MESSAGE(FATAL_ERROR "Cannot find install manifest: \"/home/vipul/ossim-svn/src/ossim_package_support/cmake/build/install_manifest.txt\"")
ENDIF()

FILE(READ "/home/vipul/ossim-svn/src/ossim_package_support/cmake/build/install_manifest.txt" files)
STRING(REGEX REPLACE "\n" ";" files "${files}")

FOREACH(file ${files})
    MESSAGE(STATUS "Uninstalling \"${file}\"")
    IF(EXISTS "${file}")
        EXEC_PROGRAM(
            "/usr/local/bin/cmake" ARGS "-E remove \"${file}\""
            OUTPUT_VARIABLE rm_out
            RETURN_VALUE rm_retval
            )
        IF(NOT "${rm_retval}" STREQUAL 0)
            MESSAGE(FATAL_ERROR "Problem when removing \"${file}\"")
        ENDIF()
    ELSE()
        MESSAGE(STATUS "File \"${file}\" does not exist.")
    ENDIF()
ENDFOREACH()
