#include "XMLFunc.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <map>

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

using namespace std;

typedef map<string,string> Attributes_t;
typedef const string &cStrRef_t;

typedef XMLFunc::Number      Number_t;
typedef Number_t::Type_t     NumberType_t;
typedef XMLFunc::Node        Node_t;
typedef XMLFunc::Args_t      Args_t;

class ArgDefs;

// prototypes for support functions

void invalid_xml(cStrRef_t msg,cStrRef_t m2="",cStrRef_t m3="",cStrRef_t m4="",cStrRef_t m5="");

bool   is_bracket(cStrRef_t xml, size_t pos);
string load_xml(cStrRef_t src);
string cleanup_xml(cStrRef_t xml);

bool   read_element(cStrRef_t xml, string &key, Attributes_t &, string &body, string &tail);
size_t read_next_attr(cStrRef_t xml,size_t pos,string &attr_key,string &attr_value);

Node_t *build_node(cStrRef_t &xml, const ArgDefs &, string &tail); // modifies xml

bool   has_content(cStrRef_t s);

bool   string_to_double  (cStrRef_t s, double &dval,  string &extra);
bool   string_to_integer (cStrRef_t s, long   &ival,  string &extra);
bool   string_to_token   (cStrRef_t s, string &token, string &extra);
bool   get_token         (cStrRef_t s, string &token, string &tail);


////////////////////////////////////////////////////////////////////////////////
// Support classes
////////////////////////////////////////////////////////////////////////////////

class ArgDefs
{
  public:
    ArgDefs(void) {}

    void add(NumberType_t type, cStrRef_t name="")
    {
      if(name.empty()==false) xref_[name] = int(types_.size());
      types_.push_back(type);
    }

    int          size(void)  const { return int(types_.size()); }
    NumberType_t type(int i) const { return types_.at(i);  }

    int index(cStrRef_t name) const
    {
      NameXref_t::const_iterator i = xref_.find(name);
      if( i == xref_.end() ) invalid_xml("Bad argument name:",name);
      return i->second;
    }

    int lookup(cStrRef_t name) const
    {
      NameXref_t::const_iterator i = xref_.find(name);
      return (i==xref_.end() ? -1 : i->second );
    }

  private:
    typedef map<string,int> NameXref_t;
    vector<NumberType_t> types_;
    NameXref_t xref_;
};


////////////////////////////////////////////////////////////////////////////////
// XMLFunc::Node subclasses
////////////////////////////////////////////////////////////////////////////////

class ConstNode : public Node_t
{
  public:
    ConstNode(const Attributes_t &, cStrRef_t body, const ArgDefs &, NumberType_t);
    ConstNode(long v)   : value_(v) {}
    ConstNode(double v) : value_(v) {}
    Number_t eval(const Args_t &args) const { return value_; }
  private:
    Number_t value_;
};

class ArgNode : public Node_t
{
  public:
    ArgNode(const Attributes_t &, cStrRef_t body, const ArgDefs &);
    ArgNode(size_t i) : index_(i) {}
    Number_t eval(const Args_t &args) const { return args.at(index_); }
  private:
    size_t index_;
};


class UnaryNode : public Node_t
{
  public:
    UnaryNode(const Attributes_t &, cStrRef_t body, const ArgDefs &);

    ~UnaryNode() { if(op_!=NULL) delete op_; }

  protected:
    Node_t *op_;
};

class BinaryNode : public Node_t
{
  public:
    BinaryNode(const Attributes_t &, cStrRef_t body, const ArgDefs &);

    ~BinaryNode() 
    { 
      if(op1_!=NULL) delete op1_;
      if(op2_!=NULL) delete op2_;
    }

  protected:
    Node_t *op1_;
    Node_t *op2_;
};

class ListNode : public Node_t
{
  public:
    ListNode(const Attributes_t &, cStrRef_t body, const ArgDefs &);

    ~ListNode() 
    { 
      for(vector<Node_t *>::iterator i=ops_.begin(); i!=ops_.end(); ++i) delete *i;
    }

  protected:
    vector<Node_t *> ops_;
};

class NegNode : public UnaryNode
{
  public:
    NegNode(const Attributes_t &attr,cStrRef_t body, const ArgDefs &argDefs)
      : UnaryNode(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const
    {
      return op_->eval(args).negate();
    }
};

class SinNode : public UnaryNode
{
  public: 
    SinNode(const Attributes_t &attr,cStrRef_t body, const ArgDefs &argDefs)
      : UnaryNode(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const
    {
      return sin(double(op_->eval(args)));
    }
};

class CosNode : public UnaryNode
{
  public:
    CosNode(const Attributes_t &attr,cStrRef_t body, const ArgDefs &argDefs)
      : UnaryNode(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const
    {
      return cos(double(op_->eval(args)));
    }
};

class TanNode : public UnaryNode
{
  public:
    TanNode(const Attributes_t &attr,cStrRef_t body, const ArgDefs &argDefs)
      : UnaryNode(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const
    {
      return tan(double(op_->eval(args)));
    }
};

class AsinNode : public UnaryNode
{
  public:
    AsinNode(const Attributes_t &attr,cStrRef_t body, const ArgDefs &argDefs)
      : UnaryNode(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const
    {
      return asin(double(op_->eval(args)));
    }
};

class AcosNode : public UnaryNode
{
  public:
    AcosNode(const Attributes_t &attr,cStrRef_t body, const ArgDefs &argDefs)
      : UnaryNode(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const
    {
      return acos(double(op_->eval(args)));
    }
};

class AtanNode : public UnaryNode
{
  public:
    AtanNode(const Attributes_t &attr,cStrRef_t body, const ArgDefs &argDefs)
      : UnaryNode(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const
    {
      return atan(double(op_->eval(args)));
    }
};

class DegNode : public UnaryNode
{
  public: 
    DegNode(const Attributes_t &attr,cStrRef_t body, const ArgDefs &argDefs)
      : UnaryNode(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const
    {
      static double f = 45./atan(1.0);
      return Number_t( f * double(op_->eval(args)) );
    }
};

class RadNode : public UnaryNode
{
  public:
    RadNode(const Attributes_t &attr,cStrRef_t body, const ArgDefs &argDefs)
      : UnaryNode(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const
    {
      static double f = atan(1.0)/45.;
      return Number_t( f * double(op_->eval(args)) );
    }
};

class AbsNode : public UnaryNode
{
  public:
    AbsNode(const Attributes_t &attr,cStrRef_t body, const ArgDefs &argDefs)
      : UnaryNode(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const
    {
      return op_->eval(args).abs();
    }
};

class SqrtNode : public UnaryNode
{
  public:
    SqrtNode(const Attributes_t &attr,cStrRef_t body, const ArgDefs &argDefs)
      : UnaryNode(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const
    {
      return Number_t( sqrt(double(op_->eval(args))) );
    }
};

class ExpNode : public UnaryNode
{
  public:
    ExpNode(const Attributes_t &attr,cStrRef_t body, const ArgDefs &argDefs)
      : UnaryNode(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const
    {
      return Number_t( exp(double(op_->eval(args))) );
    }
};

class LnNode : public UnaryNode
{
  public: 
    LnNode(const Attributes_t &attr,cStrRef_t body, const ArgDefs &argDefs)
      : UnaryNode(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const
    {
      return Number_t( log(double(op_->eval(args))) );
    }
};

class LogNode : public UnaryNode
{
  public:
    LogNode(const Attributes_t &, cStrRef_t body, const ArgDefs &);
    Number_t eval(const Args_t &args) const
    {
      return Number_t( fac_ * log( double(op_->eval(args))) );
    }
  private:
    double fac_;
};

class SubNode : public BinaryNode
{
  public: 
    SubNode(const Attributes_t &attr,cStrRef_t body, const ArgDefs &argDefs)
      : BinaryNode(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const;
};

class DivNode : public BinaryNode
{
  public: 
    DivNode(const Attributes_t &attr,cStrRef_t body, const ArgDefs &argDefs)
      : BinaryNode(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const;
};

class PowNode : public BinaryNode
{
  public: 
    PowNode(const Attributes_t &attr,cStrRef_t body, const ArgDefs &argDefs)
      : BinaryNode(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const;
};

class ModNode : public BinaryNode
{
  public: 
    ModNode(const Attributes_t &attr,cStrRef_t body, const ArgDefs &argDefs)
      : BinaryNode(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const;
};

class Atan2Node : public BinaryNode
{
  public: 
    Atan2Node(const Attributes_t &attr,cStrRef_t body, const ArgDefs &argDefs)
      : BinaryNode(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const;
};

class AddNode : public ListNode
{
  public: 
    AddNode(const Attributes_t &attr,cStrRef_t body, const ArgDefs &argDefs)
      : ListNode(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const;
};

class MultNode : public ListNode
{
  public: 
    MultNode(const Attributes_t &attr,cStrRef_t body, const ArgDefs &argDefs)
      : ListNode(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const;
};



////////////////////////////////////////////////////////////////////////////////
// XMLFunc methods
////////////////////////////////////////////////////////////////////////////////

// XMLFunc constructor

XMLFunc::XMLFunc(cStrRef_t src) : root_(NULL), numArgs_(0)
{
  string xml;
  xml = load_xml(src);
  xml = cleanup_xml(xml);

  // Extract the arg definition list
  
  string       key;
  Attributes_t attr;
  string       args;
  string       func;

  bool rc = read_element(xml,key,attr,args,func);

  if(rc==false || key!="arglist")
    throw runtime_error("XML does not contain a valid <arglist>");
  if(args.empty())
    throw runtime_error("<arglist> contains no <arg> entries");

  ArgDefs argDefs;

  while(args.empty()==false)
  {
    string arg;
    string next_args;
    rc = read_element(args,key,attr,arg,next_args);

    if(rc==false || key!="arg")
      throw runtime_error("<arglist> must only contain <arg> elements in its body");

    NumberType_t type = Number_t::Double;

    Attributes_t::const_iterator ai = attr.find("type");
    if(ai != attr.end()) 
    {
      if     (ai->second == "double")  { type = Number_t::Double; }
      else if(ai->second == "float")   { type = Number_t::Double; }
      else if(ai->second == "real")    { type = Number_t::Double; }
      else if(ai->second == "integer") { type = Number_t::Integer; }
      else if(ai->second == "int")     { type = Number_t::Integer; }
      else invalid_xml("Unknown argument type", ai->second);
    }

    ai = attr.find("name");
    if(ai != attr.end()) { argDefs.add(type,ai->second); }
    else                 { argDefs.add(type); }

    args = next_args;
  }

  // Build the root evaluation node

  string tail;
  root_ = build_node(func,argDefs,tail);

  if( root_ == NULL )
    throw runtime_error("XML does not contain any valid value elements");

  if(has_content(tail))
    throw runtime_error("XML contains too many root value elements");
}

Number_t XMLFunc::eval(const Args_t &args) const
{
  if( args.size() < numArgs_ )
  {
    stringstream err;
    err << "Insufficient arguments passed to eval.  Need " << numArgs_ 
      << ". Only " << args.size() << " were provided";
    throw runtime_error(err.str());
  }

  return root_->eval(args);
}

Number_t XMLFunc::eval(const Number_t *argArray) const
{
  // No exception will be thrown if argArray doesn't contain
  //   enough elements... which may lead to unpredictable results
  //   including the real possiblity of a segmentation fault

  Args_t args;
  for(int i=0; i<numArgs_; ++i)
  {
    const Number_t n = argArray[i];
    args.push_back(n);
  }

  return root_->eval(args);
}

////////////////////////////////////////////////////////////////////////////////
// XMLFunc::Node subclass methods
////////////////////////////////////////////////////////////////////////////////

ConstNode::ConstNode(const Attributes_t &attr, cStrRef_t body, const ArgDefs &argDefs, NumberType_t type)
{
  string value_str;

  Attributes_t::const_iterator ai = attr.find("value");
  
  bool hasValue = ai != attr.end();
  bool hasBody  = has_content(body);

  if( hasValue && hasBody ) invalid_xml("Const node cannot contain both value attribute and a body");
  else if (hasValue)        value_str = ai->second; 
  else if (hasBody)         value_str = body;
  else                      invalid_xml("Const node must contain either value attribute or a body");

  string extra;
  if(type == Number_t::Double)
  {
    double dval(0.);
    if( ! string_to_double( value_str, dval, extra ) ) invalid_xml("Invalid double value:",value_str);
    value_ = Number_t(dval);
  }
  else
  {
    long ival(0);
    if( ! string_to_integer( value_str, ival, extra ) ) invalid_xml("Invalid integer value:",value_str);
    value_ = Number_t(ival);
  }
  if(extra.empty() == false) invalid_xml("Const node value contains extraneous data:",extra);
}


ArgNode::ArgNode(const Attributes_t &attr, cStrRef_t body, const ArgDefs &argDefs)
{
  Attributes_t::const_iterator index_iter = attr.find("index");
  Attributes_t::const_iterator name_iter = attr.find("name");

  bool hasIndex = index_iter != attr.end();
  bool hasName  = name_iter  != attr.end();
  bool hasBody  = has_content(body);

  int n = (hasIndex?1:0) + (hasName?1:0) + (hasBody?1:0);

  if(n > 1) invalid_xml("Arg node may only contain one of value attribute, index attribute, or body");
  if(n < 1) invalid_xml("Arg node must contain one of value attribute, index attribute, or body");

  long index;
  string name;
  string extra;
  if(hasBody) // determine if it a name or index
  {
    if(string_to_integer(body,index,extra))
    {
      hasIndex = true;
    }
    else if(string_to_token(body,name,extra))
    {
      hasName = true;
    }
    else if(extra.empty())
    {
      invalid_xml("Arg body must contain either argument index or name");
    }
    else
    {
      invalid_xml("Arg body contains extraneous data:",extra);
    }
  }
  else if(hasIndex)
  {
    if(string_to_integer(index_iter->second, index, extra) == false)
    {
      if(extra.empty())
        invalid_xml("Arg index attribute must contain an integer value");
      else
        invalid_xml("Arg index attribute contains extraneous data:",extra);
    }
  }
  else // hasName
  {
    if(string_to_token(index_iter->second, name, extra) == false)
    {
      if(extra.empty())
        invalid_xml("Arg name attribute must contain a non-empty string");
      else
        invalid_xml("Arg name attribute contains extraneous data:",extra);
    }
  }

  string extra;
  if(hasIndex)
  {
    index_ = (size_t)index;

    if(index_<0 )
    {
      stringstream err;
      err << "Argument index " << ival << " cannot be negative";
      invalid_xml(err.str());
    }
    if(index_>=argDefs.size()) 
    {
      stringstream err;
      err << "Argument index " << ival << " exceeds max value of" << argDefs.size()-1;
      invalid_xml(err.str());
    }
  }
  else
  {
    index_ = argDefs.index(name);
  }
}


UnaryNode::UnaryNode(const Attributes_t &attr, cStrRef_t body, const ArgDefs &argDefs)
{
  string arg_str;

  Attributes_t::const_iterator ai = attr.find("arg");

  bool hasAttr = ai != attr.end();
  bool hasBody  = has_content(body);

  if( hasAttr && hasBody ) invalid_xml("Unary node cannot contain both arg attribute and a body");
  else if (hasAttr)        arg_str = ai->second; 
  else if (hasBody)        arg_str = body;
  else                     invalid_xml("Unary node must contain either arg attribute or a body");

  string token;
  string tail;
  if( get_token(arg_str,token,tail) == false ) invalid_xml("Unary node arg is empty");

  long   ival;
  double dval;
  int    index;
  string extra;
  if( token[0] == '<' )
  {
    _op = build_node(arg_str,argDefs);
  }
  else if( string_to_integer( token, ival, extra ) )
  {
    _op = new ConstNode(ival);
  }
  else if( string_to_double( token, ival, extra ) )
  {
    _op = new ConstNode(dval);
  }
  else if( ( index = argDefs.lookup(token) ) >= 0 )
  {
    _op = new ArgNode(index);
  }
  
}

BinaryNode::BinaryNode(const Attributes_t &attr, cStrRef_t body, const ArgDefs &argDefs)
{
}

ListNode::ListNode(const Attributes_t &attr, cStrRef_t body, const ArgDefs &argDefs)
{
}

LogNode::LogNode(const Attributes_t &attr, cStrRef_t body, const ArgDefs &argDefs)
  : UnaryNode(attr,body,argDefs)
{
}

Number_t SubNode::eval(const Args_t &args) const
{
  Number_t v1 = op1_->eval(args);
  Number_t v2 = op2_->eval(args);

  return 
    ( v1.isInteger() && v2.isInteger() )
    ? Number_t( long(v1) - long(v2) )
    : Number_t( double(v1) - double(v2) ) ;
}

Number_t DivNode::eval(const Args_t &args) const
{
  Number_t v1 = op1_->eval(args);
  Number_t v2 = op2_->eval(args);

  return 
    ( v1.isInteger() && v2.isInteger() )
    ? Number_t( long(v1) / long(v2) )
    : Number_t( double(v1) / double(v2) ) ;
}

Number_t PowNode::eval(const Args_t &args) const
{
  Number_t v1 = op1_->eval(args);
  Number_t v2 = op2_->eval(args);

  return Number_t( pow( double(v1), double(v2) ) );
}

Number_t ModNode::eval(const Args_t &args) const
{
  Number_t v1 = op1_->eval(args);
  Number_t v2 = op2_->eval(args);

  return 
    ( v1.isInteger() && v2.isInteger() )
    ? Number_t( long(v1) % long(v2) )
  : Number_t( std::fmod(double(v1),double(v2)) ) ;
}

Number_t Atan2Node::eval(const Args_t &args) const
{
  Number_t v1 = op1_->eval(args);
  Number_t v2 = op2_->eval(args);

  return Number_t( atan2( double(v1), double(v2) ) );
}

Number_t AddNode::eval(const Args_t &args) const
{
  long   isum(0);
  double dsum(0.);

  bool isInteger = true;

  for(vector<Node_t *>::const_iterator op=ops_.begin(); op!=ops_.end(); ++op)
  {
    Number_t t = (*op)->eval(args);

    isInteger = isInteger && t.isInteger();

    isum += long(t);
    dsum += double(t);
  }

  return isInteger ? Number_t(isum) : Number_t(dsum);
}

Number_t MultNode::eval(const Args_t &args) const
{
  long   iprod(1);
  double dprod(1.);

  bool isInteger = true;

  for(vector<Node_t *>::const_iterator op = ops_.begin(); op != ops_.end(); ++op)
  {
    Number_t t = (*op)->eval(args);

    isInteger = isInteger && t.isInteger();

    iprod *= long(t);
    dprod *= double(t);
  }

  return isInteger ? Number_t(iprod) : Number_t(dprod);
}

////////////////////////////////////////////////////////////////////////////////
// Support functions
////////////////////////////////////////////////////////////////////////////////

void invalid_xml(cStrRef_t msg,cStrRef_t m2,cStrRef_t m3,cStrRef_t m4,cStrRef_t m5) 
{
  string err = "Invalid XMLFunc: ";
  err.append(msg);
  if( m2.empty() == false ) { err.append(" "); err.append(m2); }
  if( m3.empty() == false ) { err.append(" "); err.append(m3); }
  if( m4.empty() == false ) { err.append(" "); err.append(m4); }
  if( m5.empty() == false ) { err.append(" "); err.append(m5); }
  throw runtime_error(err + msg);
}

bool is_bracket(cStrRef_t xml, size_t pos)
{
  char c = xml.at(pos);
  if( c=='<' || c=='>' ) return true;
  if( xml.compare(pos,2,"/>") == 0 ) return true;
  return false;
}

// Determines if source is the name of a file or is already an XML string
//   If the former, it loads the XML from the file
//   If the latter, it simply returns the source string.
// Yes, there is a huge assumption here... but if the file does not actually
//   contain XML, it will be caught shortly.
string load_xml(cStrRef_t src)
{
  // Assume src contains a filename and attempt to open it.
  //   If it fails, then assume src is the xml string and return it.

  ifstream s(src.c_str());
  if(s.fail()) return src;

  // read the contents into a string and return it.
  
  stringstream buffer;
  buffer << s.rdbuf();

  return buffer.str();
}


// Removes XML declaration and comments from XML
string cleanup_xml(cStrRef_t orig_xml)
{
  const string decl_start    = "<?xml";
  const string decl_end      = "?>";
  const string comment_start = "<!--";
  const string comment_end   = "-->";

  string xml = orig_xml;
  
  // If the string is empty, there is no cleanup to be done.

  if(xml.empty()) return xml;

  // strip excess whitespace
  //   - replace all sequences of whitespace with a single space character
  //   - remove all whitespace at start/end of the xml string
  //   - remove all whitespace immediately before/after '<', '>', and '/>'
  //     (yes, this hides syntax error where key doesn't immeidately follow '<')

  bool   in_ws    = isspace( xml[0] );
  bool   del_ws   = true;   // remove leading whitespace
  size_t start_ws = 0;

  for(size_t pos = 0; pos<xml.length(); ++pos)
  {
    char c = xml[pos];

    if( isspace(c) )
    {
      // only thing to be done is note the start of the whitespace block
      //   (if we're not already in one)
      if( in_ws == false ) 
      {
        in_ws    = true;
        start_ws = pos;
      }
    }
    else
    {
      bool isBracket = is_bracket(xml,pos);

      if( in_ws )
      {
        if( del_ws || isBracket )
        {
          // delete entire string of whitespace
          xml.erase(start_ws, pos-start_ws);
          pos = start_ws;
        }
        else
        {
          // replace entire string of whitespace with a single space character
          //   doesn't skip the single whitespace case as that might be tab, newline, etc.
          xml.replace(start_ws,pos-start_ws," ");
        }

        in_ws = false; // no longer in whitespace
      }

      del_ws = isBracket;
    }
  }
  if( in_ws )
  {
    // remove trailing whitespace
    xml.erase(start_ws);
  }

  // remove XML prolog(s)
  //   should only be one, but removes all just in case
  //
  // Note that this is not a completely robust search.
  //   If there is an <?xml or ?> buried within quotes,
  //   that will probablby cause issues...

  while(true)
  {
    size_t start_del = xml.find(decl_start);
    if(start_del == string::npos) break;

    size_t end_del = xml.find(decl_end,start_del);
    if(end_del==string::npos) invalid_xml(decl_start,"is missing closing",decl_end);

    xml.erase(start_del, end_del + decl_end.size() - start_del);
  }

  // remove comments
  //
  // Again, f there is an <?-- or --> buried within quotes,
  //   that will probablby cause issues...
  
  while(true)
  {
    size_t start_del = xml.find(comment_start);
    if(start_del == string::npos) break;

    size_t end_del = xml.find(comment_end,start_del);
    if(end_del==string::npos) invalid_xml(comment_start,"is missing closing",comment_end);

    xml.erase(start_del, end_del + comment_end.size() - start_del);
  }

  return xml;
}


// Finds the next tag in the XML and returns its key, attributes, and body.
//   Returns the remaining XML string after removing the current element
bool read_element(cStrRef_t xml, string &key, Attributes_t &attr, string &body, string &tail)
{
  key = "";
  body = "";
  attr.clear();

  size_t pos = 0;
  size_t end = xml.length();

  while(pos<end && isspace(xml[pos])) ++pos;
  if(pos==end)      return false;
  if(xml[pos]!='<') return false;

  const size_t start_elem = pos;

  // determine the key for this element

  const size_t start_key = ++pos;

  while(pos<end && !isspace(xml[pos]) && !is_bracket(xml,pos)) +pos;
  if(pos==end) return false;

  key = xml.substr(start_key,pos-start_key);

  // Find the closing '>' or '/>', parsing attributes along the way
  //
  // There are three ways out of the following while loop
  //   - finding '/>' indicates an empty-body tag: exit is return from this function
  //   - finding '>'  indicates an opening tag: while loop is exited with a break;
  //   - finding neither: the loop exits when it runs out of string to parse (an exception will be thrown)

  size_t body_start(0);

  while( pos<end )
  {
    char c = xml[pos];
   
    if(c=='>')
    {
      // This is a tag with a body (i.e. <key...>[...]</key>
      //   We will need to extract that as our next step

      body_start = ++pos;
      break;
    }
    else if(c=='/')
    {
      // This is an empty-element tag (i.e. <key .... />)
      //   There is no body.
      //   We can return immediately
      if(pos==end-1 || xml[pos+1] != '>') invalid_xml("Missing closing '>' for",key);

      // We can return immediately after stripping out current tag
      tail = xml.substr(pos+2);
      return true;
    }
    else if( isalpha(c) )
    {
      string attr_key;
      string attr_value;
      pos = read_next_attr(xml,pos,attr_key,attr_value); // advances pos

      if(attr_key.empty())
        invalid_xml("Failed to parse attributes for",key,"tag");

      if(attr.find(attr_key)!=attr.end()) 
        invalid_xml("Cannot specify more than one",attr_key,"attribute in",key,"tag");

      attr[attr_key] = attr_value;
    }
    else if( isspace(c) )
    {
      ++pos; // simply advance to next character in XML
    }
    else
    {
      stringstream err;
      err << "Don't know what to do with"<<c<<"in"<<key<<"tag";
      invalid_xml(err.str());
    }
  }

  if(body_start==0) invalid_xml("Missing closing '>' for",key,"tag");

  // Here comes the tricky part... we need to find the closing tag.
  //   We need to handle nested tags including open/close pairs and empty-body tags
  //   We need to ignore anything in quotes

  char q(0);          // current quote char (' or ")
  int  depth(1);      // current nested tag depth
  bool in_tag(false); 
  bool in_closing_tag(false);

  size_t body_end(0);

  while( pos<end )
  {
    char c = xml[pos];

    if(q) // currently in quoted text
    {
      if(c==q) q=0; // closing quote found, exit quote
    }
    else if(in_tag)
    {
      if(c=='\'' || c=='"')
      {
        q = c; // set quote type
      }
      else if(c=='<') 
      {
        invalid_xml("Nesting '<' in body of",key,"tag");
      }
      else if(c=='>') // 
      {
        if(in_closing_tag) --depth;  // closing tag
        else               ++depth;  // opening tag
        in_tag = false;

        if(depth==0) // exited closing tag for element we're reading
        {
          ++pos;
          break;
        }
      }
      else if(c=='/') // empty-body tag
      {
        if(pos >= end-1)    invalid_xml("Incomplete body for",key,"tag");
        if(xml[++pos]!='>') invalid_xml("/ without /> in",key,"tag");
        in_tag = false;
      }
    }
    else if(c=='<') // starting new tag (don't know yet what type)
    {
      in_tag = true;
      if(pos >= end-1) invalid_xml("Incomplete body for",key,"tag");

      in_closing_tag = (xml[pos+1]=='/');

      if( closing_tag )
      {
        ++pos;
        if( depth==1 ) // closing tag for element we're reading, verify key
        {
          body_end = pos-1;
          if( xml.compare(pos+1,key.length(),key) != 0 )
            invalid_xml("Incorrectly nested tags, cannot find closing tag for",key,"tag");
        }
      }
    }

    ++pos; // advance to next character
  }

  if(body_end==0) invalid_xml("Missing closing tag for",key,"tag");
  body = xml.substr(body_start, body_end-body_start);

  tail = xml.substr(pos);
  return true;
}

size_t read_next_attr(cStrRef_t xml,size_t pos,string &attr_key,string &attr_value)
{
  size_t rval = pos;

  attr_key = "";
  attr_value = "";

  size_t start_key(0);
  size_t end_key(0);
  size_t start_value(0);
  size_t end_value(0);

  size_t end = xml.length();

  // skip over leading whitespace (should not see any)
  while(pos<end && isspace(xml[pos])) ++pos;
  if(pos==end) return rval;

  // find the key string

  if( isalpha(xml[pos]) == false ) return rval; // must start with alpha
  start_key = pos;

  while(pos<end && isalnum(xml[pos])) ++pos;
  if(pos==end) return rval;
  end_key = pos;

  while(pos<end && isspace(xml[pos])) ++pos;  // skip over whitespace
  if(pos==end) return rval;

  // verify the equal sign and step over

  if( xml[pos++] != '=' ) return rval;
  if(pos==end) return rval;

  while(pos<end && isspace(xml[pos])) ++pos; // skip over whitespace
  if(pos==end) return rval;

  // verify/identify quote and step over

  char q = xml[pos++];
  if( q!='\'' && q!='"') return rval;
  start_value = pos;

  // find closing quote
  while(pos<end && xml[pos] != q) ++pos;
  if(pos==end) return rval;
  end_value = pos;

  attr_key   = xml.substr(start_key,   end_key   - start_key  );
  attr_value = xml.substr(start_value, end_value - start_value);

  transform(attr_key.begin(),attr_key.end(),attr_key.begin(),::tolower);
  transform(attr_value.begin(),attr_value.end(),attr_value.begin(),::tolower);

  return ++pos;  // step past closing quote
}

Node_t *build_node(cStrRef_t &xml, const ArgDefs &argDefs, string &tail)
{
  Node_t *rval=NULL;

  // first check that the input is not empty

  if( has_content(xml) == false ) return rval;

  // next check to see if the string is actually a numeric value or argument name
  
  string token;
  string junk;
  if( get_token(arg_str,token,junk0okj) == false ) invalid_xml("Unary node arg is empty");

  // next check to see if the string starts with a tag

  // good to go to parse XML

  string key="";
  Attributes_t attr;
  string body="";

  xml = read_element(xml,key,attr,body);

  if      ( key == "double"  ) { rval = new ConstNode( attr,body,argDefs, Number_t::Double  ); }
  else if ( key == "float"   ) { rval = new ConstNode( attr,body,argDefs, Number_t::Double  ); }
  else if ( key == "real"    ) { rval = new ConstNode( attr,body,argDefs, Number_t::Double  ); }
  else if ( key == "integer" ) { rval = new ConstNode( attr,body,argDefs, Number_t::Integer ); }
  else if ( key == "int"     ) { rval = new ConstNode( attr,body,argDefs, Number_t::Integer ); }
  else if ( key == "arg"     ) { rval = new   ArgNode( attr,body,argDefs ); }
  else if ( key == "neg"     ) { rval = new   NegNode( attr,body,argDefs ); }
  else if ( key == "sin"     ) { rval = new   SinNode( attr,body,argDefs ); }
  else if ( key == "cos"     ) { rval = new   CosNode( attr,body,argDefs ); }
  else if ( key == "tan"     ) { rval = new   TanNode( attr,body,argDefs ); }
  else if ( key == "asin"    ) { rval = new  AsinNode( attr,body,argDefs ); }
  else if ( key == "acos"    ) { rval = new  AcosNode( attr,body,argDefs ); }
  else if ( key == "atan"    ) { rval = new  AtanNode( attr,body,argDefs ); }
  else if ( key == "deg"     ) { rval = new   DegNode( attr,body,argDefs ); }
  else if ( key == "rad"     ) { rval = new   RadNode( attr,body,argDefs ); }
  else if ( key == "abs"     ) { rval = new   AbsNode( attr,body,argDefs ); }
  else if ( key == "sqrt"    ) { rval = new  SqrtNode( attr,body,argDefs ); }
  else if ( key == "exp"     ) { rval = new   ExpNode( attr,body,argDefs ); }
  else if ( key == "ln"      ) { rval = new    LnNode( attr,body,argDefs ); }
  else if ( key == "log"     ) { rval = new   LogNode( attr,body,argDefs ); }
  else if ( key == "sub"     ) { rval = new   SubNode( attr,body,argDefs ); }
  else if ( key == "div"     ) { rval = new   DivNode( attr,body,argDefs ); }
  else if ( key == "pow"     ) { rval = new   PowNode( attr,body,argDefs ); }
  else if ( key == "mod"     ) { rval = new   ModNode( attr,body,argDefs ); }
  else if ( key == "atan2"   ) { rval = new Atan2Node( attr,body,argDefs ); }
  else if ( key == "add"     ) { rval = new   AddNode( attr,body,argDefs ); }
  else if ( key == "mult"    ) { rval = new  MultNode( attr,body,argDefs ); }
  else
  {
    invalid_xml("Unrecognized operator key:",key);
  }

  return rval;
}

// returns whether or not the string has something other than whitespace
bool has_content(cStrRef_t s)
{
  for(size_t i=0; i<s.length() ++i)
  {
    if( isspace( s[i] ) == false ) return true;
  }
  return false;
}

// extracts the first token and attempts to interpret it as a double
//   returns true if the the conversion to double is succesful 
//       AND there is only one token
bool string_to_double(cStrRef_t s, double &val, string &extra)
{
  string v_str;
  if( string_to_token( s, v_str, extra ) == false ) return false;

  const char *a = v_str.c_str();
  char *b(NULL);

  val = strtod(a,&b);

  return (a==b ? false : true);
}

// extracts the first token and attempts to interpret it as a long integer
//   returns true if the the conversion to long integer is succesful 
//       AND there is only one token
bool string_to_integer(cStrRef_t s, long &val, string &extra)
{
  string v_str;
  if( string_to_token( s, v_str, extra ) == false ) return false;

  const char *a = v_str.c_str();
  char *b(NULL);

  val = strtol(a,&b,10);

  return (a==b ? false : true);
}

// extracts the first token and return true if there is only one token
bool string_to_token(cStrRef_t s, string &val, string &extra)
{
  if( get_token(s,val,extra) == false ) return false;

  size_t n = extra.length();

  size_t a(0);
  while( a<n && isspace(extra[a]) ) ++a;

  return a==n;
}

// returns the first token found and sets tail to the remainder of
//   the string following the first token.  Returns true if a 
//   token was found or false otherwise
bool get_token(cStrRef_t s, string &token, string &tail)
{
  size_t n = s.length();

  size_t a(0);
  while( a<n && isspace(s[a]) ) ++a;

  if(a==n) return false;

  size_t b(a);
  while( b<n && isspace(s[b])==false ) ++b;

  token = s.substr(a,b-a);
  tail  = s.substr(b);

  return true;
}
