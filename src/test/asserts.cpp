/* Frobby: Software for monomial ideal computations.
   Copyright (C) 2009 Bjarke Hammersholt Roune (www.broune.com)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see http://www.gnu.org/licenses/.
*/
#include "stdinc.h"
#include "asserts.h"

#include <sstream>

AssertException::AssertException(const string& str):
  logic_error(str) {
}

AssertException::AssertException(const AssertException& e):
logic_error(e) {
}

void assertSucceeded(bool printDot) {
  if (printDot) {
    fputc('.', stdout);
    fflush(stdout);
  }
}

void assertFailed(const char* errorMsg,
                  const char* testName, const char* file, size_t line) {
  if (testName == 0)
    testName = "";

  stringstream msg;
  msg << "Unit test " << testName
      << " failed in file " << file
      << " on line " << line << ".\n"
      << errorMsg;
  if (!msg) {
    // This means msg has run out of memory, and so no message will be
    // printed. In this case it is better to indicate running out of
    // memory. As it happens, this also avoids the need for some
    // special cases for tests when being run as a test for recovery
    // from running out of memory. E.g. when precisely this thing
    // happens with stringstream just ignoring its input without an
    // exception causes tests to fail.
    throw bad_alloc();
  }
  throw AssertException(msg.str());
}

void assertFailed2(const char* errorMsg,
				   const char* testName, const char* file, size_t line,
				   const char* expression1, const char* expression1Value,
				   const char* expression2, const char* expression2Value) {
  stringstream msg;
  msg << errorMsg
	  << "The value of the expression\n  " << expression1
	  << "\nprints as\n " << expression1Value << '\n'
	  << "and the value of the expression\n  " << expression2
	  << "\nprints as\n " << expression2Value << '\n';
  assertFailed(msg.str().c_str(), testName, file, line);
}

void assertTrue(bool value, const char* valueString,
                const char* testName, const char* file, size_t line,
                bool printDot) {
  if (value) {
    assertSucceeded(printDot);
    return;
  }

  stringstream msg;
  msg << "Expected \n   " << valueString << "\nto be true, but it was not.\n";
  assertFailed(msg.str().c_str(), testName, file, line);
}

void assertTrue2Failed(const char* valueString,
					   const char* testName, const char* file, size_t line,
					   const char* expression1, const char* expression1Value,
					   const char* expression2, const char* expression2Value) {
  stringstream msg;
  msg << "Expected \n   " << valueString << "\nto be true, but it was not.\n";
  assertFailed2(msg.str().c_str(), testName, file, line,
				expression1, expression1Value,
				expression2, expression2Value);
}

void assertFalse(bool value, const char* valueString,
                 const char* testName, const char* file, size_t line,
                 bool printDot) {
  if (!value) {
    assertSucceeded(printDot);
    return;
  }

  stringstream msg;
  msg << "Expected \n   " << valueString << "\nto be false, but it was not.\n";
  assertFailed(msg.str().c_str(), testName, file, line);
}

void assertEqualFailed(const char* a, const char* b,
                       const char* aString, const char* bString,
                       const char* testName, const char* file, size_t line) {
  stringstream msg;
  msg << "Expected " << aString << " == " << bString << ",\n"
	  << "but operator== returned false. "
      << "The  left hand side prints as\n" << a << "\nwhile "
      << "the right hand side prints as\n" << b << ".\n";
  assertFailed(msg.str().c_str(), testName, file, line);
}

void assertNotEqualFailed(const char* a, const char* b,
						  const char* aString, const char* bString,
						  const char* testName, const char* file, size_t line) {
  stringstream msg;
  msg << "Expected " << aString << " != " << bString << ",\n"
	  << "but operator!= returned false. "
      << "The  left hand side prints as\n" << a << "\nwhile "
      << "the right hand side prints as\n" << b << ".\n";
  assertFailed(msg.str().c_str(), testName, file, line);
}
