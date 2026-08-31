# Included by ctest itself on every run (TEST_INCLUDE_FILES), so the printed
# commit is always the one under test, not the one configured last
find_program(_git_program git)
if(_git_program)
  get_filename_component(_repo_dir "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
  execute_process(COMMAND ${_git_program} -C "${_repo_dir}" log -1 "--format=%H (%D) %s"
    OUTPUT_VARIABLE _revision OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET RESULT_VARIABLE _revision_rc)
  if(_revision_rc EQUAL 0)
    execute_process(COMMAND ${_git_program} -C "${_repo_dir}" status --porcelain -uno
      OUTPUT_VARIABLE _local_changes ERROR_QUIET)
    if(_local_changes STREQUAL "")
      message("revision under test: ${_revision}")
    else()
      message("revision under test: ${_revision} + local changes")
    endif()
  endif()
endif()
