/*
 * File: translations.h
 * Version: v1.0.2
 * Owner: David William Bull
 * Created: 2025-02-19
 * Last Modified: 2026-08-18
 * Description: Language selection: the six string-table pointers every message is read through, and the registry the 'L' option walks.
 * To Do: 1) Add a second language -- a header and a LANGUAGES row since ISSUES.MD D2 -- which is the one full test of that entry's fix
 * Dependencies: en-GB.h
 * ISA: Scalar
 * Thread-safety: N/A
 * Reviewers: David William Bull
 * License: MIT  Copyright: David William Bull
 */
#pragma once

#include "en-GB.h"

// A language's fixed-width label tables are pointed at by array type rather than by cwchptrcptr, and that is
// the whole of how their width stays a contract instead of a convention. The banner pads the unit and sync
// lists in whole slots and the results table gives the ProcUnit column a fixed cell, so a label of another
// width shifts every column after it -- a defect no run reports and no reader can attribute to the language
// file it came from. An array of pointers would accept a label of any length; "pointer to an array of N"
// refuses one that does not fit, at the line of the language header that wrote it
typedef cwchar (*cwchar4ptr)[4]; // Table of labels of up to 3 characters: wstrUnitsCPU, wstrSyncCPU
typedef cwchar (*cwchar8ptr)[8]; // Table of labels of up to 7 characters: wstrPass

// The six tables the whole program reads its text through, and which the 'L' option repoints at another
// language's. Declared here and defined once in CPU.cpp, with English as the default: a definition in a header
// is a duplicate symbol in every translation unit past the first, and a per-unit copy of a pointer the 'L'
// option writes is one the other units would never see written (ISSUES.MD H9).
// The last three were 'inline' tables in CPU.h, reachable by no language at all: the processing-unit and
// synchronisation labels of every banner, and the .Pass./!Fail! verdict of every results row -- the one word
// a failing CPU always prints (ISSUES.MD D1). A language that adds an entry to any of them adds it to every
// other language's table too, because each is indexed by a bit position or a verdict rather than searched
extern cwchptr     wstrInstructions;
extern cwchptrcptr wstrMessage;
extern cwchptrcptr wstrInterface;
extern cwchar4ptr  wstrUnitsCPU;
extern cwchar4ptr  wstrSyncCPU;
extern cwchar8ptr  wstrPass;

//--- Language registry ---//
// One row per language this build carries: the code the 'L' option matches a candidate against, and the six
// tables a match repoints together. A row carries all six so that a language cannot set some and leave the
// rest -- the two-language report the block above warns of -- and the 'L' case walks this table rather than
// naming any language itself, so adding one is a header and a row here, not an edit to CPU.cpp (ISSUES.MD
// D2). A code is at most five characters, the capacity of the wstrLang the active selection is recorded in
// and of the wstrLangArg local the candidate is parsed into -- widen both together with any longer code
// added here, or the clamped candidate can never match it. en-US is an alias: both rows name the one
// English. A new language header must be saved as UTF-8 -- the project compiles with /utf-8, and without it
// the wchar values of the header's L"..." literals would follow the build machine's ANSI code page
struct LANGUAGE_TABLES {
   cwchptrc    wstrCode;
   cwchptrc    wstrInstructions;
   cwchptrcptr wstrMessage;
   cwchptrcptr wstrInterface;
   cwchar4ptr  wstrUnitsCPU;
   cwchar4ptr  wstrSyncCPU;
   cwchar8ptr  wstrPass;
}; typedef const LANGUAGE_TABLES cLANGUAGE_TABLES;

// UPPER_SNAKE because it is a table at namespace scope, which GCS r12 spells that way, and 'inline' because
// it is immutable: one entity shared by every translation unit, exactly as wstrKernelName is. Every table a
// row points at is itself inline, which is what an inline definition here requires of them (ISSUES.MD H9)
inline cLANGUAGE_TABLES LANGUAGES[] = {
   { L"en-GB", wstrInstructions_English, wstrMessage_English, wstrInterface_English, wstrUnitsCPU_English, wstrSyncCPU_English, wstrPass_English },
   { L"en-US", wstrInstructions_English, wstrMessage_English, wstrInterface_English, wstrUnitsCPU_English, wstrSyncCPU_English, wstrPass_English }
};
//--- Language registry ---//
