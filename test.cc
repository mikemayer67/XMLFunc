#include <iostream>
#include <fstream>
#include <sstream>
#include <stdlib.h>

#include "XMLFunc.h"

using namespace std;

int main(int argc,char **argv)
{
  XMLFunc q1("quad_1.xml");

  ifstream q2s("quad_2.xml");
  if( q2s.good() == false ) 
  {
    cerr << endl << "Failed to open quad_2.xml" << endl << endl;
    exit(1);
  }
  stringstream q2xml;
  q2xml << q2s.rdbuf();

  XMLFunc q2(q2xml.str());

  return 0;
}
