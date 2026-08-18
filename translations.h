/*
 * File: translations.h
 * Version: v1.0.4
 * Owner: David William Bull
 * Created: 2025-02-19
 * Last Modified: 2026-08-18
 * Description: Language selection: the six string-table pointers every message is read through, and the registry the 'L' option walks.
 * To Do: 1) Add more languages
 * Dependencies: en-GB.h, fr-FR.h, zh-CN.h
 * ISA: Scalar
 * Thread-safety: N/A
 * Reviewers: David William Bull
 * License: MIT  Copyright: David William Bull
 */
#pragma once

#include "en-GB.h"
#include "fr-FR.h"
#include "zh-CN.h"

// A language's fixed-width label tables are pointed at by array type rather than by cwchptrcptr, and that is
// the whole of how their width stays a contract instead of a convention. The banner pads the unit and sync
// lists in whole slots and the results table gives the ProcUnit column a fixed cell, so a label of another
// width shifts every column after it -- a defect no run reports and no reader can attribute to the language
// file it came from. An array of pointers would accept a label of any length; "pointer to an array of N"
// refuses one that does not fit, at the line of the language header that wrote it
typedef cwchar (*cwchar4ptr)[4]; // Table of labels of up to 3 characters: wstrUnitsCPU, wstrSyncCPU
typedef cwchar (*cwchar8ptr)[8]; // Table of labels of up to 7 characters: wstrPass

// The six tables the whole program reads its text through, and which the 'L' option repoints at another
// language's. Declared here and defined once in CPU.cpp, with English as the default
extern cwchptr     wstrInstructions;
extern cwchptrcptr wstrMessage;
extern cwchptrcptr wstrInterface;
extern cwchar4ptr  wstrUnitsCPU;
extern cwchar4ptr  wstrSyncCPU;
extern cwchar8ptr  wstrPass;

//--- Language registry ---//
// One row per language this build carries: the code the 'L' option matches a candidate against, and the six
// tables a match repoints together. A row carries all six so that a language cannot set some and leave the rest
struct LANGUAGE_TABLES {
   cwchptrc    wstrCode;
   cwchptrc    wstrInstructions;
   cwchptrcptr wstrMessage;
   cwchptrcptr wstrInterface;
   cwchar4ptr  wstrUnitsCPU;
   cwchar4ptr  wstrSyncCPU;
   cwchar8ptr  wstrPass;
}; typedef const LANGUAGE_TABLES cLANGUAGE_TABLES;

inline cLANGUAGE_TABLES LANGUAGES[] = {
   { L"en-GB", wstrInstructions_English, wstrMessage_English, wstrInterface_English, wstrUnitsCPU_English, wstrSyncCPU_English, wstrPass_English },
   { L"en-US", wstrInstructions_English, wstrMessage_English, wstrInterface_English, wstrUnitsCPU_English, wstrSyncCPU_English, wstrPass_English },
   { L"fr-FR", wstrInstructions_French,  wstrMessage_French,  wstrInterface_French,  wstrUnitsCPU_French,  wstrSyncCPU_French,  wstrPass_French  },
   { L"zh-CN", wstrInstructions_Chinese, wstrMessage_Chinese, wstrInterface_Chinese, wstrUnitsCPU_Chinese, wstrSyncCPU_Chinese, wstrPass_Chinese }
};
//--- Language registry ---//
