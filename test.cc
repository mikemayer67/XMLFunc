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
    XMLFunc quad("quad.xml");

    ifstream uts("unit_tests.xml");
    if( uts.good() == false ) 
    {
      cerr << endl << "Failed to open unit_tests.xml" << endl << endl;
      exit(1);
    }
    stringstream utxml;
    utxml << uts.rdbuf();

    XMLFunc ut(utxml.str());
    
    XMLFunc::Args args;
    args.add(1);
    args.add(-3.5);
    args.add(2);
    args.add(1234);
    double x1 = quad.eval(0,args);
    double x2 = quad.eval("root2",args);

    cout << "roots of " << args.at(0) << "x^2 + " << args.at(1) << "x + " << args.at(2) << " = 0   =>  " << x1 << " and " << x2 << endl << endl;

    args.clear();
    args.add(1.23);
    
    XMLFunc::Number y;

    y = ut.eval("neg",args);
    cout << "neg(1.23) = " << y << (y.isInteger() ? " (int)" : "") << endl;
    y = ut.eval("abs",args);
    cout << "abs(1.23) = " << y << (y.isInteger() ? " (int)" : "") << endl;
    y = ut.eval("sin",args);
    cout << "sin(1.23) = " << y << (y.isInteger() ? " (int)" : "") << endl;
    y = ut.eval("cos",args);
    cout << "cos(1.23) = " << y << (y.isInteger() ? " (int)" : "") << endl;
    y = ut.eval("tan",args);
    cout << "tan(1.23) = " << y << (y.isInteger() ? " (int)" : "") << endl;
    y = ut.eval("asin",args);
    cout << "asin(1.23) = " << y << (y.isInteger() ? " (int)" : "") << endl;
    y = ut.eval("acos",args);
    cout << "acos(1.23) = " << y << (y.isInteger() ? " (int)" : "") << endl;
    y = ut.eval("atan",args);
    cout << "atan(1.23) = " << y << (y.isInteger() ? " (int)" : "") << endl;
    y = ut.eval("deg",args);
    cout << "deg(1.23) = " << y << (y.isInteger() ? " (int)" : "") << endl;
    y = ut.eval("rad",args);
    cout << "rad(1.23) = " << y << (y.isInteger() ? " (int)" : "") << endl;
    y = ut.eval("sqrt",args);
    cout << "sqrt(1.23) = " << y << (y.isInteger() ? " (int)" : "") << endl;
    y = ut.eval("exp",args);
    cout << "exp(1.23) = " << y << (y.isInteger() ? " (int)" : "") << endl;
    y = ut.eval("ln",args);
    cout << "ln(1.23) = " << y << (y.isInteger() ? " (int)" : "") << endl;
    y = ut.eval("log10",args);
    cout << "log10(1.23) = " << y << (y.isInteger() ? " (int)" : "") << endl;
    y = ut.eval("log2",args);
    cout << "log2(1.23) = " << y << (y.isInteger() ? " (int)" : "") << endl;

    cout << endl;

    args.clear();
    args.add(36);

    y = ut.eval("neg",args);
    cout << "neg(36) = " << y << (y.isInteger() ? " (int)" : "") << endl;
    y = ut.eval("abs",args);
    cout << "abs(36) = " << y << (y.isInteger() ? " (int)" : "") << endl;
    y = ut.eval("sin",args);
    cout << "sin(36) = " << y << (y.isInteger() ? " (int)" : "") << endl;
    y = ut.eval("cos",args);
    cout << "cos(36) = " << y << (y.isInteger() ? " (int)" : "") << endl;
    y = ut.eval("tan",args);
    cout << "tan(36) = " << y << (y.isInteger() ? " (int)" : "") << endl;
    y = ut.eval("asin",args);
    cout << "asin(36) = " << y << (y.isInteger() ? " (int)" : "") << endl;
    y = ut.eval("acos",args);
    cout << "acos(36) = " << y << (y.isInteger() ? " (int)" : "") << endl;
    y = ut.eval("atan",args);
    cout << "atan(36) = " << y << (y.isInteger() ? " (int)" : "") << endl;
    y = ut.eval("deg",args);
    cout << "deg(36) = " << y << (y.isInteger() ? " (int)" : "") << endl;
    y = ut.eval("rad",args);
    cout << "rad(36) = " << y << (y.isInteger() ? " (int)" : "") << endl;
    y = ut.eval("sqrt",args);
    cout << "sqrt(36) = " << y << (y.isInteger() ? " (int)" : "") << endl;
    y = ut.eval("exp",args);
    cout << "exp(36) = " << y << (y.isInteger() ? " (int)" : "") << endl;
    y = ut.eval("ln",args);
    cout << "ln(36) = " << y << (y.isInteger() ? " (int)" : "") << endl;
    y = ut.eval("log10",args);
    cout << "log10(36) = " << y << (y.isInteger() ? " (int)" : "") << endl;
    y = ut.eval("log2",args);
    cout << "log2(36) = " << y << (y.isInteger() ? " (int)" : "") << endl;

    

  }
  catch( runtime_error &e )
  {
    cout << "Exception thrown::" << endl << e.what() << endl;
  }

  return 0;
}
