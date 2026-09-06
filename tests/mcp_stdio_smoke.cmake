execute_process(
    COMMAND "${MCUTRACE_MCP}" --config "${MCUTRACE_MCP_CONFIG}"
    INPUT_FILE "${MCUTRACE_MCP_REQUESTS}"
    OUTPUT_VARIABLE output
    ERROR_VARIABLE errors
    RESULT_VARIABLE status
)

if(NOT status EQUAL 0)
    message(FATAL_ERROR "mcutrace-mcp exited with ${status}: ${errors}")
endif()

foreach(expected IN ITEMS
    "\"protocolVersion\":\"2025-11-25\""
    "\"name\":\"project_overview\""
    "\"structuredContent\""
    "REQ-0139"
    "mcutrace aggregates and validates traceability data"
)
    string(FIND "${output}" "${expected}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "mcutrace-mcp output did not contain ${expected}: ${output}")
    endif()
endforeach()

if(NOT errors STREQUAL "")
    message(FATAL_ERROR "mcutrace-mcp wrote unexpected stderr output: ${errors}")
endif()
