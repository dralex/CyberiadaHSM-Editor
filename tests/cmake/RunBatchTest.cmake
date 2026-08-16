# Run the editor batch mode on INPUT and require the EXPECTED exit code;
# with DUMP_GOOD set, also capture the --dump output into OUTPUT and
# compare it against the good file
set(_args --batch)
if(DEFINED INPUT)
  list(APPEND _args ${INPUT})
endif()
if(DEFINED DUMP_GOOD)
  list(APPEND _args --dump)
  execute_process(COMMAND ${BATCH_BIN} ${_args}
    WORKING_DIRECTORY ${WORKDIR}
    OUTPUT_FILE ${OUTPUT}
    RESULT_VARIABLE result)
else()
  execute_process(COMMAND ${BATCH_BIN} ${_args} RESULT_VARIABLE result)
endif()
if(NOT result EQUAL EXPECTED)
  message(FATAL_ERROR "exit code ${result}, expected ${EXPECTED}")
endif()
if(DEFINED DUMP_GOOD)
  execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files ${OUTPUT} ${DUMP_GOOD}
    RESULT_VARIABLE diff)
  if(NOT diff EQUAL 0)
    message(FATAL_ERROR "dump ${OUTPUT} differs from the good file ${DUMP_GOOD}")
  endif()
endif()
