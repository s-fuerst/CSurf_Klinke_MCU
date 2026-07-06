/**
 * Copyright (C) 2009-2026 Steffen Fuerst
 * Distributed under the GNU GPL v3. For full terms see the file gplv3.txt.
 */

#ifdef _WIN32
#include <Windows.h>
#endif
#include <iostream>
#include <sstream>

#ifndef NDEBUG
#define DBOUT(s)                                                               \
  {                                                                            \
    std::ostringstream os_;                                                    \
    os_ << s;                                                                  \
    OutputDebugString(os_.str().c_str());                                      \
  }
#ifndef X64
#define ASSERT_M(isOK, message)                                                \
  if (!(isOK)) {                                                               \
    (void)printf("ERROR!! Assert �%s� failed on line %d "                    \
                 "in file �%s�\n%s\n",                                       \
                 #isOK, __LINE__, __FILE__, #message);                         \
    __asm { int 3}                                                              \
  }

#define ASSERT(isOK)                                                           \
  if (!(isOK)) {                                                               \
    (void)printf("ERROR!! Assert �%s� failed on line %d "                    \
                 "in file �%s�\n",                                           \
                 #isOK, __LINE__, __FILE__);                                   \
    __asm { int 3}                                                              \
  }
#else
#define ASSERT_M(unused, message)                                              \
  do {                                                                         \
  } while (false)
#define ASSERT(unused)                                                         \
  do {                                                                         \
  } while (false)
#endif
#else
#define DBOUT(s)                                                               \
  do {                                                                         \
  } while (false)
#define ASSERT_M(unused, message)                                              \
  do {                                                                         \
  } while (false)
#define ASSERT(unused)                                                         \
  do {                                                                         \
  } while (false)
#endif
