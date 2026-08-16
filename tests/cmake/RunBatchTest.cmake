# Run the editor batch mode on INPUT and require the EXPECTED exit code;
# SCRIPT adds an edit script, DUMP_GOOD compares the --dump output (written
# to OUTPUT), SAVE_OUT/SAVE_GOOD compare the saved document and re-open it
set(_args --batch)
if(DEFINED INPUT)
  list(APPEND _args ${INPUT})
endif()
if(DEFINED SCRIPT)
  list(APPEND _args --script ${SCRIPT})
endif()
if(DEFINED SAVE_OUT)
  list(APPEND _args --save ${SAVE_OUT})
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
if(DEFINED SAVE_GOOD)
  execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files ${SAVE_OUT} ${SAVE_GOOD}
    RESULT_VARIABLE diff)
  if(NOT diff EQUAL 0)
    message(FATAL_ERROR "document ${SAVE_OUT} differs from the good file ${SAVE_GOOD}")
  endif()
  # write->read round trip: the saved document must open cleanly
  execute_process(COMMAND ${BATCH_BIN} --batch ${SAVE_OUT} RESULT_VARIABLE reopen)
  if(NOT reopen EQUAL 0)
    message(FATAL_ERROR "saved document ${SAVE_OUT} does not re-open (exit ${reopen})")
  endif()
endif()
