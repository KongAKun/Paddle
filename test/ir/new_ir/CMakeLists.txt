file(
  GLOB TEST_INTERP_CASES
  RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
  "test_*.py")
string(REPLACE ".py" "" TEST_INTERP_CASES "${TEST_INTERP_CASES}")

set(TEST_IR_SYSTEM_CASES test_build_model)
list(REMOVE_ITEM TEST_INTERP_CASES ${TEST_IR_SYSTEM_CASES})

foreach(target ${TEST_INTERP_CASES})
  py_test_modules(${target} MODULES ${target} ENVS GLOG_v=1
                  FLAGS_enable_new_ir_in_executor=true)
endforeach()

foreach(target ${TEST_IR_SYSTEM_CASES})
  py_test_modules(${target} MODULES ${target} ENVS GLOG_v=1
                  FLAGS_enable_new_ir_api=true)
endforeach()
