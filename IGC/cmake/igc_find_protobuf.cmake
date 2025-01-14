#=========================== begin_copyright_notice ============================
#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#
#============================ end_copyright_notice =============================

set(PROTOBUF_SRC_ROOT_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/../../../protobuf")

if(NOT EXISTS ${PROTOBUF_SRC_ROOT_FOLDER})
  set(PROTOBUF_SRC_ROOT_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/../../protobuf")
endif()

if(NOT EXISTS ${PROTOBUF_SRC_ROOT_FOLDER})
  set(PROTOBUF_SRC_ROOT_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/../protobuf")
endif()


if(EXISTS ${PROTOBUF_SRC_ROOT_FOLDER}/cmake/protobuf-config.cmake)
  include(${PROTOBUF_SRC_ROOT_FOLDER}/cmake/protobuf-config.cmake)
endif()

if(NOT TARGET protobuf::libprotoc OR NOT TARGET protobuf::protoc)
  message(WARNING "Cannot find Protoc program or library for Protobuf, please visit https://github.com/protocolbuffers/protobuf/releases and install - disable of IGC Metrics")
  set(IGC_METRICS OFF)
else()
  set(IGC_METRICS ON)
  if(NOT COMMAND protobuf_generate_imm)
    message(STATUS "Adding PROTOBUF_GENERATE_CPP cmake function")
    function(protobuf_generate_imm)
      set(_options APPEND_PATH DESCRIPTORS)
        set(_singleargs LANGUAGE OUT_VAR EXPORT_MACRO PROTOC_OUT_DIR)
      if(COMMAND target_sources)
        list(APPEND _singleargs TARGET)
      endif()
      set(_multiargs PROTOS IMPORT_DIRS GENERATE_EXTENSIONS)

      cmake_parse_arguments(protobuf_generate "${_options}" "${_singleargs}" "${_multiargs}" "${ARGN}")

      if(NOT protobuf_generate_PROTOS AND NOT protobuf_generate_TARGET)
        message(SEND_ERROR "Error: protobuf_generate called without any targets or source files")
        return()
      endif()

      if(NOT protobuf_generate_OUT_VAR AND NOT protobuf_generate_TARGET)
        message(SEND_ERROR "Error: protobuf_generate called without a target or output variable")
        return()
      endif()

      if(NOT protobuf_generate_LANGUAGE)
        set(protobuf_generate_LANGUAGE cpp)
      endif()
      string(TOLOWER ${protobuf_generate_LANGUAGE} protobuf_generate_LANGUAGE)

      if(NOT protobuf_generate_PROTOC_OUT_DIR)
        set(protobuf_generate_PROTOC_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR})
      endif()

      if(protobuf_generate_EXPORT_MACRO AND protobuf_generate_LANGUAGE STREQUAL cpp)
        set(_dll_export_decl "dllexport_decl=${protobuf_generate_EXPORT_MACRO}:")
      endif()

      if(NOT protobuf_generate_GENERATE_EXTENSIONS)
        if(protobuf_generate_LANGUAGE STREQUAL cpp)
          set(protobuf_generate_GENERATE_EXTENSIONS .pb.h .pb.cc)
        elseif(protobuf_generate_LANGUAGE STREQUAL python)
          set(protobuf_generate_GENERATE_EXTENSIONS _pb2.py)
        else()
          message(SEND_ERROR "Error: protobuf_generate given unknown Language ${LANGUAGE}, please provide a value for GENERATE_EXTENSIONS")
          return()
        endif()
      endif()

      if(protobuf_generate_TARGET)
        get_target_property(_source_list ${protobuf_generate_TARGET} SOURCES)
        foreach(_file ${_source_list})
          if(_file MATCHES "proto$")
            list(APPEND protobuf_generate_PROTOS ${_file})
          endif()
        endforeach()
      endif()

      if(NOT protobuf_generate_PROTOS)
        message(SEND_ERROR "Error: protobuf_generate could not find any .proto files")
        return()
      endif()

      if(protobuf_generate_APPEND_PATH)
        # Create an include path for each file specified
        foreach(_file ${protobuf_generate_PROTOS})
          get_filename_component(_abs_file ${_file} ABSOLUTE)
          get_filename_component(_abs_path ${_abs_file} PATH)
          list(FIND _protobuf_include_path ${_abs_path} _contains_already)
          if(${_contains_already} EQUAL -1)
            list(APPEND _protobuf_include_path -I ${_abs_path})
          endif()
        endforeach()
      else()
        set(_protobuf_include_path -I ${CMAKE_CURRENT_SOURCE_DIR})
      endif()

      foreach(DIR ${protobuf_generate_IMPORT_DIRS})
        get_filename_component(ABS_PATH ${DIR} ABSOLUTE)
        list(FIND _protobuf_include_path ${ABS_PATH} _contains_already)
        if(${_contains_already} EQUAL -1)
          list(APPEND _protobuf_include_path -I ${ABS_PATH})
        endif()
      endforeach()

      set(_generated_srcs_all)
      foreach(_proto ${protobuf_generate_PROTOS})
        get_filename_component(_abs_file ${_proto} ABSOLUTE)
        get_filename_component(_abs_dir ${_abs_file} DIRECTORY)
        get_filename_component(_basename ${_proto} NAME_WE)
        file(RELATIVE_PATH _rel_dir ${CMAKE_CURRENT_SOURCE_DIR} ${_abs_dir})

        set(_possible_rel_dir)
        if (NOT protobuf_generate_APPEND_PATH)
          set(_possible_rel_dir ${_rel_dir}/)
        endif()

        set(_generated_srcs)
        foreach(_ext ${protobuf_generate_GENERATE_EXTENSIONS})
          list(APPEND _generated_srcs "${protobuf_generate_PROTOC_OUT_DIR}/${_possible_rel_dir}${_basename}${_ext}")
        endforeach()

        if(protobuf_generate_DESCRIPTORS AND protobuf_generate_LANGUAGE STREQUAL cpp)
          set(_descriptor_file "${CMAKE_CURRENT_BINARY_DIR}/${_basename}.desc")
          set(_dll_desc_out "--descriptor_set_out=${_descriptor_file}")
          list(APPEND _generated_srcs ${_descriptor_file})
        endif()
        list(APPEND _generated_srcs_all ${_generated_srcs})

        get_target_property(protobuf-comp protobuf::protoc IMPORTED_LOCATION_RELEASE)
        message(STATUS "Running ${protobuf_generate_LANGUAGE} protocol buffer compiler on ${_proto}")
        get_filename_component(WRK_DIR_PROTO "${_abs_file}" DIRECTORY)
        execute_process(
          COMMAND ${protobuf-comp} --${protobuf_generate_LANGUAGE}_out ${_dll_export_decl}${protobuf_generate_PROTOC_OUT_DIR} ${_dll_desc_out} ${_protobuf_include_path} ${_abs_file}
          WORKING_DIRECTORY ${WRK_DIR_PROTO}
          RESULT_VARIABLE rv
          OUTPUT_VARIABLE ov
          ERROR_VARIABLE ev
        )
        if(NOT ${rv} EQUAL "0")
          message("${protobuf-comp} --${protobuf_generate_LANGUAGE}_out ${_dll_export_decl}${protobuf_generate_PROTOC_OUT_DIR} ${_dll_desc_out} ${_protobuf_include_path} ${_abs_file}")
          message("Result='${rv}'")
          message("Output='${ov}'")
          message("Error='${ev}'")
          message(FATAL_ERROR "Protobuf compiler : failed to compile schema ${_abs_file}")
        endif()
      endforeach()

      set_source_files_properties(${_generated_srcs_all} PROPERTIES GENERATED TRUE)
      if(protobuf_generate_OUT_VAR)
        set(${protobuf_generate_OUT_VAR} ${_generated_srcs_all} PARENT_SCOPE)
      endif()
      if(protobuf_generate_TARGET)
        target_sources(${protobuf_generate_TARGET} PRIVATE ${_generated_srcs_all})
      endif()
    endfunction()

    function(PROTOBUF_GENERATE_IMM_CPP SRCS HDRS)
      cmake_parse_arguments(protobuf_generate_cpp "" "EXPORT_MACRO;DESCRIPTORS" "" ${ARGN})

      set(_proto_files "${protobuf_generate_cpp_UNPARSED_ARGUMENTS}")
      if(NOT _proto_files)
        message(SEND_ERROR "Error: PROTOBUF_GENERATE_CPP() called without any proto files")
        return()
      endif()

      if(PROTOBUF_GENERATE_CPP_APPEND_PATH)
        set(_append_arg APPEND_PATH)
      endif()

      if(protobuf_generate_cpp_DESCRIPTORS)
        set(_descriptors DESCRIPTORS)
      endif()

      if(DEFINED PROTOBUF_IMPORT_DIRS AND NOT DEFINED Protobuf_IMPORT_DIRS)
        set(Protobuf_IMPORT_DIRS "${PROTOBUF_IMPORT_DIRS}")
      endif()

      if(DEFINED Protobuf_IMPORT_DIRS)
        set(_import_arg IMPORT_DIRS ${Protobuf_IMPORT_DIRS})
      endif()

      set(_outvar)
      protobuf_generate_imm(${_append_arg} ${_descriptors} LANGUAGE cpp EXPORT_MACRO ${protobuf_generate_cpp_EXPORT_MACRO} OUT_VAR _outvar ${_import_arg} PROTOS ${_proto_files})

      set(${SRCS})
      set(${HDRS})
      if(protobuf_generate_cpp_DESCRIPTORS)
        set(${protobuf_generate_cpp_DESCRIPTORS})
      endif()

      foreach(_file ${_outvar})
        if(_file MATCHES "cc$")
          list(APPEND ${SRCS} ${_file})
        elseif(_file MATCHES "desc$")
          list(APPEND ${protobuf_generate_cpp_DESCRIPTORS} ${_file})
        else()
          list(APPEND ${HDRS} ${_file})
        endif()
      endforeach()
      set(${SRCS} ${${SRCS}} PARENT_SCOPE)
      set(${HDRS} ${${HDRS}} PARENT_SCOPE)
      if(protobuf_generate_cpp_DESCRIPTORS)
        set(${protobuf_generate_cpp_DESCRIPTORS} "${${protobuf_generate_cpp_DESCRIPTORS}}" PARENT_SCOPE)
      endif()
    endfunction()
  endif()
endif()
