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

typedef XMLFunc::Number      Number_t;
typedef Number_t::Type_t     NumberType_t;
typedef XMLFunc::Op        Op_t;
typedef XMLFunc::Args_t      Args_t;

class ArgDefs;

// prototypes for support functions

void invalid_xml(const string &m1,
                 const string &m2="",
                 const string &m3="",
                 const string &m4="",
                 const string &m5="");

string load_xml    (const string &src);
string cleanup_xml (const string &xml);

bool   is_bracket   (const string &s, size_t pos);
bool   has_content  (const string &s);

bool   read_double  (const string &s, double &dval,  string &tail);
bool   read_integer (const string &s, long   &ival,  string &tail);
bool   read_token   (const string &s, string &token, string &tail);

bool   read_element (const string &s, string &key, Attributes_t &, string &body, string &tail);
bool   read_attr    (const string &s, string &key, string &value, string &tail);

Op_t *build_op(const string &xml, const ArgDefs &, string &tail); // modifies xml


////////////////////////////////////////////////////////////////////////////////
// Support classes
////////////////////////////////////////////////////////////////////////////////

class ArgDefs
{
  public:
    ArgDefs(void) {}

    void add(NumberType_t type, const string &name="")
    {
      if(name.empty()==false) xref_[name] = int(types_.size());
      types_.push_back(type);
    }

    int          size(void)  const { return int(types_.size()); }
    NumberType_t type(int i) const { return types_.at(i);  }

    int index(const string &name) const
    {
      NameXref_t::const_iterator i = xref_.find(name);
      if( i == xref_.end() ) invalid_xml("Bad argument name (",name,")");
      return i->second;
    }

    int lookup(const string &name) const
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
// XMLFunc::Op subclasses
////////////////////////////////////////////////////////////////////////////////

class ConstOp : public Op_t
{
  public:
    ConstOp(const Attributes_t &, const string &body, const ArgDefs &, NumberType_t);
    ConstOp(long v)   : value_(v) {}
    ConstOp(double v) : value_(v) {}
    Number_t eval(const Args_t &args) const { return value_; }
  private:
    Number_t value_;
};

class ArgOp : public Op_t
{
  public:
    ArgOp(const Attributes_t &, const string &body, const ArgDefs &);
    ArgOp(size_t i) : index_(i) {}
    Number_t eval(const Args_t &args) const { return args.at(index_); }
  private:
    size_t index_;
};


class UnaryOp : public Op_t
{
  public:
    UnaryOp(const Attributes_t &, const string &body, const ArgDefs &);
    ~UnaryOp() { if(op_!=NULL) delete op_; }
  protected:
    Op_t *op_;
};

class BinaryOp : public Op_t
{
  public:
    BinaryOp(const Attributes_t &, const string &body, const ArgDefs &);
    ~BinaryOp() { if(op1_!=NULL) delete op1_; if(op2_!=NULL) delete op2_; }
  protected:
    Op_t *op1_;
    Op_t *op2_;
};

class ListOp : public Op_t
{
  public:
    ListOp(const Attributes_t &, const string &body, const ArgDefs &);
    ~ListOp() 
    { for(vector<Op_t *>::iterator i=ops_.begin(); i!=ops_.end(); ++i) delete *i; }
  protected:
    vector<Op_t *> ops_;
};

class NegOp : public UnaryOp
{
  public:
    NegOp(const Attributes_t &attr,const string &body, const ArgDefs &argDefs)
      : UnaryOp(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const
    { return op_->eval(args).negate(); }
};

class SinOp : public UnaryOp
{
  public: 
    SinOp(const Attributes_t &attr,const string &body, const ArgDefs &argDefs)
      : UnaryOp(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const
    { return sin(double(op_->eval(args))); }
};

class CosOp : public UnaryOp
{
  public:
    CosOp(const Attributes_t &attr,const string &body, const ArgDefs &argDefs)
      : UnaryOp(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const
    { return cos(double(op_->eval(args))); }
};

class TanOp : public UnaryOp
{
  public:
    TanOp(const Attributes_t &attr,const string &body, const ArgDefs &argDefs)
      : UnaryOp(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const
    { return tan(double(op_->eval(args))); }
};

class AsinOp : public UnaryOp
{
  public:
    AsinOp(const Attributes_t &attr,const string &body, const ArgDefs &argDefs)
      : UnaryOp(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const
    { return asin(double(op_->eval(args))); }
};

class AcosOp : public UnaryOp
{
  public:
    AcosOp(const Attributes_t &attr,const string &body, const ArgDefs &argDefs)
      : UnaryOp(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const
    { return acos(double(op_->eval(args))); }
};

class AtanOp : public UnaryOp
{
  public:
    AtanOp(const Attributes_t &attr,const string &body, const ArgDefs &argDefs)
      : UnaryOp(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const
    { return atan(double(op_->eval(args))); }
};

class DegOp : public UnaryOp
{
  public: 
    DegOp(const Attributes_t &attr,const string &body, const ArgDefs &argDefs)
      : UnaryOp(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const
    {
      static double f = 45./atan(1.0);
      return Number_t( f * double(op_->eval(args)) );
    }
};

class RadOp : public UnaryOp
{
  public:
    RadOp(const Attributes_t &attr,const string &body, const ArgDefs &argDefs)
      : UnaryOp(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const
    {
      static double f = atan(1.0)/45.;
      return Number_t( f * double(op_->eval(args)) );
    }
};

class AbsOp : public UnaryOp
{
  public:
    AbsOp(const Attributes_t &attr,const string &body, const ArgDefs &argDefs)
      : UnaryOp(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const
    { return op_->eval(args).abs(); }
};

class SqrtOp : public UnaryOp
{
  public:
    SqrtOp(const Attributes_t &attr,const string &body, const ArgDefs &argDefs)
      : UnaryOp(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const
    { return Number_t( sqrt(double(op_->eval(args))) ); }
};

class ExpOp : public UnaryOp
{
  public:
    ExpOp(const Attributes_t &attr,const string &body, const ArgDefs &argDefs)
      : UnaryOp(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const
    { return Number_t( exp(double(op_->eval(args))) ); }
};

class LnOp : public UnaryOp
{
  public: 
    LnOp(const Attributes_t &attr,const string &body, const ArgDefs &argDefs)
      : UnaryOp(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const
    { return Number_t( log(double(op_->eval(args))) ); }
};

class LogOp : public UnaryOp
{
  public:
    LogOp(const Attributes_t &, const string &body, const ArgDefs &);
    Number_t eval(const Args_t &args) const
    {
      return Number_t( fac_ * log( double(op_->eval(args))) );
    }
  private:
    double fac_;
};

class SubOp : public BinaryOp
{
  public: 
    SubOp(const Attributes_t &attr,const string &body, const ArgDefs &argDefs)
      : BinaryOp(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const;
};

class DivOp : public BinaryOp
{
  public: 
    DivOp(const Attributes_t &attr,const string &body, const ArgDefs &argDefs)
      : BinaryOp(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const;
};

class PowOp : public BinaryOp
{
  public: 
    PowOp(const Attributes_t &attr,const string &body, const ArgDefs &argDefs)
      : BinaryOp(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const;
};

class ModOp : public BinaryOp
{
  public: 
    ModOp(const Attributes_t &attr,const string &body, const ArgDefs &argDefs)
      : BinaryOp(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const;
};

class Atan2Op : public BinaryOp
{
  public: 
    Atan2Op(const Attributes_t &attr,const string &body, const ArgDefs &argDefs)
      : BinaryOp(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const;
};

class AddOp : public ListOp
{
  public: 
    AddOp(const Attributes_t &attr,const string &body, const ArgDefs &argDefs)
      : ListOp(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const;
};

class MultOp : public ListOp
{
  public: 
    MultOp(const Attributes_t &attr,const string &body, const ArgDefs &argDefs)
      : ListOp(attr,body,argDefs) {}

    Number_t eval(const Args_t &args) const;
};

////////////////////////////////////////////////////////////////////////////////
// XMLFunc methods
////////////////////////////////////////////////////////////////////////////////

// XMLFunc constructor

XMLFunc::XMLFunc(const string &src) : root_(NULL), numArgs_(0)
{
  string xml = cleanup_xml( load_xml(src) );

  // Extract the arg definition list
  
  string       key;
  Attributes_t attr;
  string       args;
  string       func;

  bool rc = read_element(xml,key,attr,args,func);

  if(rc==false || key!="arglist") invalid_xml("missing valid <arglist>");
  if(args.empty())                invalid_xml("<arglist> contains no <arg> entries");

  ArgDefs argDefs;

  while(args.empty()==false)
  {
    string body;
    rc = read_element(args,key,attr,body,args);

    if(rc==false || key!="arg") invalid_xml("<arglist> may only contain <arg> elements in its body");
    if(body.empty()==false)     invalid_xml("<arg>cannot contain a body");

    NumberType_t type = Number_t::Double;

    Attributes_t::const_iterator ai = attr.find("type");
    if(ai != attr.end()) 
    {
      if     (ai->second == "double")  { type = Number_t::Double; }
      else if(ai->second == "float")   { type = Number_t::Double; }
      else if(ai->second == "real")    { type = Number_t::Double; }
      else if(ai->second == "integer") { type = Number_t::Integer; }
      else if(ai->second == "int")     { type = Number_t::Integer; }
      else invalid_xml("Unknown argument type: ", ai->second);
    }

    ai = attr.find("name");
    if(ai != attr.end()) { argDefs.add(type,ai->second); }
    else                 { argDefs.add(type); }
  }

  // Build the root evaluation operation

  string extra;
  root_ = build_op(func,argDefs,extra);

  if( root_ == NULL )      invalid_xml("missing valid root value element");
  if( has_content(extra) ) invalid_xml("only one root value element allowed");
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
// XMLFunc::Op subclass methods
////////////////////////////////////////////////////////////////////////////////

ConstOp::ConstOp(const Attributes_t &attr, const string &body, const ArgDefs &argDefs, NumberType_t type)
{
  string value_str;

  Attributes_t::const_iterator ai = attr.find("value");
  
  bool hasValue = ( ai != attr.end() );
  bool hasBody  = has_content(body);

  if( hasValue && hasBody ) invalid_xml("Const op cannot contain both value attribute and a body");
  else if (hasValue)        value_str = ai->second; 
  else if (hasBody)         value_str = body;
  else                      invalid_xml("Const op must contain either value attribute or a body");

  string extra;
  if(type == Number_t::Double)
  {
    double dval(0.);
    if( ! read_double( value_str, dval, extra ) ) invalid_xml("Invalid double value (",value_str,")");
    value_ = Number_t(dval);
  }
  else
  {
    long ival(0);
    if( ! read_integer( value_str, ival, extra ) ) invalid_xml("Invalid integer value (",value_str,")");
    value_ = Number_t(ival);
  }
  if( has_content(extra) ) invalid_xml("Extraneous data (",extra,") following ",value_str);
}


ArgOp::ArgOp(const Attributes_t &attr, const string &body, const ArgDefs &argDefs)
{
  Attributes_t::const_iterator index_iter = attr.find("index");
  Attributes_t::const_iterator name_iter  = attr.find("name");

  bool hasIndex = index_iter != attr.end();
  bool hasName  = name_iter  != attr.end();
  bool hasBody  = has_content(body);

  int n = (hasIndex?1:0) + (hasName?1:0) + (hasBody?1:0);

  if(n > 1) invalid_xml("Arg op may only contain one of value attribute, index attribute, or body");
  if(n < 1) invalid_xml("Arg op must contain one of value attribute, index attribute, or body");

  long index;
  string name;
  string extra;
  if(hasBody) // determine if it a name or index
  {
    if     ( read_integer(body,index,extra) )  { hasIndex = true; }
    else if(   read_token(body,name,extra)  )  { hasName  = true; {
    else 
      invalid_xml("Arg body must contain either argument index or name");

    if( has_content(extra) ) 
      invalid_xml("Arg body contains extraneous data (",extra,")");
  }
  else if(hasIndex)
  {
    if( read_integer(index_iter->second, index, extra) )
    {
      if( has_content(extra) ) invalid_xml("Arg index attribute contains extraneous data (",extra,")");
    }
    else
    {
      invalid_xml("Arg index attribute is not an integer (",index_iter->second,")");
    }
  }
  else // hasName
  {
    if( read_token(index_iter->second, name, extra) )
    {
      if( has_content(extra) ) invalid_xml("Arg name attribute contains extraneous data (",extra,")");
    }
    else
    {
      invalid_xml("Arg name attribute must contain a non-empty string");
    }
  }

  if(hasIndex)
  {
    index_ = (size_t)index;

    if(index_<0 )
    {
      stringstream err;
      err << "Argument index " << index_ << " cannot be negative";
      invalid_xml(err.str());
    }
    if(index_>=argDefs.size()) 
    {
      stringstream err;
      err << "Argument index " << index_ << " exceeds max value of" << argDefs.size()-1;
      invalid_xml(err.str());
    }
  }
  else
  {
    index_ = argDefs.index(name);
  }
}


UnaryOp::UnaryOp(const Attributes_t &attr, const string &body, const ArgDefs &argDefs)
  : op_(NULL);
{
  Attributes_t::const_iterator ai = attr.find("arg");
  if( ai != attr.end() )
  {
    string extra;
    op_ = build_op(ai->second, argDefs, extra);
    if( has_content(extra) ) invalid_xml("Extraneous arg data found");
  }
  else if( has_content(body) )
  {
    op_ = build_op(body, argDefs, body);
  }
  else
  {
    invalid_xml("Missing argument data");
  }
  if(op_==NULL) invalid_xml("Failed to parse arg data");

  if(has_content(body)) invalid_xml("Extraneous argument data found");
}

BinaryOp::BinaryOp(const Attributes_t &attr, const string &body, const ArgDefs &argDefs)
  : op1_(NULL), op2_(NULL)
{
  Attributes_t::const_iterator ai = attr.find("arg1");
  if( ai != attr.end() )
  {
    string extra;
    op1_ = build_op(ai->second, argDefs, extra);
    if(has_content(extra)) invalid_xml("Extraneous arg1 data found");
  }
  else if( has_content(body) )
  {
    op1_ = build_op(body, argDefs, body);
  }
  else
  {
    invalid_xml("Missing argument data");
  }
  if(op1_==NULL) invalid_xml("Failed to parse arg1 data");

  ai = attr.find("arg2");
  if( ai != attr.end() )
  {
    string extra;
    op2_ = build_op(ai->second, argDefs, extra);
    if(has_content(extra)) invalid_xml("Extraneous arg2 data found");
  }
  else if( has_content(body) )
  {
    op2_ = build_op(body, argDefs, body);
  }
  else
  {
    invalid_xml("Missing second argument data");
  }
  if(op2_==NULL) invalid_xml("Failed to parse arg1 data");

  if(has_content(body)) invalid_xml("Extraneous argument data found");
}

ListOp::ListOp(const Attributes_t &attr, const string &body, const ArgDefs &argDefs)
{
  while(has_content(body))
  {
    Op_t *op = build_op(body,argDefs,body);
    if( op == NULL ) 
    {
      stringstream err;
      err << "Failed to parse arg" << ops_.length() << " data";
      invalid_xml(err.str());
    }
    ops_.push_back(op);
  }
  if(ops_.empty()) invalid_xml("Missing argument data");
}

LogOp::LogOp(const Attributes_t &attr, const string &body, const ArgDefs &argDefs)
  : UnaryOp(attr,body,argDefs)
{
  double base(10.);

  Attributes_t::const_iterator ai = attr.find("base");
  if( ai != attr.end() )
  {
    string extra;
    if( read_double(ai->second, base, extra) == false ) 
      invalid_xml("Invalid base value (",ai->second,") for log");
    if( has_content(extra) )
      invalid_xml("Extraneous data found for base value");
    if(base <= 0.)
      invalid_xml("Base for log must be a positive value");
  }

  fac_ = 1. / log(base);
}

Number_t SubOp::eval(const Args_t &args) const
{
  Number_t v1 = op1_->eval(args);
  Number_t v2 = op2_->eval(args);

  return 
    ( v1.isInteger() && v2.isInteger() )
    ? Number_t( long(v1) - long(v2) )
    : Number_t( double(v1) - double(v2) ) ;
}

Number_t DivOp::eval(const Args_t &args) const
{
  Number_t v1 = op1_->eval(args);
  Number_t v2 = op2_->eval(args);

  return 
    ( v1.isInteger() && v2.isInteger() )
    ? Number_t( long(v1) / long(v2) )
    : Number_t( double(v1) / double(v2) ) ;
}

Number_t PowOp::eval(const Args_t &args) const
{
  Number_t v1 = op1_->eval(args);
  Number_t v2 = op2_->eval(args);

  return Number_t( pow( double(v1), double(v2) ) );
}

Number_t ModOp::eval(const Args_t &args) const
{
  Number_t v1 = op1_->eval(args);
  Number_t v2 = op2_->eval(args);

  return 
    ( v1.isInteger() && v2.isInteger() )
    ? Number_t( long(v1) % long(v2) )
  : Number_t( std::fmod(double(v1),double(v2)) ) ;
}

Number_t Atan2Op::eval(const Args_t &args) const
{
  Number_t v1 = op1_->eval(args);
  Number_t v2 = op2_->eval(args);

  return Number_t( atan2( double(v1), double(v2) ) );
}

Number_t AddOp::eval(const Args_t &args) const
{
  long   isum(0);
  double dsum(0.);

  bool isInteger = true;

  for(vector<Op_t *>::const_iterator op=ops_.begin(); op!=ops_.end(); ++op)
  {
    Number_t t = (*op)->eval(args);

    isInteger = isInteger && t.isInteger();

    isum += long(t);
    dsum += double(t);
  }

  return isInteger ? Number_t(isum) : Number_t(dsum);
}

Number_t MultOp::eval(const Args_t &args) const
{
  long   iprod(1);
  double dprod(1.);

  bool isInteger = true;

  for(vector<Op_t *>::const_iterator op = ops_.begin(); op != ops_.end(); ++op)
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

void invalid_xml(const string &msg,const string &m2,const string &m3,const string &m4,const string &m5) 
{
  string err = "Invalid XMLFunc: ";
  err.append(msg);
  if( m2.empty() == false ) { err.append(m2); }
  if( m3.empty() == false ) { err.append(m3); }
  if( m4.empty() == false ) { err.append(m4); }
  if( m5.empty() == false ) { err.append(m5); }
  throw runtime_error(err + msg);
}

bool is_bracket(const string &xml, size_t pos)
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
string load_xml(const string &src)
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
string cleanup_xml(const string &orig_xml)
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

  for(size_t pos=0; pos<xml.length(); ++pos)
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

  if(in_ws) xml.erase(start_ws);  // remove trailing whitespace

  // remove XML prolog(s)
  //   should only be one, but removes all just in case
  //
  // Note that this is not a completely robust search.
  //   If there is an <?xml or ?> buried within quotes,
  //   that will probably cause issues...

  while(true)
  {
    size_t start_del = xml.find(decl_start);
    if(start_del == string::npos) break;

    size_t end_del = xml.find(decl_end,start_del);
    if(end_del==string::npos) invalid_xml(decl_start," is missing closing ",decl_end);

    xml.erase(start_del, end_del + decl_end.size() - start_del);
  }

  // remove comments
  //
  // Again, if there is an <?-- or --> buried within quotes,
  //   that will probably cause issues...
  
  while(true)
  {
    size_t start_del = xml.find(comment_start);
    if(start_del == string::npos) break;

    size_t end_del = xml.find(comment_end,start_del);
    if(end_del==string::npos) invalid_xml(comment_start," is missing closing ",comment_end);

    xml.erase(start_del, end_del + comment_end.size() - start_del);
  }

  return xml;
}


// Finds the next tag in the XML and returns its key, attributes, and body.
//   Returns the remaining XML string after removing the current element
bool read_element(const string &xml, string &key, Attributes_t &attr, string &body, string &tail)
{
  key = "";
  body = "";
  attr.clear();

  size_t pos = 0;
  size_t end = xml.length();

  while(pos<end && isspace(xml[pos])) ++pos;
  if(pos==end)      return false;
  if(xml[pos]!='<') return false;
  
  if(pos+1<end && xml[pos+1]=='/') invalid_xml("Cannot start element with closing tag");

  size_t elem_start = pos;
  size_t elem_end(0);

  size_t attr_start(0);
  size_t attr_end(0);

  size_t body_start(0);
  size_t body_end(0);

  bool in_tag(true);
  bool in_closing_tag(false);
  bool in_key(false);

  char q='\0';

  vector<string> keys;

  for( ++pos ; pos<end ; ++pos )
  {
    char c = xml[pos];

    if(q) //currently in quoted text
    {
      if(c==q) q = '\0'; // closing quote found, exit quote
    }
    else if(c=='\'' || c=='"') // enter quote
    {
      q = c; // set quote type
    }
    else if(in_tag)
    {
      if(c=='<')
      {
        invalid_xml("Extraneous '<' found inside tag");
      }
      else if(c=='/')
      {
        if(pos+1 < end && xml[pos+1]=='>') // bodyless tag
        {
          if( in_closing_tag ) invalid_xml("Tag cannot start with </ and end with />");

          in_tag = in_closing_tag = false;
          key = keys.back();
          keys.pop_back();

          if(keys.empty()) 
          { 
            attr_end = pos-1;
            pos+=2; 
            break; 
          }
        }
        else
        {
          invalid_xml("Extraneous '/' found inside tag");
        }
      }
      else if(c=='>')
      {
      }
    }
    else // in body
    {
      if(c=='>')
      {
        invalid_xml("Extraneous '>' found outside XML tag");
      }
      if(c=='<') // entering tag
      {
        in_tag = true;

        if( pos+1 < end && xml[pos+1]=='/' ) // </ => closing tag
        {
          ++pos;
          in_closing_tag = true;

          // validate key
          key = keys.back();
          keys.pop_back();
          if( xml.compare(pos+1,key.size(),key) != 0 )
            invalid_xml("Incorrectly nexted tags, missing closing tag for <",key,">");

          if(keys.empty()) body_end = pos-1;
        }
        else // opening or body-less tag
        {
          size_t key_start = ++pos;
          while(pos<end && isalpha(xml[pos]) ) ++pos;
          if( pos==end ) invalid_xml("Incomplete opening tag");
          if( pos==key_start) invalid_xml("Opening tag is missing key");

          if(keys.empty()) attr_start = pos;

          key = xml.substr(key_start,pos-key_start);
          keys.push_back(key) ;
          --pos;
        }
      }
    }
  }




  }

  // determine the key for this element

  const size_t start_key = ++pos;

  while(pos<end && !isspace(xml[pos]) && !is_bracket(xml,pos)) ++pos;
  if(pos==end) return false;

  key = xml.substr(start_key,pos-start_key);

  // Find the closing '>' or '/>', parsing attributes along the way
  //
  // There are three ways out of the following while loop
  //   - finding '/>' indicates an empty-body tag: exit is return from this function
  //   - finding '>'  indicates an opening tag: while loop is exited with a break;
  //   - finding neither: the loop exits when it runs out of string to parse (an exception will be thrown)

  size_t attr_start(pos);
  size_t body_start(0);

  char q='\0';

  while( pos<end )
  {
    char c = xml[pos];
   
    if(c=='>')
    {
      // This is a tag with a body (i.e. <key...>[...]</key>
      attr_end   = pos-1;
      body_start = ++pos;
      break;
    }
    else if(c=='/')
    {
      // This is an empty-element tag (i.e. <key .... />), i.e. no body
      if(pos==end-1 || xml[pos+1] != '>') invalid_xml("Missing closing '>' for ",key);

      attr_end = pos-1;
      break;
    }
    else if( isalpha(c) )
    {
      string attr_key;
      string attr_value;
      read_next_attr(xml,pos,attr_key,attr_value); // advances pos

      if(attr_key.empty())
        invalid_xml("Failed to parse attributes for <",key,">");

      if(attr.find(attr_key)!=attr.end()) 
        invalid_xml("Cannot specify more than one ",attr_key," attribute in <",key,">");

      attr[attr_key] = attr_value;
    }
    else if( isspace(c) )
    {
      ++pos; // simply advance to next character in XML
    }
    else
    {
      stringstream err;
      err << "Don't know what to do with '"<<c<<"' in <"<<key<<">";
      invalid_xml(err.str());
    }
  }

  if(body_start==0) invalid_xml("Missing closing '>' for <",key,">");

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
        invalid_xml("Nesting '<' in body of <",key,">");
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
        if(pos >= end-1)    invalid_xml("Incomplete body for <",key,">");
        if(xml[++pos]!='>') invalid_xml("/ without /> in <",key,">");
        in_tag = false;
      }
    }
    else if(c=='<') // starting new tag (don't know yet what type)
    {
      in_tag = true;
      if(pos >= end-1) invalid_xml("Incomplete body for <",key,">");

      in_closing_tag = (xml[pos+1]=='/');

      if( closing_tag )
      {
        ++pos;
        if( depth==1 ) // closing tag for element we're reading, verify key
        {
          body_end = pos-1;
          if( xml.compare(pos+1,key.length(),key) != 0 )
            invalid_xml("Incorrectly nested tags, cannot find closing tag for <",key,">");
        }
      }
    }

    ++pos; // advance to next character
  }

  if(body_end==0) invalid_xml("Missing closing tag for <",key,">");
  body = xml.substr(body_start, body_end-body_start);

  tail = xml.substr(pos);
  return true;
}

bool read_attr(const string &xml,string &key,string &value, string &tail)
{
  if(has_content(xml)==false) return false;

  key = "";
  value = "";

  size_t end = xml.length();
  
  size_t start_key(0);
  while( start_key<end && isspace(xml[start_key]) ) ++start_key;

  if(start_key == end) return false;
  if(isalpha(xml[start_key])==false) invalid_xml("attribute keys must start with a roman letter");

  size_t end_key = start_key;
  while( end_key<end && isalnum(xml[end_key]) ) ++end_key;

  if(end_key == end ) invalid_xml("attribute is missing value");

  size_t start_value = end_key;
  while( start_value<end && isspace(xml[start_value]) ) ++start_value;
  if( start_value == end || xml[start_value] != '=' ) invalid_xml("attribute is missing '='");
  ++start_value;
  while( start_value<pos && isspace(xml[start_value]) ) ++start_value;
  if( start_value == end ) invalid_xml("attribute is missing value");
  
  char q = xml[start_value];
  if( q != '\'' && q != '"' ) invalid_xml("attribute value must be in quotes");
  ++start_value;

  size_t end_value = start_value;
  while( end_value<end && xml[end_value] != q ) ++end_value;
  if( end_value == end ) invalid_xml("attribute value is missing closing quote");

  key = xml.substr(start_key, end_key-start_key);
  value = xml.substr(start_value, end_value-start_value);
  tail = xml.substr( end_value + 1 );

  return true;
}

Op_t *build_op(const string &&xml, const ArgDefs &argDefs, string &tail)
{
  Op_t *rval=NULL;

  // get the first token off of the XML string
  //   return NULL if none is found

  string token;
  string tail;

  if( read_token(xml,token,tail) == false ) return NULL;

  // if token does not start with '<' (aka is not a tag), look to see if
  //   it is either a numeric value or argument name

  if( token[0] != '<' )
  {
    string extra;

    long ival;
    if( read_integer( token, ival, extra ) )
    {
      if(has_content(extra)) 
        invalid_xml("Extraneous data found after integer value (",extra,")");

      return new ConstOp(ival);
    }

    double dval;
    if( read_double( token, dval, extra ) )
    {
      if(has_content(extra)) 
        invalid_xml("Extraneous data found after double value (",extra,")");

      return new ConstOp(dval);
    }

    int index = argDefs.lookup(token);
    if( index < 0 ) invalid_xml("Unrecognized argument name (",token,")");

    return new ArgOp(index);
  }

  // good to go to parse XML

  string key="";
  Attributes_t attr;
  string body="";

  if( read_element(xml,key,attr,body,tail) == false ) return NULL;

  if      ( key == "double"  ) { rval = new ConstOp( attr,body,argDefs, Number_t::Double  ); }
  else if ( key == "float"   ) { rval = new ConstOp( attr,body,argDefs, Number_t::Double  ); }
  else if ( key == "real"    ) { rval = new ConstOp( attr,body,argDefs, Number_t::Double  ); }
  else if ( key == "integer" ) { rval = new ConstOp( attr,body,argDefs, Number_t::Integer ); }
  else if ( key == "int"     ) { rval = new ConstOp( attr,body,argDefs, Number_t::Integer ); }
  else if ( key == "arg"     ) { rval = new   ArgOp( attr,body,argDefs ); }
  else if ( key == "neg"     ) { rval = new   NegOp( attr,body,argDefs ); }
  else if ( key == "sin"     ) { rval = new   SinOp( attr,body,argDefs ); }
  else if ( key == "cos"     ) { rval = new   CosOp( attr,body,argDefs ); }
  else if ( key == "tan"     ) { rval = new   TanOp( attr,body,argDefs ); }
  else if ( key == "asin"    ) { rval = new  AsinOp( attr,body,argDefs ); }
  else if ( key == "acos"    ) { rval = new  AcosOp( attr,body,argDefs ); }
  else if ( key == "atan"    ) { rval = new  AtanOp( attr,body,argDefs ); }
  else if ( key == "deg"     ) { rval = new   DegOp( attr,body,argDefs ); }
  else if ( key == "rad"     ) { rval = new   RadOp( attr,body,argDefs ); }
  else if ( key == "abs"     ) { rval = new   AbsOp( attr,body,argDefs ); }
  else if ( key == "sqrt"    ) { rval = new  SqrtOp( attr,body,argDefs ); }
  else if ( key == "exp"     ) { rval = new   ExpOp( attr,body,argDefs ); }
  else if ( key == "ln"      ) { rval = new    LnOp( attr,body,argDefs ); }
  else if ( key == "log"     ) { rval = new   LogOp( attr,body,argDefs ); }
  else if ( key == "sub"     ) { rval = new   SubOp( attr,body,argDefs ); }
  else if ( key == "div"     ) { rval = new   DivOp( attr,body,argDefs ); }
  else if ( key == "pow"     ) { rval = new   PowOp( attr,body,argDefs ); }
  else if ( key == "mod"     ) { rval = new   ModOp( attr,body,argDefs ); }
  else if ( key == "atan2"   ) { rval = new Atan2Op( attr,body,argDefs ); }
  else if ( key == "add"     ) { rval = new   AddOp( attr,body,argDefs ); }
  else if ( key == "mult"    ) { rval = new  MultOp( attr,body,argDefs ); }
  else
  {
    invalid_xml("Unrecognized operator key (",key,")");
  }

  return rval;
}

// returns whether or not the string has something other than whitespace
bool has_content(const string &s)
{
  for(size_t i=0; i<s.length() ++i)
  {
    if( isspace( s[i] ) == false ) return true;
  }
  return false;
}

// extracts the first token and attempts to interpret it as a double
//   if succesful, true is returned and tail is set to the substring following the first token
//   otherwise, false is returned and tail is set to the empty string
bool read_double(const string &s, double &val, string &tail)
{
  string token;
  if( read_token(s,token,tail) ) return false;

  const char *a = token.c_str();
  char *b(NULL);

  val = strtod(a,&b);

  if( a==b ) { tail = ""; return false; }
  
  return true;
}

// extracts the first token and attempts to interpret it as a long integer
//   if succesful, true is returned and tail is set to the substring following the first token
//   otherwise, false is returned and tail is set to the empty string
bool read_integer(const string &s, long &val, string &tail)
{
  string token;
  if( read_token(s,token,tail) ) return false;

  const char *a = token.c_str();
  char *b(NULL);

  val = strtol(a,&b,10);

  if( a==b ) { tail = ""; return false; }
  
  return true;
}

// extracts the first token from the input string
//   returns true if one is found and sets tail to the following substring
//   returns false if no token is found
bool read_token(const string &s, string &token, string &tail)
{
  token = "";
  tail  = "";

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
