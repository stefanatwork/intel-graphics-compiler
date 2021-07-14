/*========================== begin_copyright_notice ============================

Copyright (C) 2017-2021 Intel Corporation

SPDX-License-Identifier: MIT

============================= end_copyright_notice ===========================*/

#ifndef NULLSTR
#define NULLSTR ""
#endif
#ifndef UNUSED
#define UNUSED ""
#endif

// There are 4 available types: ET_BOOL, ET_INT32, ET_INT64 and ET_CSTR.
// DEF_VISA_OPTION(ENUM, ET_BOOL,    STRING, UNUSED,    DEFAULT_VAL)
// DEF_VISA_OPTION(ENUM, ET_INT32,   STRING, ERROR_MSG, DEFAULT_VAL)
// DEF_VISA_OPTION(ENUM, ET_INT64,   STRING, ERROR_MSG, DEFAULT_VAL)
// DEF_VISA_OPTION(ENUM, ET_CSTR,    STRING, ERROR_MSG, DEFAULT_VAL)
// Note: ET_2xINT32 is a 64 bit value set using 2 int32 Hi32 Lo32
// DEF_VISA_OPTION(ENUM, ET_2xINT32, STRING, ERROR_MSG, DEFAULT_VAL)


//=== Debugging options ===
DEF_VISA_OPTION(vISA_DumpPasses,            ET_BOOL, "-dumpPassesAll",   UNUSED, false)
// subsumes the above (should replace; 0 = none, 1 = only when modifications are present, 2 = all)
DEF_VISA_OPTION(vISA_DumpPassesSubset,      ET_INT32, "-dumpPassesSubset",
  "0 means none; 1 means only when modifications happen; 2 means all passes", 0)
// dump out dot file for debugging
DEF_VISA_OPTION(vISA_DumpDot,               ET_BOOL, "-dot",             UNUSED, false)
DEF_VISA_OPTION(vISA_DumpDotAll,            ET_BOOL, "-dotAll",          UNUSED, false)
DEF_VISA_OPTION(VISA_FullIRVerify,          ET_BOOL, "-fullIRVerify",    UNUSED, false)
// dump each option while it is being set by setOption()
DEF_VISA_OPTION(vISA_dumpVISAOptions,       ET_BOOL, "-dumpVisaOptions", UNUSED, false)
// dump all options after we have finished parsing
DEF_VISA_OPTION(vISA_dumpVISAOptionsAll,    ET_BOOL, "-dumpVisaOptionsAll", UNUSED, false)
DEF_VISA_OPTION(vISA_Debug,                 ET_BOOL, "-debug",           UNUSED, false)
DEF_VISA_OPTION(vISA_DebugParse,            ET_BOOL, "-debugParse",      UNUSED, false)
DEF_VISA_OPTION(vISA_DebugConsoleDump,      ET_BOOL, "-dumpDebugConsoleOutput", UNUSED, false)
DEF_VISA_OPTION(vISA_EmitLocation,          ET_BOOL, "-emitLocation",    UNUSED, false)
DEF_VISA_OPTION(vISA_dumpRPE,               ET_BOOL, "-dumpRPE",         UNUSED, false)
DEF_VISA_OPTION(vISA_dumpLiveness,               ET_BOOL, "-dumpLiveness",         UNUSED, false)
DEF_VISA_OPTION(vISA_disableInstDebugInfo,  ET_BOOL, "-disableInstDebugInfo",      UNUSED, false)
DEF_VISA_OPTION(vISA_analyzeMove,           ET_BOOL, "-analyzeMove",     UNUSED, false)
DEF_VISA_OPTION(vISA_skipFDE,               ET_BOOL, "-skipFDE",         UNUSED, false)
// setting this flag makes VISA emit matching name for variable wrt visaasm file
// but this makes it impossible to emit correct elf, so this is strictly for debugging
DEF_VISA_OPTION(vISA_UseFriendlyNameInDbg,  ET_BOOL, "-useFriendlyNameInDbg", UNUSED, false)
DEF_VISA_OPTION(vISA_removeInstrinsics,           ET_BOOL, "-removeInstrinsics",     UNUSED, true)
DEF_VISA_OPTION(vISA_addSWSBInfo,           ET_BOOL, "-addSWSBInfo",     UNUSED, true)
DEF_VISA_OPTION(vISA_DumpRAIntfGraph,       ET_BOOL, "-dumpintf",        UNUSED, false)
DEF_VISA_OPTION(vISA_DumpGenOffset,         ET_BOOL, "-dumpgenoffset",   UNUSED, false)

//=== Optimization options ===
DEF_VISA_OPTION(vISA_EnableAlways,          ET_BOOL, NULLSTR,            UNUSED, true)
DEF_VISA_OPTION(vISA_EnableSendFusion,      ET_BOOL, "-enableSendFusion",   UNUSED, false)
DEF_VISA_OPTION(vISA_EnableWriteFusion,     ET_BOOL, "-enableWriteFusion",  UNUSED, false)
DEF_VISA_OPTION(vISA_EnableAtomicFusion,    ET_BOOL, "-enableAtomicFusion", UNUSED, false)
DEF_VISA_OPTION(vISA_LocalCopyProp,         ET_BOOL, "-nocopyprop",      UNUSED, true)
DEF_VISA_OPTION(vISA_LocalFlagOpt,          ET_BOOL, "-noflagopt",       UNUSED, true)
DEF_VISA_OPTION(vISA_LocalMACopt,           ET_BOOL, "-nomacopt",        UNUSED, true)
DEF_VISA_OPTION(vISA_LocalCleanMessageHeader, ET_BOOL, "-nomsgheaderopt", UNUSED, true)
DEF_VISA_OPTION(vISA_LocalRenameRegister,   ET_BOOL, "-noregrenaming",   UNUSED, true)
DEF_VISA_OPTION(vISA_LocalDefHoist,         ET_BOOL, "-nodefhoist",      UNUSED, true)
DEF_VISA_OPTION(vISA_FoldAddrImmed,         ET_BOOL, "-nofoldaddrimmed", UNUSED, true)
DEF_VISA_OPTION(vISA_enableCSEL,            ET_BOOL, "-disablecsel",     UNUSED, true)
DEF_VISA_OPTION(vISA_OptReport,             ET_BOOL, "-optreport",       UNUSED, false)
DEF_VISA_OPTION(vISA_MergeScalar,           ET_BOOL, "-nomergescalar",   UNUSED, true)
DEF_VISA_OPTION(vISA_EnableMACOpt,          ET_BOOL, "-nomac",           UNUSED, true)
DEF_VISA_OPTION(vISA_EnableDCE,             ET_BOOL, "-dce",             UNUSED, false)
DEF_VISA_OPTION(vISA_DisableleHFOpt,        ET_BOOL, "-disableHFOpt",    UNUSED, false)
DEF_VISA_OPTION(vISA_enableUnsafeCP_DF,     ET_BOOL, "-enableUnsafeCP_DF", UNUSED, false)
DEF_VISA_OPTION(vISA_EnableStructurizer,    ET_BOOL, "-enableStructurizer",  UNUSED, false)
DEF_VISA_OPTION(vISA_StructurizerCF,        ET_BOOL, "-noSCF",           "-noSCF: structurizer generates UCF only", true)
DEF_VISA_OPTION(vISA_EnableScalarJmp,       ET_BOOL, "-noScalarJmp",     UNUSED, true)
DEF_VISA_OPTION(vISA_enableCleanupBindless, ET_BOOL, NULLSTR,            UNUSED, true)
DEF_VISA_OPTION(vISA_EnableSplitVariables,  ET_BOOL, "-noSplitVariables", UNUSED, false)
DEF_VISA_OPTION(vISA_ChangeMoveType,        ET_BOOL, "-ALTMode",         UNUSED, true)
DEF_VISA_OPTION(vISA_accSubstitution,       ET_BOOL, "-noAccSub",          UNUSED, true)
DEF_VISA_OPTION(vISA_accSubBeforeRA,       ET_BOOL, "-noAccSubBRA",          UNUSED, true)
DEF_VISA_OPTION(vISA_doAccSubAfterSchedule, ET_BOOL, "-accSubPostSchedule",    UNUSED, true)
DEF_VISA_OPTION(vISA_localizationForAccSub, ET_BOOL, "-localizeForACC",    UNUSED, false)
DEF_VISA_OPTION(vISA_mathAccSub, ET_BOOL, "-mathAccSub",    UNUSED, false)
DEF_VISA_OPTION(vISA_src2AccSub, ET_BOOL, "-src2AccSub",    UNUSED, false)
DEF_VISA_OPTION(vISA_ifCvt,                 ET_BOOL, "-noifcvt",     UNUSED, true)
DEF_VISA_OPTION(vISA_RegSharingHeuristics,  ET_BOOL, (IGC_MANGLE("-regSharingHeuristics")), UNUSED, false)
DEF_VISA_OPTION(vISA_LVN,                   ET_BOOL, "-nolvn",       UNUSED, true)
// only affects acc substitution for now
DEF_VISA_OPTION(vISA_numGeneralAcc,         ET_INT32, "-numGeneralAcc", "USAGE: -numGeneralAcc <accNum>\n", 0)
DEF_VISA_OPTION(vISA_reassociate,           ET_BOOL, "-noreassoc",   UNUSED, true)
DEF_VISA_OPTION(vISA_split4GRFVar,          ET_BOOL, "-no4GRFSplit", UNUSED, true)
DEF_VISA_OPTION(vISA_divergentBB,           ET_BOOL, "-divergentBB", UNUSED, true)
DEF_VISA_OPTION(vISA_splitInstructions,     ET_BOOL, "-noSplitInstructions", UNUSED, true)
DEF_VISA_OPTION(vISA_ignoreBFRounding,      ET_BOOL, (IGC_MANGLE("-ignoreBFRounding")), UNUSED, false)
DEF_VISA_OPTION(vISA_scheduleFenceCommit,   ET_BOOL, NULLSTR, UNUSED, true)
DEF_VISA_OPTION(vISA_SkipRedundantFillInRMW,ET_BOOL, "-rmwopt", UNUSED, false)

//=== code gen options ===
DEF_VISA_OPTION(vISA_noSrc1Byte,          ET_BOOL, "-nosrc1byte",         UNUSED, false)
DEF_VISA_OPTION(vISA_expandPlane,         ET_BOOL, "-expandPlane",        UNUSED, false)
DEF_VISA_OPTION(vISA_FImmToHFImm,         ET_BOOL,  NULLSTR,        UNUSED, false)
DEF_VISA_OPTION(vISA_cacheSamplerHeader,  ET_BOOL, "-noSamplerHeaderCache", UNUSED, true)
DEF_VISA_OPTION(vISA_forceSamplerHeader,  ET_BOOL, "-forceSamplerHeader", UNUSED, false)
DEF_VISA_OPTION(vISA_noncoherentStateless, ET_BOOL, "-ncstateless",       UNUSED, false)
DEF_VISA_OPTION(vISA_enablePreemption,    ET_BOOL,  "-enablePreemption",  UNUSED, false)
DEF_VISA_OPTION(VISA_EnableBarrierInstCounterBits, ET_BOOL, "-enableBarrierInstCounterBits", UNUSED, false)
DEF_VISA_OPTION(vISA_forceFPMAD,            ET_BOOL, NULLSTR,               UNUSED, true)
DEF_VISA_OPTION(vISA_DisableMixMode,        ET_BOOL, "-disableMixMode",     UNUSED, false)
DEF_VISA_OPTION(vISA_DisableHFMath,         ET_BOOL, "-disableHFMath",      UNUSED, false)
DEF_VISA_OPTION(vISA_ForceMixMode,          ET_BOOL, "-forceMixMode",       UNUSED, false)
DEF_VISA_OPTION(vISA_UseSends,              ET_BOOL, "-nosends",            UNUSED, true)
DEF_VISA_OPTION(vISA_doAlign1Ternary,       ET_BOOL, "-noalign1ternary",    UNUSED, true)
DEF_VISA_OPTION(vISA_loadThreadPayload,     ET_BOOL, "-noLoadPayload",      UNUSED, true)
DEF_VISA_OPTION(vISA_foldEOTtoPrevSend,     ET_BOOL, "-foldEOT",            UNUSED, false)
DEF_VISA_OPTION(vISA_hasRNEandDenorm,       ET_BOOL, "-hasRNEandDenorm",    UNUSED, false)
DEF_VISA_OPTION(vISA_forceNoFP64bRegioning, ET_BOOL, "-noFP64bRegion",      UNUSED, false)
DEF_VISA_OPTION(vISA_noStitchExternFunc,    ET_BOOL, "-noStitchExternFunc", UNUSED, true)
DEF_VISA_OPTION(vISA_autoLoadLocalID,       ET_BOOL, NULLSTR,            UNUSED, false)
DEF_VISA_OPTION(vISA_loadCrossThreadConstantData, ET_BOOL, NULLSTR,      UNUSED, true)
DEF_VISA_OPTION(vISA_useInlineData,         ET_BOOL, (IGC_MANGLE("-useInlineData")),   UNUSED, false)
DEF_VISA_OPTION(vISA_loadThreadPayloadStartReg, ET_INT32, (IGC_MANGLE("-setStartReg")),UNUSED, 1)
DEF_VISA_OPTION(vISA_emitCrossThreadOffR0Reloc,  ET_BOOL, "-emitCrossThreadOffR0Reloc",    UNUSED, false)
DEF_VISA_OPTION(vISA_CodePatch,   ET_INT32, (IGC_MANGLE("-codePatch")),        UNUSED, 0)
DEF_VISA_OPTION(vISA_Linker,      ET_INT32, (IGC_MANGLE("-linker")),        UNUSED, 0)

//=== RA options ===
DEF_VISA_OPTION(vISA_RoundRobin,            ET_BOOL, "-noroundrobin",    UNUSED, true)
DEF_VISA_OPTION(vISA_PrintRegUsage,         ET_BOOL, "-printregusage",   UNUSED, false)
DEF_VISA_OPTION(vISA_IPA,                   ET_BOOL, "-noipa",           UNUSED, true)
DEF_VISA_OPTION(vISA_LocalRA,               ET_BOOL, "-nolocalra",       UNUSED, true)
DEF_VISA_OPTION(vISA_LocalRARoundRobin,     ET_BOOL, "-nolocalraroundrobin", UNUSED, true)
DEF_VISA_OPTION(vISA_ForceSpills,           ET_BOOL, "-forcespills",     UNUSED, false)
DEF_VISA_OPTION(vISA_NoIndirectForceSpills, ET_BOOL, "-noindirectforcespills", UNUSED, false)
DEF_VISA_OPTION(vISA_AbortOnSpill,          ET_BOOL, "-abortonspill",    UNUSED, false)
DEF_VISA_OPTION(vISA_VerifyRA,              ET_BOOL, "-verifyra",        UNUSED, false)
DEF_VISA_OPTION(vISA_LocalBankConflictReduction, ET_BOOL, "-nolocalBCR",   UNUSED, true)
DEF_VISA_OPTION(vISA_FailSafeRA,            ET_BOOL, "-nofailsafera",    UNUSED, true)
DEF_VISA_OPTION(vISA_FlagSpillCodeCleanup,  ET_BOOL, "-disableFlagSpillClean",            UNUSED, true)
DEF_VISA_OPTION(vISA_GRFSpillCodeCleanup,   ET_BOOL, NULLSTR,            UNUSED, true)
DEF_VISA_OPTION(vISA_SpillSpaceCompression, ET_BOOL, "-nospillcompression",            UNUSED, true)
DEF_VISA_OPTION(vISA_ConsiderLoopInfoInRA,  ET_BOOL, "-noloopra",        UNUSED, true)
DEF_VISA_OPTION(vISA_ReserveR0,             ET_BOOL, "-reserveR0",       UNUSED, false)
DEF_VISA_OPTION(vISA_SpiltLLR,              ET_BOOL, "-nosplitllr",      UNUSED, true)
DEF_VISA_OPTION(vISA_EnableGlobalScopeAnalysis,   ET_BOOL,  "-enableGlobalScopeAnalysis", UNUSED, false)
DEF_VISA_OPTION(vISA_LocalDeclareSplitInGlobalRA, ET_BOOL, "-noLocalSplit",        UNUSED, true)
DEF_VISA_OPTION(vISA_DisableSpillCoalescing, ET_BOOL, "-nospillcleanup", UNUSED, false)
DEF_VISA_OPTION(vISA_GlobalSendVarSplit,    ET_BOOL, "-globalSendVarSplit", UNUSED, false)
DEF_VISA_OPTION(vISA_NoRemat,               ET_BOOL, "-noremat",         UNUSED, false)
DEF_VISA_OPTION(vISA_ForceRemat,            ET_BOOL, "-forceremat",      UNUSED, false)
DEF_VISA_OPTION(vISA_SpillMemOffset,        ET_INT32, "-spilloffset",           "USAGE: -spilloffset <offset>\n",     0)
DEF_VISA_OPTION(vISA_ReservedGRFNum,        ET_INT32, "-reservedGRFNum",        "USAGE: -reservedGRFNum <regNum>\n",  0)
DEF_VISA_OPTION(vISA_TotalGRFNum,           ET_INT32, "-TotalGRFNum",           "USAGE: -TotalGRFNum <regNum>\n",     128)
DEF_VISA_OPTION(vISA_GRFNumToUse,           ET_INT32, "-GRFNumToUse",           "USAGE: -GRFNumToUse <regNum>\n",       0)
DEF_VISA_OPTION(vISA_RATrace,               ET_BOOL, "-ratrace", UNUSED, false)
DEF_VISA_OPTION(vISA_FastSpill,             ET_BOOL, "-fasterRA", UNUSED, false)
DEF_VISA_OPTION(vISA_AbortOnSpillThreshold, ET_INT32, NULLSTR, UNUSED, 0)
DEF_VISA_OPTION(vISA_enableBCR, ET_BOOL, "-enableBCR",   UNUSED, false)
DEF_VISA_OPTION(vISA_forceBCR, ET_BOOL, "-forceBCR",   UNUSED, false)
DEF_VISA_OPTION(vISA_enableBundleCR, ET_BOOL, "-enableBundleCR",   UNUSED, true)
DEF_VISA_OPTION(vISA_IntrinsicSplit,       ET_BOOL, "-doSplit", UNUSED, false)
DEF_VISA_OPTION(vISA_LraFFWindowSize,       ET_INT32, "-lraFFWindowSize", UNUSED, 12)
DEF_VISA_OPTION(vISA_SplitGRFAlignedScalar, ET_BOOL, "-nosplitGRFalignedscalar", UNUSED, true)

DEF_VISA_OPTION(vISA_VerifyAugmentation,    ET_BOOL, "-verifyaugmentation", UNUSED, false)
DEF_VISA_OPTION(vISA_VerifyExplicitSplit,   ET_BOOL, "-verifysplit", UNUSED, false)
DEF_VISA_OPTION(vISA_DumpRegChart,          ET_BOOL, "-dumpregchart", UNUSED, false)
DEF_VISA_OPTION(vISA_DumpAllBCInfo,          ET_BOOL, "-dumpAllBCInfo", UNUSED, false)
DEF_VISA_OPTION(vISA_LinearScan,               ET_BOOL, "-linearScan",       UNUSED, false)
DEF_VISA_OPTION(vISA_LSFristFit,               ET_BOOL, "-lsFirstFit",       UNUSED, true)
DEF_VISA_OPTION(vISA_verifyLinearScan,               ET_BOOL, "-verifyLinearScan",       UNUSED, false)

//=== scheduler options ===
DEF_VISA_OPTION(vISA_LocalScheduling,       ET_BOOL, "-noschedule",      UNUSED, true)
DEF_VISA_OPTION(vISA_preRA_Schedule,        ET_BOOL, "-nopresched",      UNUSED, true)
DEF_VISA_OPTION(vISA_preRA_ScheduleForce,   ET_BOOL, "-presched",        UNUSED, false)
DEF_VISA_OPTION(vISA_preRA_ScheduleCtrl,      ET_INT32, "-presched-ctrl",      "USAGE: -presched-ctrl <ctrl>\n", 4)
DEF_VISA_OPTION(vISA_preRA_ScheduleRPThreshold, ET_INT32, "-presched-rp",      "USAGE: -presched-rp <threshold>\n", 0)
DEF_VISA_OPTION(vISA_DumpSchedule,          ET_BOOL, "-dumpSchedule",    UNUSED, false)
DEF_VISA_OPTION(vISA_DumpDagDot,            ET_BOOL, "-dumpDagDot",      UNUSED, false)
DEF_VISA_OPTION(vISA_EnableNoDD,            ET_BOOL, "-enable-noDD",     UNUSED, false)
DEF_VISA_OPTION(vISA_DebugNoDD,             ET_BOOL, "-debug-noDD",      UNUSED, false)
DEF_VISA_OPTION(vISA_NoDDLookBack,          ET_INT32, "-noDD-lookback",  "USAGE: -noDD-lookback <NUM>\n", 3)
DEF_VISA_OPTION(vISA_EnableNoSrcDep,        ET_BOOL, "-enable-noSrcDep", UNUSED, false)
DEF_VISA_OPTION(vISA_EnableNoSrcDepScen1,   ET_BOOL, "-disable-noSrcDep-scen1", UNUSED, true)
DEF_VISA_OPTION(vISA_EnableNoSrcDepScen2,   ET_BOOL, "-disable-noSrcDep-scen2", UNUSED, true)
DEF_VISA_OPTION(vISA_DumpNoSrcDep,          ET_BOOL, "-dump-noSrcDep",   UNUSED, false)
DEF_VISA_OPTION(vISA_stopNoSrcDepSetAt,     ET_INT32, "-stop-noSrcDep-at", "Usage: -stop-noSrcDep-at <NUMBER>\n", UINT_MAX)
DEF_VISA_OPTION(vISA_FuseTypedWrites,       ET_BOOL, "-nofuse-typedWrites", UNUSED, false)
DEF_VISA_OPTION(vISA_ReorderDPSendToDifferentBti, ET_BOOL, "-nodpsendreorder", UNUSED, true)
DEF_VISA_OPTION(vISA_WAWSubregHazardAvoidance,    ET_BOOL, "-noWAWSubregHazardAvoidance", UNUSED, true)
DEF_VISA_OPTION(vISA_useMultiThreadedLatencies,   ET_BOOL, "-dontUseMultiThreadedLatencies", UNUSED, true)
DEF_VISA_OPTION(vISA_SchedulerWindowSize,         ET_INT32, "-schedulerwindow", "USAGE: -schedulerwindow <window-size>\n", 4096)
DEF_VISA_OPTION(vISA_HWThreadNumberPerEU, ET_INT32, "-HWThreadNumberPerEU", "USAGE: -HWThreadNumberPerEU <num>\n",  0)
DEF_VISA_OPTION(vISA_ForceHWThreadNumberPerEU, ET_INT32, "-forceHWThreadNumberPerEU", "USAGE: -forceHWThreadNumberPerEU <num>\n",  0)
DEF_VISA_OPTION(vISA_NoAtomicSend, ET_BOOL, "-noAtomicSend", UNUSED, false)
DEF_VISA_OPTION(vISA_ReadSuppressionDepth, ET_INT32, "-readSuppressionDepth", UNUSED, 0)
DEF_VISA_OPTION(vISA_ScheduleForReadSuppression, ET_BOOL, "-scheduleForReadSuppression", UNUSED, false)
DEF_VISA_OPTION(vISA_LocalSchedulingStartBB,   ET_INT32, "-scheduleStartBB", UNUSED, 0)
DEF_VISA_OPTION(vISA_LocalSchedulingEndBB,     ET_INT32, "-scheduleEndBB", UNUSED, UINT_MAX)

//=== SWSB options ===
DEF_VISA_OPTION(vISA_USEL3HIT,      ET_BOOL,  "-SBIDL3Hit",    UNUSED, false)
DEF_VISA_OPTION(vISA_EnableIGASWSB,      ET_BOOL,  "-IGASWSB",    UNUSED, false)
DEF_VISA_OPTION(vISA_SWSBDepReduction,    ET_BOOL,  "-SWSBDepReduction",  UNUSED, false)
DEF_VISA_OPTION(vISA_forceDebugSWSB,      ET_BOOL,  "-forceDebugSWSB",    UNUSED, false)
DEF_VISA_OPTION(vISA_SWSBInstStall,       ET_INT32, "-SWSBInstStall",     UNUSED, 0)
DEF_VISA_OPTION(vISA_SWSBInstStallEnd,    ET_INT32, "-SWSBInstStallEnd",  UNUSED, 0)
DEF_VISA_OPTION(vISA_SWSBTokenBarrier,    ET_INT32, "-SWSBTokenBarrier",  UNUSED, 0)
DEF_VISA_OPTION(vISA_EnableSwitch,      ET_BOOL,  "-enableSwitch",    UNUSED, false)
DEF_VISA_OPTION(vISA_EnableISBIDBUNDLE,      ET_BOOL,  "-SBIDBundle",    UNUSED, false)
DEF_VISA_OPTION(vISA_EnableGroupScheduleForBC,      ET_BOOL,  "-groupScheduleForBC",    UNUSED, true)
DEF_VISA_OPTION(vISA_SWSBTokenNum,        ET_INT32, "-SWSBTokenNum",      "USAGE: -SWSBTokenNum <tokenNum>\n", 0)
DEF_VISA_OPTION(vISA_EnableSendTokenReduction,      ET_BOOL,  "-SendTokenReduction",    UNUSED, false)
DEF_VISA_OPTION(vISA_GlobalTokenAllocation,      ET_BOOL,  "-globalTokenAllocation",    UNUSED, false)
DEF_VISA_OPTION(vISA_QuickTokenAllocation,      ET_BOOL,  "-quickTokenAllocation",    UNUSED, false)
DEF_VISA_OPTION(vISA_DistPropTokenAllocation,      ET_BOOL,  "-distPropTokenAllocation",    UNUSED, false)
DEF_VISA_OPTION(vISA_SWSBStitch,      ET_BOOL,  "-SWSBStitch",    UNUSED, false)
DEF_VISA_OPTION(vISA_SBIDDepLoc,      ET_BOOL,  "-SBIDDepLoc",    UNUSED, false)
DEF_VISA_OPTION(vISA_DumpSBID,      ET_BOOL,  "-dumpSBID",    UNUSED, false)

DEF_VISA_OPTION(vISA_EnableALUThreePipes,      ET_BOOL,  (IGC_MANGLE("-threeALUPipes")),    UNUSED, true)
DEF_VISA_OPTION(vISA_EnableDPASTokenReduction,      ET_BOOL,  (IGC_MANGLE("-DPASTokenReduction")),    UNUSED, false)
DEF_VISA_OPTION(vISA_EnableDPASBundleConflictReduction,      ET_BOOL,  (IGC_MANGLE("-DPASBundleReduction")),    UNUSED, true)
DEF_VISA_OPTION(vISA_NoDPASMacro,      ET_BOOL,  (IGC_MANGLE("-noDPASMacro")),    UNUSED, false)
DEF_VISA_OPTION(vISA_forceDPASMacro,      ET_BOOL,  (IGC_MANGLE("-forceDPASMacro")),    UNUSED, false)
DEF_VISA_OPTION(vISA_TrueDepOnly,      ET_BOOL,  (IGC_MANGLE("-trueDepOnly")),    UNUSED, false)
DEF_VISA_OPTION(vISA_SplitMov64,            ET_INT32,"-SplitMov64",  "USAGE: -SplitMov64 (0|1|2)\n", 0)
DEF_VISA_OPTION(vISA_UseOldSubRoutineAugIntf,    ET_BOOL, (IGC_MANGLE("-useOldSubRoutineAugIntf")),     UNUSED, false)
DEF_VISA_OPTION(vISA_FastCompileRA,    ET_BOOL, (IGC_MANGLE("-fastCompileRA")),     UNUSED, false)
DEF_VISA_OPTION(vISA_HybridRAWithSpill,    ET_BOOL, (IGC_MANGLE("-hybridRAWithSpill")),     UNUSED, false)

//=== binary emission options ===
DEF_VISA_OPTION(vISA_Compaction,          ET_BOOL,  "-nocompaction",    UNUSED, true)
DEF_VISA_OPTION(vISA_BXMLEncoder,         ET_BOOL,  "-nobxmlencoder",   UNUSED, true)
DEF_VISA_OPTION(vISA_IGAEncoder,          ET_BOOL,  "-IGAEncoder",      UNUSED, false)

//=== asm/isaasm/isa emission options ===
DEF_VISA_OPTION(vISA_outputToFile,        ET_BOOL,  "-output",          UNUSED, false)
DEF_VISA_OPTION(vISA_SymbolReg,           ET_BOOL,  "-symbolreg",       UNUSED, false)
DEF_VISA_OPTION(vISA_PrintASMCount,       ET_BOOL,  "-printasmcount",   UNUSED, false)
DEF_VISA_OPTION(vISA_GenerateBinary,      ET_BOOL,  "-binary",          UNUSED, false)
DEF_VISA_OPTION(vISA_GenerateISAASM,      ET_BOOL,  "-dumpcommonisa",   UNUSED, false)
DEF_VISA_OPTION(vISA_DumpIsaVarNames,     ET_BOOL,  "-dumpisavarnames", UNUSED, true)
DEF_VISA_OPTION(vISA_GenIsaAsmList,       ET_BOOL,  "-genIsaasmList",   UNUSED, false)
DEF_VISA_OPTION(vISA_UniqueLabels,        ET_BOOL,  NULLSTR,            UNUSED, false)
//   specifies a file containing isaasm paths/names to parse
DEF_VISA_OPTION(vISA_IsaasmNamesFileUsed, ET_BOOL,  NULLSTR,            UNUSED, false)
DEF_VISA_OPTION(vISA_DumpvISA,            ET_BOOL,  "-dumpvisa",        UNUSED, false)
DEF_VISA_OPTION(vISA_StripComments,       ET_BOOL, "-stripcomments",      UNUSED, false)
DEF_VISA_OPTION(vISA_dumpNewSyntax,       ET_BOOL, "-disableIGASyntax",   UNUSED, true)
DEF_VISA_OPTION(vISA_NumGenBinariesWillBePatched, ET_INT32, "-numGenBinariesWillBePatched", "USAGE: missing number of gen binaries that will be patched.\n", 0)
DEF_VISA_OPTION(vISA_ISAASMNamesFile,   ET_CSTR, "-isaasmNamesOutputFile", "USAGE: File Name with isaasm paths.\n", NULL)
DEF_VISA_OPTION(vISA_GetvISABinaryName, ET_CSTR, "-outputCisaBinaryName",  UNUSED,                                  NULL)
DEF_VISA_OPTION(vISA_OutputvISABinaryName, ET_BOOL, NULLSTR,               UNUSED, false)
DEF_VISA_OPTION(vISA_LabelStr,          ET_CSTR,  "-uniqueLabels",         "Label String is not provided for the -uniqueLabels option.", NULL)
DEF_VISA_OPTION(VISA_AsmFileName,       ET_CSTR, "-asmOutput",             "USAGE: -asmOutput <FILE>\n",          NULL)
DEF_VISA_OPTION(vISA_DecodeDbg,         ET_CSTR, "-decodedbg",             "USAGE: -decodedbg <dbg filename>\n",    NULL)
DEF_VISA_OPTION(vISA_encoderFile,       ET_CSTR, "-encoderStatisticsFile", "USAGE: -encoderStatisticsFile <reloc file>\n", "encoderStatistics.csv")
DEF_VISA_OPTION(vISA_CISAbinary,        ET_CSTR, "-CISAbinary",            "USAGE: File Name with isaasm paths. ",  NULL)
DEF_VISA_OPTION(vISA_DumpRegInfo, ET_BOOL, "-dumpRegInfo",            UNUSED, false)

//=== misc options ===
DEF_VISA_OPTION(vISA_PlatformIsSet,       ET_BOOL,  NULLSTR,              UNUSED, false)
DEF_VISA_OPTION(vISA_NoVerifyvISA,        ET_BOOL,  "-noverifyCISA",      UNUSED, false)
DEF_VISA_OPTION(vISA_InitPayload,         ET_BOOL,  "-initializePayload", UNUSED, false)
DEF_VISA_OPTION(vISA_isParseMode,         ET_BOOL,  NULLSTR,              UNUSED, false)
//   rerun RA post scheduling for gtpin
DEF_VISA_OPTION(vISA_ReRAPostSchedule,    ET_BOOL,  "-rerapostschedule",  UNUSED, false)
DEF_VISA_OPTION(vISA_GTPinReRA,           ET_BOOL, "-GTPinReRA",          UNUSED, false)
DEF_VISA_OPTION(vISA_GetFreeGRFInfo,      ET_BOOL,  "-getfreegrfinfo",    UNUSED, false)
DEF_VISA_OPTION(vISA_GTPinScratchAreaSize,ET_INT32, "-GTPinScratchAreaSize", UNUSED, 0)
DEF_VISA_OPTION(vISA_skipFenceCommit,     ET_BOOL,  "-skipFenceCommit", UNUSED, false)

//=== HW Workarounds ===
DEF_VISA_OPTION(vISA_clearScratchWritesBeforeEOT,   ET_BOOL,  NULLSTR, UNUSED, false)
DEF_VISA_OPTION(vISA_clearHDCWritesBeforeEOT,       ET_BOOL,  NULLSTR, UNUSED, false)
DEF_VISA_OPTION(vISA_setA0toTdrForSendc,            ET_BOOL,  "-setA0toTdrForSendc", UNUSED, false)
DEF_VISA_OPTION(vISA_addFFIDProlog,                 ET_BOOL,  "-noFFIDProlog",       UNUSED, true)
DEF_VISA_OPTION(vISA_setFFID,                       ET_INT32, "-setFFID", "USAGE: -setFFID <ffid>\n", FFID_INVALID)
DEF_VISA_OPTION(vISA_replaceIndirectCallWithJmpi,   ET_BOOL,  "-replaceIndirectCallWithJmpi",  UNUSED, false)
DEF_VISA_OPTION(vISA_noMaskWA,                      ET_INT32, "-noMaskWA",  "USAGE: -noMaskWA <[0:1]=0 (off)|1|2|3(3=2), [2:2]=0|1>\n", 0)
DEF_VISA_OPTION(vISA_forceNoMaskWA,                 ET_BOOL,  "-forceNoMaskWA",  UNUSED, false)
DEF_VISA_OPTION(vISA_DstSrcOverlapWA,               ET_BOOL,  "-dstSrcOverlapWA",  UNUSED, true)
DEF_VISA_OPTION(vISA_noSendSrcDstOverlap,           ET_BOOL,  "-noSendSrcDstOverlap", UNUSED, false)
DEF_VISA_OPTION(vISA_cloneSampleInst,               ET_BOOL,  "-cloneSampleInst", UNUSED, false)
DEF_VISA_OPTION(vISA_cloneEvaluateSampleInst,       ET_BOOL,  "-cloneEvaluateSampleInst", UNUSED, false)
DEF_VISA_OPTION(vISA_expandMulPostSchedule,         ET_BOOL,  "-expandMulPostSchedule", UNUSED, true)
DEF_VISA_OPTION(vISA_expandMadwPostSchedule,        ET_BOOL,  "-expandMadwPostSchedule", UNUSED, true)
DEF_VISA_OPTION(vISA_disableRegDistDep,         ET_BOOL,  "-disableRegDistDep", UNUSED, false)

//=== HW debugging options ===
DEF_VISA_OPTION(vISA_GenerateDebugInfo,   ET_BOOL,  "-generateDebugInfo", UNUSED, false)
DEF_VISA_OPTION(vISA_setStartBreakPoint,  ET_BOOL,  "-setstartbp",        UNUSED, false)
DEF_VISA_OPTION(vISA_InsertHashMovs,      ET_BOOL,  NULLSTR,              UNUSED, false)
DEF_VISA_OPTION(vISA_InsertDummyMovForHWRSWA,      ET_BOOL,  "-insertRSDummyMov",              UNUSED, false)
DEF_VISA_OPTION(vISA_InsertDummyMovForDPASRSWA,      ET_BOOL,  "-insertDPASRSDummyMov",              UNUSED, true)
DEF_VISA_OPTION(vISA_registerHWRSWA,      ET_INT32,  "-dummyRegisterHWRSWA",              UNUSED, 0)
DEF_VISA_OPTION(vISA_InsertDummyCompactInst, ET_BOOL, "-insertDummyCompactInst", UNUSED, false)
DEF_VISA_OPTION(vISA_AsmFileNameOverridden,  ET_BOOL,  NULLSTR,        UNUSED, false)
DEF_VISA_OPTION(vISA_HashVal,             ET_2xINT32, "-hashmovs", "USAGE: -hashmovs hi32 lo32\n", 0)
DEF_VISA_OPTION(vISA_HashVal1,            ET_2xINT32, "-hashmovs1", "USAGE: -hashmovs1 hi32 lo32\n", 0)
DEF_VISA_OPTION(vISA_AddKernelID,         ET_BOOL,  "-addKernelID", UNUSED, false)
DEF_VISA_OPTION(vISA_dumpPayload,         ET_BOOL, "-dumpPayload",        UNUSED, false)

DEF_VISA_OPTION(vISA_dumpToCurrentDir,    ET_BOOL, "-dumpToCurrentDir",   UNUSED, false)
DEF_VISA_OPTION(vISA_dumpTimer,           ET_BOOL, "-timestats",          UNUSED, false)
DEF_VISA_OPTION(vISA_EnableCompilerStats,   ET_BOOL, "-compilerStats",      UNUSED, false)

DEF_VISA_OPTION(vISA_3DOption,            ET_BOOL, "-3d",                 UNUSED, false)
DEF_VISA_OPTION(vISA_Stepping,          ET_CSTR, "-stepping",              "USAGE: missing stepping string. ",      NULL)
DEF_VISA_OPTION(vISA_Platform,          ET_CSTR, "-platform",              "USAGE: missing platform string. ",      NULL)
DEF_VISA_OPTION(vISA_HasEarlyGRFRead,     ET_BOOL, "-earlyGRFRead",       UNUSED, false)