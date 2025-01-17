/*========================== begin_copyright_notice ============================

Copyright (C) 2017-2021 Intel Corporation

SPDX-License-Identifier: MIT

============================= end_copyright_notice ===========================*/

//                  enumName                              stringName
//                  --------                              ----------
DEFINE_SHADER_STAT( STATS_ISA_INST_COUNT,                 "ISA count simd8"  )
DEFINE_SHADER_STAT( STATS_ISA_INST_COUNT_SIMD16,          "ISA count simd16" )
DEFINE_SHADER_STAT( STATS_ISA_INST_COUNT_SIMD32,          "ISA count simd32" )
DEFINE_SHADER_STAT( STATS_ISA_SPILL8,                     "simd8 spill"      )
DEFINE_SHADER_STAT( STATS_ISA_SPILL16,                    "simd16 spill"     )
DEFINE_SHADER_STAT( STATS_ISA_SPILL32,                    "simd32 spill"     )
DEFINE_SHADER_STAT( STATS_ISA_EARLYEXIT8,                 "simd8  early exit")
DEFINE_SHADER_STAT( STATS_ISA_EARLYEXIT16,                "simd16 early exit")
DEFINE_SHADER_STAT( STATS_ISA_EARLYEXIT32,                "simd32 early exit")
DEFINE_SHADER_STAT( STATS_ISA_BASIC_BLOCKS,               "Basic Blocks"     )
DEFINE_SHADER_STAT( STATS_ISA_ALU,                        "Alu"              )
DEFINE_SHADER_STAT( STATS_ISA_LOGIC,                      "Logic"            )
DEFINE_SHADER_STAT( STATS_ISA_MOV,                        "Mov"              )
DEFINE_SHADER_STAT( STATS_ISA_SEND,                       "Send"             )
DEFINE_SHADER_STAT( STATS_ISA_SEL_CMP,                    "Select/Compare"   )
DEFINE_SHADER_STAT( STATS_ISA_STRUCTCF,                   "Struct CF"        )
DEFINE_SHADER_STAT( STATS_ISA_GOTOJOIN,                   "Goto Join"        )
DEFINE_SHADER_STAT( STATS_ISA_THREADCF,                   "Thread CF"        )
DEFINE_SHADER_STAT( STATS_ISA_CALL,                       "Call"             )
DEFINE_SHADER_STAT( STATS_ISA_OTHERS,                     "Others"           )
DEFINE_SHADER_STAT( STATS_MAX_SHADER_STATS_ITEMS,         ""                 )
