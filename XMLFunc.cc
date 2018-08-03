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
typedef XMLFunc::Operation  *OpPtr_t;
typedef vector<OpPtr_t>      OpList_t;
typedef XMLFunc::ArgDefs     ArgDefs_t;
typedef XMLFunc::Args        Args_t;

class XMLNode;

// prototypes for support functions

#define INVALID_XML(err) { \
  stringstream msg; \
  msg << "Invalid XML [" << __FILE__ << ":" << __LINE__ << "]: " << err; \
  throw runtime_error(msg.str()); \
}

string load_xml     (const string &src);
string strip_xml    (const string &xml, const string &start, const string &end);

size_t skip_whitespace(const string &xml,size_t pos=0);

bool   has_content  (const string &s);

bool   read_double  (const string &s, double &dval,  string &tail);
bool   read_integer (const string &s, long   &ival,  string &tail);
bool   read_token   (const string &s, string &token, string &tail);

OpPtr_t build_op(const string &arg,  const ArgDefs_t &);
OpPtr_t build_op(const XMLNode *xml, const ArgDefs_t &);

////////////////////////////////////////////////////////////////////////////////
// Support classes
////////////////////////////////////////////////////////////////////////////////

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


// XMLFunc::ArgDefs methods

void XMLFunc::ArgDefs::add(NumberType_t type, const string &name)
{
  if(name.empty()==false) xref_[name] = int(types_.size());
  types_.push_back(type);
}

size_t XMLFunc::ArgDefs::index(const string &name) const
{
  Xref_t::const_iterator i = xref_.find(name);
  if( i == xref_.end() ) INVALID_XML("bad argument name (" << name << ")");
  return i->second;
}

pair<size_t,bool> XMLFunc::ArgDefs::find(const string &name) const
{
  pair<size_t,bool> rval(0,false);

  Xref_t::const_iterator i = xref_.find(name);
  if( i != xref_.end() )
  {
    rval.first = i->second;
    rval.second = true;
  }

  return rval;
}

////////////////////////////////////////////////////////////////////////////////
// XMLFunc::Op subclasses
////////////////////////////////////////////////////////////////////////////////

class ConstOp : public XMLFunc::Operation
{
  public:

    static ConstOp *build(const XMLNode *xml, const ArgDefs_t &argDefs)
    {
      ConstOp *rval(NULL);

      string name = xml->name();
      if      ( name == "double"  ) { rval = new ConstOp( xml,argDefs, Number_t::Double  ); }
      else if ( name == "float"   ) { rval = new ConstOp( xml,argDefs, Number_t::Double  ); }
      else if ( name == "real"    ) { rval = new ConstOp( xml,argDefs, Number_t::Double  ); }
      else if ( name == "integer" ) { rval = new ConstOp( xml,argDefs, Number_t::Integer ); }
      else if ( name == "int"     ) { rval = new ConstOp( xml,argDefs, Number_t::Integer ); }

      return rval;
    }

    ConstOp(long v)   : value_(v) {}
    ConstOp(double v) : value_(v) {}

    Number_t eval(const Args_t &args) const { return value_; }

  private:

    ConstOp(const XMLNode *xml, const ArgDefs_t &, NumberType_t);

    Number_t value_;
};

class ArgOp : public XMLFunc::Operation
{
  public:

    static ArgOp *build(const XMLNode *xml, const ArgDefs_t &args)
    {
      ArgOp *rval(NULL);
      if( xml->name() == "arg") rval = new ArgOp(xml,args);
      return rval;
    }

    ArgOp(size_t i) : index_(i) {}

    Number_t eval(const Args_t &args) const { return args.at(index_); }

  private:

    ArgOp(const XMLNode *xml, const ArgDefs_t &);

    size_t index_;
};


class UnaryOp : public XMLFunc::Operation
{
  public:

    typedef enum { NEG, ABS, SIN, COS, TAN, ASIN, ACOS, ATAN, DEG, RAD, SQRT, EXP, LN, CHILD } Type_t;

    ~UnaryOp() { if(op_!=NULL) delete op_; }

    static UnaryOp *build(const XMLNode *xml, const ArgDefs_t &args)
    {
      UnaryOp *rval(NULL);

      string name = xml->name();
      if      ( name == "neg"  ) rval = new UnaryOp(xml,args,NEG);
      else if ( name == "abs"  ) rval = new UnaryOp(xml,args,ABS);
      else if ( name == "sin"  ) rval = new UnaryOp(xml,args,SIN);
      else if ( name == "cos"  ) rval = new UnaryOp(xml,args,COS);
      else if ( name == "tan"  ) rval = new UnaryOp(xml,args,TAN);
      else if ( name == "asin" ) rval = new UnaryOp(xml,args,ASIN);
      else if ( name == "acos" ) rval = new UnaryOp(xml,args,ACOS);
      else if ( name == "atan" ) rval = new UnaryOp(xml,args,ATAN);
      else if ( name == "deg"  ) rval = new UnaryOp(xml,args,DEG);
      else if ( name == "rad"  ) rval = new UnaryOp(xml,args,RAD);
      else if ( name == "sqrt" ) rval = new UnaryOp(xml,args,SQRT);
      else if ( name == "exp"  ) rval = new UnaryOp(xml,args,EXP);
      else if ( name == "ln"   ) rval = new UnaryOp(xml,args,LN);

      return rval;
    }

    Number_t eval(const Args_t &args) const
    {
      static double deg_to_rad = atan(1.0)/45.;
      static double rad_to_deg = 1./deg_to_rad;

      Number_t v = op_->eval(args);

      Number_t rval;
      switch(type_)
      {
        case NEG:  rval = v.negate();             break;
        case ABS:  rval = v.abs();                break;

        case SIN:  rval = sin(  double(v) );      break;
        case COS:  rval = cos(  double(v) );      break;
        case TAN:  rval = tan(  double(v) );      break;
        case ASIN: rval = asin( double(v) );      break;
        case ACOS: rval = acos( double(v) );      break;
        case ATAN: rval = atan( double(v) );      break;
        case SQRT: rval = sqrt( double(v) );      break;
        case EXP:  rval = exp(  double(v) );      break;
        case LN:   rval = log(  double(v) );      break;

        case DEG:  rval = double(v) * rad_to_deg; break;
        case RAD:  rval = double(v) * deg_to_rad; break;

        case CHILD: 
          throw logic_error("Child class of UnaryOp missing override of eval method"); 
          break;
      }
      return rval;
    }

  protected:

    UnaryOp(const XMLNode *, const ArgDefs_t &, Type_t);

    Type_t  type_;
    OpPtr_t op_;
};

class BinaryOp : public XMLFunc::Operation
{
  public:

    typedef enum { SUB, DIV, MOD, POW, ATAN2 } Type_t;

    ~BinaryOp() 
    {
      if(op1_!=NULL) delete op1_;
      if(op2_!=NULL) delete op2_; 
    }

    static BinaryOp *build(const XMLNode *xml, const ArgDefs_t &args)
    {
      BinaryOp *rval(NULL);

      string name = xml->name();
      if      ( name == "sub"   ) rval = new BinaryOp(xml,args,SUB);
      else if ( name == "div"   ) rval = new BinaryOp(xml,args,DIV);
      else if ( name == "mod"   ) rval = new BinaryOp(xml,args,MOD);
      else if ( name == "pow"   ) rval = new BinaryOp(xml,args,POW);
      else if ( name == "atan2" ) rval = new BinaryOp(xml,args,ATAN2);

      return rval;
    }

    Number_t eval(const Args_t &args) const
    {
      Number_t v1 = op1_->eval(args);
      Number_t v2 = op2_->eval(args);

      bool isInteger = v1.isInteger() && v2.isInteger();

      Number_t rval;
      switch(type_)
      {
        case SUB: 
          if(isInteger) rval = Number_t( long(v1)   - long(v2)   );
          else          rval = Number_t( double(v1) - double(v2) ); 
          break;

        case DIV: 
          if(isInteger) rval = Number_t( long(v1)   / long(v2)   );
          else          rval = Number_t( double(v1) / double(v2) ); 
          break;

        case MOD:
          if(isInteger) rval = Number_t( long(v1) % long(v2) );
          else          rval = Number_t( std::fmod(double(v1),double(v2)) );

        case POW: 
          rval = Number_t( pow( double(v1), double(v2) ) );
          break;

        case ATAN2: 
          rval = Number_t( atan2( double(v1), double(v2)) );
          break;
      }
      return rval;
    }

  protected:

    BinaryOp(const XMLNode *xml, const ArgDefs_t &, Type_t);

    Type_t  type_;
    OpPtr_t op1_;
    OpPtr_t op2_;
};

class ListOp : public XMLFunc::Operation
{
  public:

    typedef enum { ADD, MULT } Type_t;

    ~ListOp() { for(OpList_t::iterator i=ops_.begin(); i!=ops_.end(); ++i) delete *i; }

    static ListOp *build(const XMLNode *xml, const ArgDefs_t &argDefs)
    {
      ListOp *rval(NULL);

      string name = xml->name();
      if      ( name == "add"  ) rval = new ListOp(xml,argDefs,ADD);
      else if ( name == "mult" ) rval = new ListOp(xml,argDefs,MULT);

      return rval;
    }

    Number_t eval(const Args_t &args) const
    {
      long   ival(0);
      double dval(0);

      switch(type_)
      {
        case ADD:  ival=0; dval=0.0;  break;
        case MULT: ival=1; dval=1.0;  break;
      }

      bool isInteger = true;

      for(OpList_t::const_iterator op = ops_.begin(); op!=ops_.end(); ++op)
      {
        Number_t v = (*op)->eval(args);

        isInteger = isInteger && v.isInteger();

        switch(type_)
        {
          case ADD:   ival += long(v);  dval += double(v);  break;
          case MULT:  ival *= long(v);  dval *= double(v);  break;
        }
      }

      return ( isInteger ? Number_t(ival) : Number_t(dval) );
    }

  protected:

    ListOp(const XMLNode *xml, const ArgDefs_t &, Type_t);

    Type_t   type_;
    OpList_t ops_;
};




class LogOp : public UnaryOp
{
  public:

    static LogOp *build(const XMLNode *xml, const ArgDefs_t &args)
    {
      LogOp *rval(NULL);
      if( xml->name() == "log" ) rval = new LogOp(xml,args);
      return rval;
    }

    Number_t eval(const Args_t &args) const
    {
      return Number_t( fac_ * log( double(op_->eval(args))) );
    }
  private:

    LogOp(const XMLNode *xml, const ArgDefs_t &); 

    double fac_;
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
  static const string alpha      = "abcdefghijklmnopqrstuvwxyz";
  static const string alphanum   = alpha + "0123456789";

  size_t end_xml = xml.length();

  size_t start_tag = xml.find("<");
  if( start_tag == string::npos )
  {
    if(has_content(xml)) INVALID_XML("all content must be tagged");
    return NULL;
  }
  if( has_content( xml.substr(0,start_tag) ) ) INVALID_XML("all content must be tagged");

  bool   is_closing(false);
  size_t start_name = start_tag + 1;

  if( xml.compare(start_tag,2,"</") == 0 ) // this is a closing tag
  {
    is_closing = true;
    ++start_name;
  }

  size_t end_name = xml.find_first_not_of(alphanum,start_name);
  if( end_name==start_name  ) INVALID_XML("missing tag name");
  if( end_name==string::npos) INVALID_XML("tag is missing closing '>'");

  string name = xml.substr(start_name, end_name-start_name);

  // validate/handle closing tag
 
  if(is_closing)
  {
    if(parent==NULL)
      INVALID_XML("closing </" << name << "> tag has no opening tag");

    if(name != parent->name())
      INVALID_XML("closing </" << name << "> tag does not pair with opening <" << parent->name() << "> tag");

    size_t end_tag = skip_whitespace(xml,end_name);

    if( end_tag == string::npos )
      INVALID_XML("</" << name << "> tag does not have a closing '>'");

    if( xml[end_tag] != '>' )
      INVALID_XML("closing tags cannot have attributes");

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
      if( xml.find_first_of(alpha,pos) != pos )
        INVALID_XML("attribute keys must start with a-z, not '" << xml.substr(pos,1) << "'");

      size_t start_key = pos;
      size_t end_key = xml.find_first_not_of(alphanum,pos);

      if(end_key==string::npos)
        INVALID_XML("attribute key '" << xml.substr(start_key) << "' in <" << name << "> has no assigned value");

      string key = xml.substr(start_key,end_key-start_key);

      if(xml[end_key]!='=')
        INVALID_XML("attribute key '" << key << "' in <" << name << "> not followed by an '='");

      if(end_key==start_key)
        INVALID_XML("attributes keys must consist solely of alphanumeric characters");

      size_t start_value = end_key+1;
      if( start_value >= end_xml)
        INVALID_XML("<" << name << "> tag does not have a closing '>'");


      // value may or may not be quoted
      size_t end_value = start_value;

      char q = xml[start_value];
      if( q=='"' || q=='\'' ) 
      { 
        start_value += 1; 
        if( start_value >= end_xml)
          INVALID_XML("<" << name << "> tag does not have a closing '>'");

        end_value = xml.find(q,start_value);
        if(end_value == string::npos)
          INVALID_XML("value for attribute key '" << key << "' in <" << name << "> has no closing quote");
        
        pos = end_value+1;
      }
      else
      { 
        end_value = xml.find_first_not_of(alphanum+".-+",start_value);
        if(end_value == string::npos)
          INVALID_XML("<" << name << "> tag does not have a closing '>'");
        
        pos = end_value;
      }

      string value = xml.substr(start_value,end_value-start_value);

      rval->addAttribute(key,value);
    }
  }

  xml = xml.substr(end_tag);

  // Add children nodes if unless this a bodyless tag

  if(is_opening_tag)
  {
    for(XMLNode *child=build(xml,rval); child!=rval; child=build(xml,rval))
    {
      if(child == NULL) INVALID_XML("<" << name << "> tag is missing closing </" << name <<"> tag");
      rval->addChild(child);
    }
  }

  return rval;
}


////////////////////////////////////////////////////////////////////////////////
// XMLFunc methods
////////////////////////////////////////////////////////////////////////////////

void populate(ArgDefs_t &argDefs, const XMLNode *xml)
{
  size_t numArgs = xml->numChildren();

  if(numArgs==0)  INVALID_XML("<arglist> is empty");

  argDefs.clear();
  for(size_t i=0; i<numArgs; ++i)
  {
    const XMLNode *arg = xml->child(i);
    if(arg->name() != "arg") INVALID_XML("<arglist> may only contain <arg> elements");

    NumberType_t type = Number_t::Double;

    const string &type_str = arg->attributeValue("type");
    if( type_str.empty() == false )
    {
      if     (type_str == "double")  { type = Number_t::Double; }
      else if(type_str == "float")   { type = Number_t::Double; }
      else if(type_str == "real")    { type = Number_t::Double; }
      else if(type_str == "integer") { type = Number_t::Integer; }
      else if(type_str == "int")     { type = Number_t::Integer; }
      else INVALID_XML("Unknown argument type: " << type_str);
    }

    const string &name = arg->attributeValue("name");

    if( name.empty() ) { argDefs.add(type); }
    else               { argDefs.add(type,name); }
  }
}

// XMLFunc constructor

XMLFunc::XMLFunc(const string &src)
{
  string raw_xml = load_xml(src);
  raw_xml = strip_xml(raw_xml,"<?xml","?>"); // remove declaration
  raw_xml = strip_xml(raw_xml,"<!--","-->"); // remove comments

  transform( raw_xml.begin(), raw_xml.end(), raw_xml.begin(), ::tolower );

  ArgDefs_t sharedArgDefs;

  while(has_content(raw_xml))
  {
    XMLNode *xml = XMLNode::build(raw_xml);
    if( xml==NULL ) INVALID_XML("Failed to parse root level element");

    string tag = xml->name();

    if( tag == "arglist" )
    {
      populate(sharedArgDefs,xml);
    }
    else if( tag == "func" )
    {
      size_t numChildren = xml->numChildren();
      if( numChildren == 1 )
      {
        if(sharedArgDefs.empty()) 
          INVALID_XML("<func> must have <arglist> child as there is no root level <arglist>");

        OpPtr_t func = build_op( xml->child(0), sharedArgDefs );
        funcs_.push_back( Function(func, sharedArgDefs) );
      }
      else if(numChildren == 2 )
      {
        const XMLNode *arglist = xml->child(0);
        if( arglist->name() != "arglist" ) 
          INVALID_XML("<arglist> must be first element in <func> if there is more than one child element");

        ArgDefs_t argDefs;
        populate(argDefs,arglist);

        OpPtr_t func = build_op( xml->child(1), argDefs );

        funcs_.push_back( Function(func, argDefs) );
      }
      else
      {
        INVALID_XML("<func> must one child element, with an optional arg list");
      }

      if( xml->hasAttribute("name") )
      {
        string name = xml->attributeValue("name");
        size_t index = funcs_.size() - 1;

        XrefEntry_t entry(name,index);

        pair< Xref_t::iterator, bool> rc = funcXref_.insert(entry);

        if(rc.second == false) INVALID_XML("function name " << name << " can only be used once");
      }
    }
    else
    {
      INVALID_XML("Only <func> and <arglist> elements may exist at root level");
    }
  }

  if(funcs_.empty()) INVALID_XML("contains no <func> elements");
}

Number_t XMLFunc::eval(const Args_t &args) const
{
  if(funcs_.size() != 1) 
    throw runtime_error("Must specify the function by name or index as there is more than one functin defined");

  return _eval(funcs_.at(0), args);
}

Number_t XMLFunc::eval(size_t index, const Args_t &args) const
{
  if(index >= funcs_.size())
  {
    stringstream err;
    err << "Invalid function index (" << index << ").  There are only " << funcs_.size() << " functions defined";
    throw runtime_error(err.str());
  }

  return _eval(funcs_.at(index), args);
}

Number_t XMLFunc::eval(const string &name,const Args_t &args) const
{
  Xref_t::const_iterator i = funcXref_.find(name);

  if(i==funcXref_.end())
  {
    stringstream err;
    err << "Invalid function name (" << name << ")";
    throw runtime_error(err.str());
  }

  size_t index = i->second;

  return _eval( funcs_.at(index), args);
}

Number_t XMLFunc::_eval(const Function &f, const Args_t &args) const
{
  if( args.size() < f.argDefs.count() )
  {
    stringstream err;
    err << "Insufficient arguments passed to eval.  Need " << f.argDefs.count()
      << ". Only " << args.size() << " were provided";
    throw runtime_error(err.str());
  }

  for(int i=0; i<f.argDefs.count(); ++i)
  {
    const Number &arg = args.at(i);
    if(f.argDefs.type(i) == Number_t::Integer && arg.isDouble())
    {
      stringstream err;
      err << "Argument " << i << " should be an integer, but a double ("
        << double(arg) << ") was passed to eval()";
      throw runtime_error(err.str());
    }
  }

  return f.root->eval(args);
}

////////////////////////////////////////////////////////////////////////////////
// XMLFunc::Op subclass methods
////////////////////////////////////////////////////////////////////////////////

ConstOp::ConstOp(const XMLNode *xml, const ArgDefs_t &argDefs, NumberType_t type)
{
  const string &value = xml->attributeValue("value");
  if( value.empty() ) INVALID_XML("Const op must have a value attribute");

  if( xml->hasChildren() ) INVALID_XML("Const op cannot have child ops");

  string extra;
  if(type == Number_t::Double)
  {
    double dval(0.);
    if( ! read_double( value, dval, extra ) ) INVALID_XML("Invalid double value (" << value << ")");
    value_ = Number_t(dval);
  }
  else
  {
    long ival(0);
    if( ! read_integer( value, ival, extra ) ) INVALID_XML("Invalid integer value (" << value << ")");
    value_ = Number_t(ival);
  }
  if( has_content(extra) ) INVALID_XML("Extraneous data (" << extra << ") following " << value);
}


ArgOp::ArgOp(const XMLNode *xml, const ArgDefs_t &argDefs)
{
  const string &index_attr = xml->attributeValue("index");
  const string &name_attr  = xml->attributeValue("name");

  bool hasIndex = ( index_attr.empty() == false );
  bool hasName  = ( name_attr.empty() == false );

  if( hasIndex && hasName ) 
  {
    INVALID_XML("Arg op may only contain name or index attribute, not both");
  }
  else if(hasIndex)
  {
    long ival(0);
    string extra;

    if( read_integer(index_attr,ival,extra) == false )
      INVALID_XML("index attribute is not an integer (" << index_attr << ")");

    if(has_content(extra)) 
      INVALID_XML("index attribute contains extraneous data (" << extra << ")");

    index_ = (size_t)ival;

    if( ival<0 || index_ >= argDefs.count() )
      INVALID_XML("Argument index " << index_ << " is out of range (0-" << argDefs.count()-1 << ")");
  }
  else if(hasName)
  {
    string name;
    string extra;

    if( read_token(name_attr,name,extra) == false )
      INVALID_XML("Arg name attribute must contain a non-empty string");

    if(has_content(extra)) 
      INVALID_XML("name attribute contains extraneous data (" << extra << ")");

    index_ = argDefs.index(name);
  }
  else
  {
    INVALID_XML("Arg op must contain either name or index attribute");
  }
}


UnaryOp::UnaryOp(const XMLNode *xml, const ArgDefs_t &argDefs, Type_t type) 
  : type_(type), op_(NULL)
{
  const string &arg = xml->attributeValue("arg");

  bool hasArg = arg.empty() == false;

  size_t numArg  = xml->numChildren();
  if(hasArg) ++numArg;

  if(numArg == 0)
    INVALID_XML(xml->name() << " op requires an arg attribute or child element");
  if(numArg>1)
    INVALID_XML(xml->name() << " op cannot specify more than one arg attribute or child element");

  if(hasArg) op_ = build_op(arg,argDefs);
  else       op_ = build_op(xml->child(0),argDefs);
}

BinaryOp::BinaryOp(const XMLNode *xml, const ArgDefs_t &argDefs, Type_t type) 
  : type_(type), op1_(NULL), op2_(NULL)
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
    INVALID_XML(xml->name() << " op requires two arg attributes or child elements");
  }
  else if(numArg>2)
  {
    INVALID_XML(xml->name() << " op cannot specify more than two arg attribute or child element");
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

ListOp::ListOp(const XMLNode *xml, const ArgDefs_t &argDefs,Type_t type) : type_(type)
{
  const string &arg1 = xml->attributeValue("arg1");
  const string &arg2 = xml->attributeValue("arg2");

  bool hasArg1 = arg1.empty() == false;
  bool hasArg2 = arg2.empty() == false;

  size_t numArg  = xml->numChildren();
  if(hasArg1) ++numArg;
  if(hasArg2) ++numArg;

  if(hasArg2 && numArg<2)
    INVALID_XML(xml->name() << " op requires at least two arg attributes or child elements if arg2 is specified");

  if(numArg<1)
    INVALID_XML(xml->name() << " op requires at least one arg attribute or child element");

  size_t j(0);
  for(size_t i=0; i<numArg; ++i)
  {
    OpPtr_t op = NULL;
    if( i==0 && hasArg1 )      op = build_op( arg1, argDefs );
    else if( i==1 && hasArg2 ) op = build_op( arg2, argDefs );
    else                       op = build_op( xml->child(j++), argDefs );
    ops_.push_back(op);
  }
}

LogOp::LogOp(const XMLNode *xml, const ArgDefs_t &argDefs) : UnaryOp(xml,argDefs,CHILD)
{
  double base(10.);

  string base_str = xml->attributeValue("base");
  if( base_str.empty() == false )
  {
    string extra;
    if( read_double(base_str, base, extra) == false ) INVALID_XML("Invalid base value (" << base_str << ") for log");
    if( has_content(extra) )                          INVALID_XML("Extraneous data found for base value");
    if(base <= 0.)                                    INVALID_XML("Base for log must be a positive value");
  }

  fac_ = 1. / log(base);
}

////////////////////////////////////////////////////////////////////////////////
// Support functions
////////////////////////////////////////////////////////////////////////////////

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
    if(end_del==string::npos) INVALID_XML(start << " is missing closing " << end);

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
  if( read_token(s,token,tail) == false ) return false;

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
  if( read_token(s,token,tail) == false ) return false;

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


// Constructs an XMLFunc::operation pointer from an XMLNode
OpPtr_t build_op(const XMLNode *xml, const ArgDefs_t &argDefs)
{
  OpPtr_t rval=NULL;

  if( rval == NULL ) rval =  ConstOp::build( xml, argDefs );
  if( rval == NULL ) rval =    ArgOp::build( xml, argDefs );
  if( rval == NULL ) rval =  UnaryOp::build( xml, argDefs );
  if( rval == NULL ) rval = BinaryOp::build( xml, argDefs );
  if( rval == NULL ) rval =   ListOp::build( xml, argDefs );
  if( rval == NULL ) rval =    LogOp::build( xml, argDefs );

  if( rval == NULL) 
    INVALID_XML("Unrecognized operator name (" << xml->name() << ")");

  return rval;
}


// Constructs an XMLFunc::operation pointer from an attribute value
OpPtr_t build_op(const string &xml, const ArgDefs_t &argDefs)
{
  string token;
  string extra;

  if( read_token(xml,token,extra) == false ) INVALID_XML("empty argument value");
  if( has_content(extra) )                   INVALID_XML("extraneous data in arg value ('" << extra << "')");

  long ival;
  if( read_integer( token, ival, extra ) )
  {
    if(has_content(extra)) 
      INVALID_XML("Extraneous data found after integer value (" << extra << ")");

    return new ConstOp(ival);
  }

  double dval;
  if( read_double( token, dval, extra ) )
  {
    if(has_content(extra)) 
      INVALID_XML("Extraneous data found after double value (" << extra << ")");

    return new ConstOp(dval);
  }

  pair<size_t,bool> rc = argDefs.find(token);
  if( rc.second == false ) INVALID_XML("Unrecognized argument name (" << token << ")");

  return new ArgOp(rc.first);
}

