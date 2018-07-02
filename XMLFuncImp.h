#ifndef _XMLFUNCIMP_h_
#define _XMLFUNCIMP_H_

using <vector>

////////////////////////////////////////////////////////////////////////////////
// This file contains all of the internal guts of the XMLFunc class.
//   As it is internal, no doxygen style documentation is provided
////////////////////////////////////////////////////////////////////////////////

namespace XMLFuncImp
{
  struct ArgInfo
  {
    ArgInfo(void) : isInteger(false) {}

    std::string name;
    bool        isInteger;
  };

  typedef std::vector<ArgInfo>       ArgList_t;
  typedef ArgList_t::const_iterator  ArgIter_t;

  class Node
  {
    public:

      Node(const std::string &xml, const ArgList_t &) {}

      virtual XMLFunc::Number eval(const std::vector<XMLFunc::Number> &args) = 0;

      static Node *build(const std::string &xml, const ArgList_t &);
  };
};

#endif // _XMLFUNCIMP_H_

