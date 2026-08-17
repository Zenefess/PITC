/*
 * File: translations.h
 * Version: v1.0.2
 * Owner: David William Bull
 * Created: 2025-02-19
 * Last Modified: 2026-08-17
 * Description: Language selection: declares the six string-table pointers every message, prompt and report is read through.
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
