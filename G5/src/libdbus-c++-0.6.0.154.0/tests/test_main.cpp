/*
** Copyright (C) 2010 Fargier Sylvain <fargier.sylvain@free.fr>
**
** This software is provided 'as-is', without any express or implied
** warranty.  In no event will the authors be held liable for any damages
** arising from the use of this software.
**
** Permission is granted to anyone to use this software for any purpose,
** including commercial applications, and to alter it and redistribute it
** freely, subject to the following restrictions:
**
** 1. The origin of this software must not be misrepresented; you must not
**    claim that you wrote the original software. If you use this software
**    in a product, an acknowledgment in the product documentation would be
**    appreciated but is not required.
** 2. Altered source versions must be plainly marked as such, and must not be
**    misrepresented as being the original software.
** 3. This notice may not be removed or altered from any source distribution.
**
** test_main.cc
**
**        Created on: Thu 29 Apr 2010 11:18:09 AM CEST
**            Author: Fargier Sylvain <fargier.sylvain@free.fr>
**
*/

#include <cppunit/CompilerOutputter.h>
#include <cppunit/XmlOutputter.h>
#include <cppunit/TextOutputter.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/TestResult.h>
#include <cppunit/TestResultCollector.h>
#include <cppunit/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>

#include <string>
#include <list>
#include <iostream>

#include "config.h"

#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#include <stdlib.h>

static std::ostream &printTest(std::ostream &out, const CppUnit::Test *test) {
  out << test->getName() << "\n";
  for (unsigned int i = 0; i < test->getChildTestCount(); ++i)
  {
    printTest(out, test->getChildTestAt(i));
  }
  return out << std::flush;
}

#endif

/* Test program */
int main(int argc, char** argv)
{
  CppUnit::TestResult testresult;
  CppUnit::TestResultCollector collectedresults;
  CppUnit::TestRunner testrunner;
  std::ofstream fb;
  std::list< std::string > test_list;

#ifdef HAVE_GETOPT_LONG
  int c;
  struct option long_options[] = {
    {"help", 0, 0, 'h'},
    {"list", 0, 0, 'l'},
    {0, 0, 0, 0}
  };

  while ((c = getopt_long(argc, argv, "hl", long_options, NULL)) != -1) {
    switch (c) {
      case 'h':
        std::cout << "Usage: " << argv[0] << " [test name]\n";
        std::cout << "\nOptions:\n" <<
          "  -h, --help           Show this help message and exit\n" <<
          "  -l, --list           List available tests and exit" <<
          std::endl;
        exit(EXIT_SUCCESS);
      case 'l':
      {
        CppUnit::Test *t = CppUnit::TestFactoryRegistry::getRegistry().makeTest();
        std::cout << "Available test suites:\n";
        printTest(std::cout, t);
        delete t;
        exit(EXIT_SUCCESS);
      }
    }
  }

  if (optind < argc) {
    while (optind < argc) {
      test_list.push_back(argv[optind++]);
    }
  }
#endif

  testresult.addListener(&collectedresults);
  testrunner.addTest(CppUnit::TestFactoryRegistry::getRegistry().makeTest());
  if (test_list.empty())
    testrunner.run(testresult);
  else {
    std::list<std::string>::const_iterator it;
    for (it = test_list.begin(); it != test_list.end(); ++it) {
      testrunner.run(testresult, *it);
    }
  }

  fb.open((std::string(argv[0]) + ".xml").c_str());
  CppUnit::XmlOutputter xml_outputter(&collectedresults, fb);
  xml_outputter.write();
  fb.close();

  fb.open((std::string(argv[0]) + ".cmp").c_str());
  CppUnit::CompilerOutputter comp_outputter(&collectedresults, fb);
  comp_outputter.write();
  fb.close();

  CppUnit::TextOutputter txt_outputter(&collectedresults, std::cout);
  txt_outputter.write();

  return collectedresults.wasSuccessful() ? 0 : 1;
}

