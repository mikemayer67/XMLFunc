#include <iostream>
#include "XMLFunc.h"

using namespace std;

int main(int argc,char **argv)
{
  XMLFunc::Number i(0x7fffffff);
  XMLFunc::Number d(456.789);

  int ii = int(i);
  int di = double(i);

  double id = int(d);
  double dd = double(d);
  
  cout << "ii: " << ii << endl;
  cout << "di: " << di << endl;
  cout << "id: " << id << endl;
  cout << "dd: " << dd << endl;

  return 0;
}
