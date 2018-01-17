/*
 *
 *
 *
 *  Copyright (C) 2010 Wyplay
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
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

