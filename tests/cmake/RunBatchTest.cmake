# Run the editor batch mode on INPUT and require the EXPECTED exit code
if(DEFINED INPUT)
  execute_process(COMMAND ${BATCH_BIN} --batch ${INPUT} RESULT_VARIABLE result)
else()
  execute_process(COMMAND ${BATCH_BIN} --batch RESULT_VARIABLE result)
endif()
if(NOT result EQUAL EXPECTED)
  message(FATAL_ERROR "exit code ${result}, expected ${EXPECTED}")
endif()
