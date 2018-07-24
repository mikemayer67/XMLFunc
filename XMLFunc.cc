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
typedef XMLFunc::Operation   Op_t;
typedef XMLFunc::Args_t      Args_t;

class XMLNode;

class ArgDefs;

// prototypes for support functions

void invalid_xml(const string &err1,
                 const string &err2="",
                 const string &err3="",
                 const string &err4="",
                 const string &err5="");

string load_xml     (const string &src);
string strip_xml    (const string &xml, const string &start, const string &end);

size_t skip_whitespace(const string &xml,size_t pos=0);

bool   has_content  (const string &s);

bool   read_double  (const string &s, double &dval,  string &tail);
bool   read_integer (const string &s, long   &ival,  string &tail);
bool   read_token   (const string &s, string &token, string &tail);

Op_t *build_op(const string &arg,  const ArgDefs &);
Op_t *build_op(const XMLNode *xml, const ArgDefs &);


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
      if( i == xref_.end() ) invalid_xml("bad argument name (",name,")");
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

class XMLNode
{
  public:
    static XMLNode *build(string &xml, XMLNode *parent=NULL);

    ~XMLNode()
    {
      for(vector<XMLNode *>::iterator ci=children_.begin(); ci!=children_.end(); ++ci) delete *ci;
    }

    const string name(void) const { return name_; }
    
    bool hasAttribute(const string &key) const
    {
      return attributes_.find(key) != attributes_.end();
    }

    string attributeValue(const string &key) const
    {
      string rval;
      Attributes_t::const_iterator ai = attributes_.find(key);
      if(ai != attributes_.end()) rval = ai->second;
      return rval;
    }


    bool   hasChildren(void) const { return ! children_.empty(); }
    size_t numChildren(void) const { return children_.size();    }

    const XMLNode *child(size_t i) const
    {
      XMLNode *rval = NULL;
      if(i<children_.size()) rval = children_.at(i);
      return rval;
    }

  private:

    XMLNode(string name) : name_(name) {}

    void addAttribute(const string &key, const string &value) { attributes_[key] = value;   }
    void addChild(XMLNode *node)                              { children_.push_back(node); }

    string             name_;
    Attributes_t       attributes_;
    vector<XMLNode *>  children_;
};


////////////////////////////////////////////////////////////////////////////////
// XMLFunc::Op subclasses
////////////////////////////////////////////////////////////////////////////////

class ConstOp : public XMLFunc::Operation
{
  public:
    ConstOp(const XMLNode *xml, const ArgDefs &, NumberType_t);
    ConstOp(long v)   : value_(v) {}
    ConstOp(double v) : value_(v) {}
    Number_t eval(const Args_t &args) const { return value_; }
  private:
    Number_t value_;
};

class ArgOp : public XMLFunc::Operation
{
  public:
    ArgOp(const XMLNode *xml, const ArgDefs &);
    ArgOp(size_t i) : index_(i) {}
    Number_t eval(const Args_t &args) const { return args.at(index_); }
  private:
    size_t index_;
};


class UnaryOp : public XMLFunc::Operation
{
  public:
    UnaryOp(const XMLNode *xml, const ArgDefs &);
    ~UnaryOp() { if(op_!=NULL) delete op_; }
  protected:
    Op_t *op_;
};

class BinaryOp : public XMLFunc::Operation
{
  public:
    BinaryOp(const XMLNode *xml, const ArgDefs &);
    ~BinaryOp() { if(op1_!=NULL) delete op1_; if(op2_!=NULL) delete op2_; }
  protected:
    Op_t *op1_;
    Op_t *op2_;
};

class ListOp : public XMLFunc::Operation
{
  public:
    ListOp(const XMLNode *xml, const ArgDefs &);
    ~ListOp() 
    { for(vector<Op_t *>::iterator i=ops_.begin(); i!=ops_.end(); ++i) delete *i; }
  protected:
    vector<Op_t *> ops_;
};

class NegOp : public UnaryOp
{
  public:
    NegOp(const XMLNode *xml, const ArgDefs &argDefs) : UnaryOp(xml,argDefs) {}

    Number_t eval(const Args_t &args) const
    { return op_->eval(args).negate(); }
};

class SinOp : public UnaryOp
{
  public: 
    SinOp(const XMLNode *xml, const ArgDefs &argDefs) : UnaryOp(xml,argDefs) {}

    Number_t eval(const Args_t &args) const
    { return sin(double(op_->eval(args))); }
};

class CosOp : public UnaryOp
{
  public:
    CosOp(const XMLNode *xml, const ArgDefs &argDefs) : UnaryOp(xml,argDefs) {}

    Number_t eval(const Args_t &args) const
    { return cos(double(op_->eval(args))); }
};

class TanOp : public UnaryOp
{
  public:
    TanOp(const XMLNode *xml, const ArgDefs &argDefs) : UnaryOp(xml,argDefs) {}

    Number_t eval(const Args_t &args) const
    { return tan(double(op_->eval(args))); }
};

class AsinOp : public UnaryOp
{
  public:
    AsinOp(const XMLNode *xml, const ArgDefs &argDefs) : UnaryOp(xml,argDefs) {}

    Number_t eval(const Args_t &args) const
    { return asin(double(op_->eval(args))); }
};

class AcosOp : public UnaryOp
{
  public:
    AcosOp(const XMLNode *xml, const ArgDefs &argDefs) : UnaryOp(xml,argDefs) {}

    Number_t eval(const Args_t &args) const
    { return acos(double(op_->eval(args))); }
};

class AtanOp : public UnaryOp
{
  public:
    AtanOp(const XMLNode *xml, const ArgDefs &argDefs) : UnaryOp(xml,argDefs) {}

    Number_t eval(const Args_t &args) const
    { return atan(double(op_->eval(args))); }
};

class DegOp : public UnaryOp
{
  public: 
    DegOp(const XMLNode *xml, const ArgDefs &argDefs) : UnaryOp(xml,argDefs) {}

    Number_t eval(const Args_t &args) const
    {
      static double f = 45./atan(1.0);
      return Number_t( f * double(op_->eval(args)) );
    }
};

class RadOp : public UnaryOp
{
  public:
    RadOp(const XMLNode *xml, const ArgDefs &argDefs) : UnaryOp(xml,argDefs) {}

    Number_t eval(const Args_t &args) const
    {
      static double f = atan(1.0)/45.;
      return Number_t( f * double(op_->eval(args)) );
    }
};

class AbsOp : public UnaryOp
{
  public:
    AbsOp(const XMLNode *xml, const ArgDefs &argDefs) : UnaryOp(xml,argDefs) {}

    Number_t eval(const Args_t &args) const
    { return op_->eval(args).abs(); }
};

class SqrtOp : public UnaryOp
{
  public:
    SqrtOp(const XMLNode *xml, const ArgDefs &argDefs) : UnaryOp(xml,argDefs) {}

    Number_t eval(const Args_t &args) const
    { return Number_t( sqrt(double(op_->eval(args))) ); }
};

class ExpOp : public UnaryOp
{
  public:
    ExpOp(const XMLNode *xml, const ArgDefs &argDefs) : UnaryOp(xml,argDefs) {}

    Number_t eval(const Args_t &args) const
    { return Number_t( exp(double(op_->eval(args))) ); }
};

class LnOp : public UnaryOp
{
  public: 
    LnOp(const XMLNode *xml, const ArgDefs &argDefs) : UnaryOp(xml,argDefs) {}

    Number_t eval(const Args_t &args) const
    { return Number_t( log(double(op_->eval(args))) ); }
};

class LogOp : public UnaryOp
{
  public:
    LogOp(const XMLNode *xml, const ArgDefs &); Number_t eval(const Args_t &args) const
    {
      return Number_t( fac_ * log( double(op_->eval(args))) );
    }
  private:
    double fac_;
};

class SubOp : public BinaryOp
{
  public: 
    SubOp(const XMLNode *xml, const ArgDefs &argDefs) : BinaryOp(xml,argDefs) {}

    Number_t eval(const Args_t &args) const;
};

class DivOp : public BinaryOp
{
  public: 
    DivOp(const XMLNode *xml, const ArgDefs &argDefs) : BinaryOp(xml,argDefs) {}

    Number_t eval(const Args_t &args) const;
};

class PowOp : public BinaryOp
{
  public: 
    PowOp(const XMLNode *xml, const ArgDefs &argDefs) : BinaryOp(xml,argDefs) {}

    Number_t eval(const Args_t &args) const;
};

class ModOp : public BinaryOp
{
  public: 
    ModOp(const XMLNode *xml, const ArgDefs &argDefs) : BinaryOp(xml,argDefs) {}

    Number_t eval(const Args_t &args) const;
};

class Atan2Op : public BinaryOp
{
  public: 
    Atan2Op(const XMLNode *xml, const ArgDefs &argDefs) : BinaryOp(xml,argDefs) {}

    Number_t eval(const Args_t &args) const;
};

class AddOp : public ListOp
{
  public: 
    AddOp(const XMLNode *xml, const ArgDefs &argDefs) : ListOp(xml,argDefs) {}

    Number_t eval(const Args_t &args) const;
};

class MultOp : public ListOp
{
  public: 
    MultOp(const XMLNode *xml, const ArgDefs &argDefs) : ListOp(xml,argDefs) {}

    Number_t eval(const Args_t &args) const;
};


////////////////////////////////////////////////////////////////////////////////
// XMLNode methods
////////////////////////////////////////////////////////////////////////////////

// Constructs a new XMLNode using the specified XML.
//   Returns NULL if the XML string is empty.
//   Returns the parent node if this is a closing tag
//   Throws a runtime_error if invalid XMl syntax
XMLNode *XMLNode::build(string &xml, XMLNode *parent)
{
  static const string english_alphabet = "abcdefghijklmnopqrstuvwxyz";

  size_t end_xml = xml.length();

  size_t start_tag = xml.find("<");
  if( start_tag == string::npos )
  {
    if(has_content(xml)) invalid_xml("all content must be tagged");
    return NULL;
  }
  if( has_content( xml.substr(0,start_tag) ) ) invalid_xml("all content must be tagged");

  bool   is_closing(false);
  size_t start_name = start_tag + 1;

  if( xml.compare(start_tag,2,"</") == 0 ) // this is a closing tag
  {
    is_closing = true;
    ++start_name;
  }

  size_t end_name = xml.find_first_not_of(english_alphabet,start_tag);
  if( end_name==start_name) invalid_xml("missing tag name");
  else if(end_name==string::npos) invalid_xml("tag is missing closing '>'");

  string name = xml.substr(start_name, end_name-start_name);

  // validate/handle closing tag
 
  if(is_closing)
  {
    if(parent==NULL)
      invalid_xml("closing </",name,"> tag has no opening tag");

    if(name != parent->name())
      invalid_xml("closing </",name,"> tag does not pair with opening <",parent->name(),"> tag");

    size_t end_tag = skip_whitespace(xml,end_name);

    if( end_tag == string::npos )
      invalid_xml("</",name,"> tag does not have a closing '>'");

    if( xml[end_tag] != '>' )
      invalid_xml("closing tags cannot have attributes");

    xml = xml.substr(end_tag+1);

    return parent;
  }

  // find attributes

  XMLNode *rval = new XMLNode(name);

  bool is_opening_tag(false);

  size_t end_tag(0);
  size_t pos(end_name);
  while( (pos=skip_whitespace(xml,pos)) != string::npos )
  {
    if(xml[pos]=='>') 
    {
      is_opening_tag = true;
      end_tag = pos + 1;
      break;
    }
    else if(xml.compare(pos,2,"/>")==0)
    {
      is_opening_tag = false;
      end_tag = pos + 2;
      break;
    }
    else
    {
      size_t start_key = pos;
      size_t end_key = xml.find_first_not_of(english_alphabet,pos);

      if(end_key==string::npos)
        invalid_xml("attribute key '", xml.substr(pos), "' in <", name, "> has no assigned value");

      if(xml[end_key]!='=')
        invalid_xml("attribute key '", xml.substr(pos), "' in <", name, "> not followed by an '='");

      if(end_key==start_key)
        invalid_xml("attributes keys must consist solely of english alphabet characters");

      size_t open_quote = end_key+1;
      if( open_quote >= end_xml)
        invalid_xml("<",name,"> tag does not have a closing '>'");

      char q = xml[open_quote];
      if( q!='\'' & q!='"' )
        invalid_xml("value for attribute key '", xml.substr(pos), "' in <", name, "> is not quoted");

      size_t start_value = open_quote+1;
      if( start_value >= end_xml)
        invalid_xml("<",name,"> tag does not have a closing '>'");

      size_t end_value = xml.find(q,start_value);
      if(end_value == string::npos)
        invalid_xml("value for attribute key '", xml.substr(pos), "' in <", name, "> has no closing quote");

      string key = xml.substr(start_key,end_key-start_key);
      string value = xml.substr(start_value,end_value-start_value);

      rval->addAttribute(key,value);

      pos = end_value + 1;
    }
  }

  xml = xml.substr(end_tag);

  // Add children nodes if unless this a bodyless tag

  if(is_opening_tag)
  {
    XMLNode *child = build(xml,rval);
    while(child != rval)
    {
      if(child == NULL) invalid_xml("<", name, "> tag is missing closing </", name,"> tag");
      rval->addChild(child);
    }
  }

  return rval;
}


////////////////////////////////////////////////////////////////////////////////
// XMLFunc methods
////////////////////////////////////////////////////////////////////////////////

// XMLFunc constructor

XMLFunc::XMLFunc(const string &src) : root_(NULL), numArgs_(0)
{
  string raw_xml = load_xml(src);
  raw_xml = strip_xml(raw_xml,"<?xml","?>"); // remove declaration
  raw_xml = strip_xml(raw_xml,"<!--","-->"); // remove comments

  transform( raw_xml.begin(), raw_xml.end(), raw_xml.begin(), ::tolower );

  // Extract the arg definition list
  
  ArgDefs argDefs;

  XMLNode *arglist = XMLNode::build(raw_xml);
  if(arglist==NULL)                invalid_xml("empty");
  if(arglist->name() != "arglist") invalid_xml("missing <arglist> element");

  numArgs_ = arglist->numChildren();
  if(numArgs_==0)  invalid_xml("<arglist> is empty");

  for(size_t i=0; i<numArgs_; ++i)
  {
    const XMLNode *arg = arglist->child(i);
    if(arg->name() != "arg") invalid_xml("<arglist> may only contain <arg> elements");

    NumberType_t type = Number_t::Double;

    const string &type_str = arg->attributeValue("type");
    if( type_str.empty() == false )
    {
      if     (type_str == "double")  { type = Number_t::Double; }
      else if(type_str == "float")   { type = Number_t::Double; }
      else if(type_str == "real")    { type = Number_t::Double; }
      else if(type_str == "integer") { type = Number_t::Integer; }
      else if(type_str == "int")     { type = Number_t::Integer; }
      else invalid_xml("Unknown argument type: ", type_str);
    }

    const string &name = arg->attributeValue("name");

    if( name.empty() ) { argDefs.add(type); }
    else               { argDefs.add(type,name); }
  }

  // Extract the function defintion

  XMLNode *xml = XMLNode::build(raw_xml);
  if(!xml) invalid_xml("missing valid root value element");
  if(has_content(raw_xml)) invalid_xml("only one root value element allowed");

  root_ = build_op(xml,argDefs);
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

////////////////////////////////////////////////////////////////////////////////
// XMLFunc::Op subclass methods
////////////////////////////////////////////////////////////////////////////////

ConstOp::ConstOp(const XMLNode *xml, const ArgDefs &argDefs, NumberType_t type)
{
  const string &value = xml->attributeValue("value");
  if( value.empty() ) invalid_xml("Const op must have a value attribute");

  if( xml->hasChildren() ) invalid_xml("Const op cannot have child ops");

  string extra;
  if(type == Number_t::Double)
  {
    double dval(0.);
    if( ! read_double( value, dval, extra ) ) invalid_xml("Invalid double value (",value,")");
    value_ = Number_t(dval);
  }
  else
  {
    long ival(0);
    if( ! read_integer( value, ival, extra ) ) invalid_xml("Invalid integer value (",value,")");
    value_ = Number_t(ival);
  }
  if( has_content(extra) ) invalid_xml("Extraneous data (",extra,") following ",value);
}


ArgOp::ArgOp(const XMLNode *xml, const ArgDefs &argDefs)
{
  const string &index_attr = xml->attributeValue("index");
  const string &name_attr  = xml->attributeValue("name");

  bool hasIndex = ( index_attr.empty() == false );
  bool hasName  = ( name_attr.empty() == false );

  if( hasIndex && hasName ) 
  {
    invalid_xml("Arg op may only contain name or index attribute, not both");
  }
  else if(hasIndex)
  {
    long ival(0);
    string extra;

    if( read_integer(index_attr,ival,extra) == false )
      invalid_xml("index attribute is not an integer (",index_attr,")");

    if(has_content(extra)) 
      invalid_xml("index attribute contains extraneous data (",extra,")");

    index_ = (size_t)ival;

    if( ival<0 || index_ >= argDefs.size() )
    {
      stringstream err;
      err << "Argument index " << index_ << " is out of range (0-" << argDefs.size()-1 << ")";
      invalid_xml(err.str());
    }
  }
  else if(hasName)
  {
    string name;
    string extra;

    if( read_token(name_attr,name,extra) == false )
      invalid_xml("Arg name attribute must contain a non-empty string");

    if(has_content(extra)) 
      invalid_xml("name attribute contains extraneous data (",extra,")");

    index_ = argDefs.index(name);
  }
  else
  {
    invalid_xml("Arg op must contain either name or index attribute");
  }
}


UnaryOp::UnaryOp(const XMLNode *xml, const ArgDefs &argDefs) : op_(NULL)
{
  const string &arg = xml->attributeValue("arg");

  bool hasArg = arg.empty() == false;

  size_t numArg  = xml->numChildren();
  if(hasArg) ++numArg;

  if(numArg == 0)
  {
    invalid_xml(xml->name(), " op requires an arg attribute or child element");
  }
  else if(numArg>1)
  {
    invalid_xml(xml->name(), " op cannot specify more than one arg attribute or child element");
  }
  else if(hasArg)
  {
    op_ = build_op(arg,argDefs);
  }
  else
  {
    op_ = build_op(xml->child(0),argDefs);
  }
}

BinaryOp::BinaryOp(const XMLNode *xml, const ArgDefs &argDefs) : op1_(NULL), op2_(NULL)
{
  const string &arg1 = xml->attributeValue("arg1");
  const string &arg2 = xml->attributeValue("arg2");

  bool hasArg1 = arg1.empty() == false;
  bool hasArg2 = arg2.empty() == false;

  size_t numArg  = xml->numChildren();
  if(hasArg1) ++numArg;
  if(hasArg2) ++numArg;

  if(numArg < 2)
  {
    invalid_xml(xml->name(), " op requires two arg attributes or child elements");
  }
  else if(numArg>2)
  {
    invalid_xml(xml->name(), " op cannot specify more than two arg attribute or child element");
  }
  else if(hasArg1 && hasArg2)
  {
    op1_ = build_op(arg1,argDefs);
    op2_ = build_op(arg2,argDefs);
  }
  else if(hasArg1)
  {
    op1_ = build_op(arg1,argDefs);
    op2_ = build_op(xml->child(0),argDefs);
  }
  else if(hasArg2)
  {
    op1_ = build_op(xml->child(0),argDefs);
    op2_ = build_op(arg2,argDefs);
  }
  else
  {
    op1_ = build_op(xml->child(0),argDefs);
    op2_ = build_op(xml->child(1),argDefs);
  }
}

ListOp::ListOp(const XMLNode *xml, const ArgDefs &argDefs)
{
  const string &arg1 = xml->attributeValue("arg1");
  const string &arg2 = xml->attributeValue("arg2");

  bool hasArg1 = arg1.empty() == false;
  bool hasArg2 = arg2.empty() == false;

  size_t numArg  = xml->numChildren();
  if(hasArg1) ++numArg;
  if(hasArg2) ++numArg;

  if(hasArg2 && numArg<2)
    invalid_xml(xml->name(), " op requires at least two arg attributes or child elements if arg2 is specified");

  if(numArg<1)
    invalid_xml(xml->name(), " op requires at least one arg attribute or child element");

  size_t j(0);
  for(size_t i=0; i<numArg; ++i)
  {
    Op_t *op = NULL;
    if( i==0 && hasArg1 )      op = build_op( arg1, argDefs );
    else if( i==1 && hasArg2 ) op = build_op( arg2, argDefs );
    else                       op = build_op( xml->child(j++), argDefs );
    ops_.push_back(op);
  }
}

LogOp::LogOp(const XMLNode *xml, const ArgDefs &argDefs) : UnaryOp(xml,argDefs)
{
  double base(10.);

  string base_str = xml->attributeValue("base");
  if( base_str.empty() == false )
  {
    string extra;
    if( read_double(base_str, base, extra) == false ) invalid_xml("Invalid base value (",base_str,") for log");
    if( has_content(extra) )                          invalid_xml("Extraneous data found for base value");
    if(base <= 0.)                                    invalid_xml("Base for log must be a positive value");
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

void invalid_xml(const string &m1,const string &m2,const string &m3,const string &m4,const string &m5) 
{
  string err = "Invalid XMLFunc: ";
  err.append(m1).append(m2).append(m3).append(m4).append(m5);
  throw runtime_error(err);
}

// Determines if source is the name of a file or is already an XML string
//   If the former, it loads the XML from the file
//   If the latter, it simply returns the source string.
// Yes, there is a huge assumption here... but if the file does not actually//   contain XML, it will be caught shortly.
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

// Removes specified string (based on start/end) from XML
string strip_xml(const string &xml, const string &start, const string &end)
{
  // If the string is empty, there is no cleanup to be done.
  if(xml.empty()) return xml;

  string rval = xml;

  // remove all occurances of start.[stuff].end

  while(true)
  {
    size_t start_del = rval.find(start);
    if(start_del == string::npos) break;

    size_t end_del = rval.find(end,start_del);
    if(end_del==string::npos) invalid_xml(start," is missing closing ",end);

    rval.erase(start_del, end_del + end.size() - start_del);
  }

  return rval;
}

// Locates the first non-whitespace character in the string beginning at
//   the specified location.  Returns string::npos if all reamaining 
//   characters are white space.
size_t skip_whitespace(const string &s,size_t pos)
{
  size_t end = s.length();
  for(size_t i=pos; i<end; ++i)
  {
    if( isspace(s[i]) == false ) return i;
  }
  return string::npos;
}


// returns whether or not the string has something other than whitespace
bool has_content(const string &s)
{
  for(size_t i=0; i<s.length(); ++i)
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


// Constructs an XMLFunc::operation pointer from an attribute value
Op_t *build_op(const string &xml, const ArgDefs &argDefs)
{
  string token;
  string extra;

  if( read_token(xml,token,extra) == false ) invalid_xml("empty argument value");
  if( has_content(extra) )                   invalid_xml("extraneous data in arg value ('", extra,"')");

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

// Constructs an XMLFunc::operation pointer from an XMLNode
Op_t *build_op(const XMLNode *xml, const ArgDefs &argDefs)
{
  Op_t *rval=NULL;

  string name=xml->name();

  if      ( name == "double"  ) { rval = new ConstOp( xml,argDefs, Number_t::Double  ); }
  else if ( name == "float"   ) { rval = new ConstOp( xml,argDefs, Number_t::Double  ); }
  else if ( name == "real"    ) { rval = new ConstOp( xml,argDefs, Number_t::Double  ); }
  else if ( name == "integer" ) { rval = new ConstOp( xml,argDefs, Number_t::Integer ); }
  else if ( name == "int"     ) { rval = new ConstOp( xml,argDefs, Number_t::Integer ); }
  else if ( name == "arg"     ) { rval = new   ArgOp( xml,argDefs ); }
  else if ( name == "neg"     ) { rval = new   NegOp( xml,argDefs ); }
  else if ( name == "sin"     ) { rval = new   SinOp( xml,argDefs ); }
  else if ( name == "cos"     ) { rval = new   CosOp( xml,argDefs ); }
  else if ( name == "tan"     ) { rval = new   TanOp( xml,argDefs ); }
  else if ( name == "asin"    ) { rval = new  AsinOp( xml,argDefs ); }
  else if ( name == "acos"    ) { rval = new  AcosOp( xml,argDefs ); }
  else if ( name == "atan"    ) { rval = new  AtanOp( xml,argDefs ); }
  else if ( name == "deg"     ) { rval = new   DegOp( xml,argDefs ); }
  else if ( name == "rad"     ) { rval = new   RadOp( xml,argDefs ); }
  else if ( name == "abs"     ) { rval = new   AbsOp( xml,argDefs ); }
  else if ( name == "sqrt"    ) { rval = new  SqrtOp( xml,argDefs ); }
  else if ( name == "exp"     ) { rval = new   ExpOp( xml,argDefs ); }
  else if ( name == "ln"      ) { rval = new    LnOp( xml,argDefs ); }
  else if ( name == "log"     ) { rval = new   LogOp( xml,argDefs ); }
  else if ( name == "sub"     ) { rval = new   SubOp( xml,argDefs ); }
  else if ( name == "div"     ) { rval = new   DivOp( xml,argDefs ); }
  else if ( name == "pow"     ) { rval = new   PowOp( xml,argDefs ); }
  else if ( name == "mod"     ) { rval = new   ModOp( xml,argDefs ); }
  else if ( name == "atan2"   ) { rval = new Atan2Op( xml,argDefs ); }
  else if ( name == "add"     ) { rval = new   AddOp( xml,argDefs ); }
  else if ( name == "mult"    ) { rval = new  MultOp( xml,argDefs ); }
  else
  {
    invalid_xml("Unrecognized operator name (",name,")");
  }

  return rval;
}

