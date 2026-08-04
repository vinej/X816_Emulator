# GetGitRevision.cmake.in

set(HEAD_DIR "C:/quartus/projects/X816_Emulator")
set(OUTPUT_FILE "C:/quartus/projects/X816_Emulator/build-trace/generated/git_rev.h")
set(GIT_EXECUTABLE "C:/kick/Git/mingw64/bin/git.exe")

if(GIT_EXECUTABLE)
    # Check for dirty repo
    execute_process(
        COMMAND ${GIT_EXECUTABLE} diff-index --quiet HEAD --
        WORKING_DIRECTORY ${HEAD_DIR}
        RESULT_VARIABLE GIT_DIFF_EXIT_CODE
        ERROR_QUIET
    )

    # 0 = Clean, 1 = Dirty
    if(GIT_DIFF_EXIT_CODE EQUAL 0)
        set(HASH_LEN 8)
        set(REV_SUFFIX "")
    else()
        set(HASH_LEN 7)
        set(REV_SUFFIX "+")
    endif()

    # Get the hash
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short=${HASH_LEN} HEAD
        WORKING_DIRECTORY ${HEAD_DIR}
        OUTPUT_VARIABLE GIT_REV
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(GIT_REV)
        string(APPEND GIT_REV "${REV_SUFFIX}")
    endif()
else()
    # If the git package wasn't found
    set(GIT_REV "00000000")
endif()

# fallback if the git command failed to find a repo or commit hash
if(NOT GIT_REV)
    set(GIT_REV "00000000")
endif()

set(GIT_REV_CONTENT "#define GIT_REV \"${GIT_REV}\"\n")

if(EXISTS "${OUTPUT_FILE}")
    file(READ "${OUTPUT_FILE}" OLD_CONTENT)
else()
    set(OLD_CONTENT "")
endif()

if(NOT "${GIT_REV_CONTENT}" STREQUAL "${OLD_CONTENT}")
    file(WRITE "${OUTPUT_FILE}" "${GIT_REV_CONTENT}")
    message(STATUS "Git revision updated to: ${GIT_REV}")
endif()
