if(NOT DEFINED MCUTRACE_VSIX OR NOT EXISTS "${MCUTRACE_VSIX}")
    message(FATAL_ERROR "MCUTRACE_VSIX must name a produced VSIX")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar tf "${MCUTRACE_VSIX}"
    RESULT_VARIABLE list_result
    OUTPUT_VARIABLE contents
    ERROR_VARIABLE list_error)
if(NOT list_result EQUAL 0)
    message(FATAL_ERROR "Unable to inspect ${MCUTRACE_VSIX}: ${list_error}")
endif()

# @req REQ-0179 REQ-0190 REQ-0191
foreach(required IN ITEMS
        "extension/package.json"
        "extension/extension.js"
        "extension/agent-integration.js"
        "extension/bin/mcutrace-lsp${MCUTRACE_VSIX_EXECUTABLE_SUFFIX}"
        "extension/bin/mcutrace-mcp${MCUTRACE_VSIX_EXECUTABLE_SUFFIX}"
        "extension/skills/mcutrace/SKILL.md")
    string(FIND "${contents}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "VSIX is missing ${required}")
    endif()
endforeach()

message(STATUS "Verified mcutrace VSIX payload: ${MCUTRACE_VSIX}")
