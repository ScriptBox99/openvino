# Copyright (C) 2018-2021 Intel Corporation
# SPDX-License-Identifier: Apache-2.0
#

set(TARGET_NAME cpuUnitTests)

if(BUILD_SHARED_LIBS)
    set (OBJ_LIB $<TARGET_OBJECTS:ov_intel_cpu_plugin_obj>)
endif()

addIeTargetTest(
        NAME ${TARGET_NAME}
        ROOT ${CMAKE_CURRENT_SOURCE_DIR}
        INCLUDES
            $<TARGET_PROPERTY:ov_intel_cpu_plugin,SOURCE_DIR>/src
            $<TARGET_PROPERTY:ov_intel_cpu_plugin,SOURCE_DIR>/src/nodes
            $<TARGET_PROPERTY:openvino::conditional_compilation,INTERFACE_INCLUDE_DIRECTORIES>
        OBJECT_FILES
            ${OBJ_LIB}
        LINK_LIBRARIES
            gtest
            gtest_main
            gmock
            mkldnn
            inference_engine_transformations
            inference_engine_lp_transformations
            ov_shape_inference
            inference_engine_s
            unitTestUtils
            inference_engine_snippets
            ngraphFunctions
        ADD_CPPLINT
        LABELS
            CPU
)

ie_faster_build(${TARGET_NAME}
    UNITY
)

target_include_directories(${TARGET_NAME} SYSTEM PRIVATE
    $<TARGET_PROPERTY:mkldnn,SOURCE_DIR>/src/common
    $<TARGET_PROPERTY:mkldnn,SOURCE_DIR>/src/cpu
    $<TARGET_PROPERTY:mkldnn,SOURCE_DIR>/include)
