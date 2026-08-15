/************************************************************
 * File: translations.h                 Created: 2025/02/19 *
 *                                    Last mod.: 2026/08/15 *
 *                                                          *
 * Desc:                                                    *
 *                                                          *
 * MIT license             Copyright (c) David William Bull *
 ************************************************************/
#pragma once

#include "en-GB.h"

// The three tables the whole program reads its text through, and which the 'L' option repoints at another
// language's. Declared here and defined once in CPU.cpp, with English as the default: a definition in a header
// is a duplicate symbol in every translation unit past the first, and a per-unit copy of a pointer the 'L'
// option writes is one the other units would never see written (ISSUES.MD H9)
extern cwchptr     wstrInstructions;
extern cwchptrcptr wstrMessage;
extern cwchptrcptr wstrInterface;
