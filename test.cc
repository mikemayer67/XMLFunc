#include <iostream>
#include <fstream>
#include <sstream>
#include <stdlib.h>

#include "XMLFunc.h"

using namespace std;

int main(int argc,char **argv)
{
  try
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
    
    XMLFunc::Args args;
    args.add(1);
    args.add(-3.5);
    args.add(2);
    args.add(1234);
    double x = q1.eval(0,args);
    cout <<"q1(0) -> " << x << endl;
    x = q1.eval(1,args);
    cout <<"q1(1) -> " << x << endl;
    x = q1.eval("root2",args);
    cout <<"q1(root2) -> " << x << endl;
    x = q2.eval(args);
    cout <<"q2() -> " << x << endl;
  }
  catch( runtime_error &e )
  {
    cout << "Exception thrown::" << endl << e.what() << endl;
  }

  return 0;
}
