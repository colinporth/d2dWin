//{{{  includes
#include "windows.h"

#include "html/ParserDom.h"
#include "html/utils.h"
#include "html/wincstring.h"

#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <cstdio>

using namespace std;
using namespace htmlcxx;
//}}}

int main(int argc, char **argv) {

  try {
    ifstream file (argv[1]);

    string html;
    while (true) {
      char buf[BUFSIZ];
      file.read (buf, BUFSIZ);
      if (file.gcount() == 0)
        break;
      html.append (buf, file.gcount());
      }
    file.close();

    HTML::ParserDom parser;
    parser.parse (html);
    tree<HTML::Node> tr = parser.getTree();
    cout << tr << endl;
    }
  catch (exception &e) {
    cerr << "Exception " << e.what() << " caught" << endl;
    exit(1);
    }
  catch (...) {
    cerr << "Unknow exception caught " << endl;
    }

  Sleep (100000);
  return 0;
  }
