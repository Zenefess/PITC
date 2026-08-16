/*
 * File: translations.h
 * Version: v1.0.2
 * Owner: David William Bull
 * Created: 2025-02-19
 * Last Modified: 2026-08-16
 * Description: Language selection: declares the three string-table pointers every message, prompt and report is read through.
 * To Do: 1) Make selection data-driven, so a language is a header and a table entry rather than an edit to the 'L' case (ISSUES.MD K6)
 *        2) Set the console output code page before the first wide write, so a translation outside it does not print mojibake (K6)
 * Dependencies: en-GB.h
 * ISA: Scalar
 * Thread-safety: N/A
 * Reviewers: David William Bull
 * License: MIT  Copyright: David William Bull
 */
#pragma once

#include "en-GB.h"

// The three tables the whole program reads its text through, and which the 'L' option repoints at another
// language's. Declared here and defined once in CPU.cpp, with English as the default: a definition in a header
// is a duplicate symbol in every translation unit past the first, and a per-unit copy of a pointer the 'L'
// option writes is one the other units would never see written (ISSUES.MD H9)
extern cwchptr     wstrInstructions;
extern cwchptrcptr wstrMessage;
extern cwchptrcptr wstrInterface;
