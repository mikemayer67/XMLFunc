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

// prototypes for support functions

void invalid_xml(cStrRef_t);
void invalid_xml(cStrRef_t,cStrRef_t);
void invalid_xml(cStrRef_t,cStrRef_t,cStrRef_t);
void invalid_xml(cStrRef_t,cStrRef_t,cStrRef_t,cStrRef_t);
void invalid_xml(cStrRef_t,cStrRef_t,cStrRef_t,cStrRef_t,cStrRef_t);

bool   is_bracket(cStrRef_t xml, size_t pos);
string load_xml(cStrRef_t src);
string cleanup_xml(cStrRef_t xml);
string read_next_element(cStrRef_t xml, string &key, Attributes_t &, string &body);
size_t read_next_attr(cStrRef_t xml,size_t pos,string &attr_key,string &attr_value);
//bool   parse_attr(cStrRef_t s, size_t attr_start, size_t attr_end, Attributes_t &);

typedef XMLFunc::Number      Number_t;
typedef Number_t::Type_t     NumberType_t;
typedef XMLFunc::Node        Node_t;
typedef XMLFunc::Args_t      Args_t;

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

    int index(cStrRef_t key) const
    {
      NameXref_t::const_iterator i = xref_.find(key);
      if( i == xref_.end() )
      {
        stringstream err;
        err << "Invalid argument name (" << key << ")";
        throw runtime_error(err.str());
      }

      return i->second;
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
    ConstNode(const Number_t &n) : value_(n) {}
    Number_t eval(const Args_t &args) const { return value_; }
  private:
    Number_t value_;
};

class ArgNode : public Node_t
{
  public:
    ArgNode(size_t i) : index_(i) {}
    Number_t eval(const Args_t &args) const { return args.at(index_); }
  private:
    size_t index_;
};


class UnaryNode : public Node_t
{
  public:
    UnaryNode(cStrRef_t xml, cStrRef_t key, const ArgDefs &);

    ~UnaryNode() { if(op_!=NULL) delete op_; }

  protected:
    Node_t *op_;
};

class BinaryNode : public Node_t
{
  public:
    BinaryNode(cStrRef_t xml, cStrRef_t key, const ArgDefs &);

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
    ListNode(cStrRef_t xml, cStrRef_t key, const ArgDefs &);

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
    Number_t eval(const Args_t &args) const
    {
      return op_->eval(args).negate();
    }
};

class SinNode : public UnaryNode
{
  public: 
    Number_t eval(const Args_t &args) const
    {
      return sin(double(op_->eval(args)));
    }
};

class CosNode : public UnaryNode
{
  public:
    Number_t eval(const Args_t &args) const
    {
      return cos(double(op_->eval(args)));
    }
};

class TanNode : public UnaryNode
{
  public:
    Number_t eval(const Args_t &args) const
    {
      return tan(double(op_->eval(args)));
    }
};

class AsinNode : public UnaryNode
{
  public:
    Number_t eval(const Args_t &args) const
    {
      return asin(double(op_->eval(args)));
    }
};

class AcosNode : public UnaryNode
{
  public:
    Number_t eval(const Args_t &args) const
    {
      return acos(double(op_->eval(args)));
    }
};

class AtanNode : public UnaryNode
{
  public:
    Number_t eval(const Args_t &args) const
    {
      return atan(double(op_->eval(args)));
    }
};

class DegNode : public UnaryNode
{
  public: 
    Number_t eval(const Args_t &args) const
    {
      static double f = 45./atan(1.0);
      return Number_t( f * double(op_->eval(args)) );
    }
};

class RadNode : public UnaryNode
{
  public:
    Number_t eval(const Args_t &args) const
    {
      static double f = atan(1.0)/45.;
      return Number_t( f * double(op_->eval(args)) );
    }
};

class AbsNode : public UnaryNode
{
  public:
    Number_t eval(const Args_t &args) const
    {
      return op_->eval(args).abs();
    }
};

class SqrtNode : public UnaryNode
{
  public:
    Number_t eval(const Args_t &args) const
    {
      return Number_t( sqrt(double(op_->eval(args))) );
    }
};

class ExpNode : public UnaryNode
{
  public:
    Number_t eval(const Args_t &args) const
    {
      return Number_t( exp(double(op_->eval(args))) );
    }
};

class LnNode : public UnaryNode
{
  public: 
    Number_t eval(const Args_t &args) const
    {
      return Number_t( log(double(op_->eval(args))) );
    }
};

class LogNode : public UnaryNode
{
  public:
    LogNode(cStrRef_t xml, const ArgDefs &);
    Number_t eval(const Args_t &args) const
    {
      return Number_t( fac_ * log( double(op_->eval(args))) );
    }
  private:
    double fac_;
};

class SubNode : public BinaryNode
{
  public: Number_t eval(const Args_t &args) const;
};

class DivNode : public BinaryNode
{
  public: Number_t eval(const Args_t &args) const;
};

class PowNode : public BinaryNode
{
  public: Number_t eval(const Args_t &args) const;
};

class ModNode : public BinaryNode
{
  public: Number_t eval(const Args_t &args) const;
};

class Atan2Node : public BinaryNode
{
  public: Number_t eval(const Args_t &args) const;
};

class AddNode : public ListNode
{
  public: Number_t eval(const Args_t &args) const;
};

class MultNode : public ListNode
{
  public: Number_t eval(const Args_t &args) const;
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
  string       body;

  xml = read_next_element(xml,key,attr,body);

  if(key.empty() || key!="arglist")
    throw runtime_error("XML does not contain a valid <arglist>");
  if(body.empty())
    throw runtime_error("<arglist> contains no <arg> entries");

  ArgDefs argDefs;

  while(body.empty()==false)
  {
    string arg_body;
    body = read_next_element(body,key,attr,arg_body);

    if(key.empty() || key!="arg")
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
  }

  // WORK HERE
  //   Add logic to parse the Nodes!!!
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

UnaryNode::UnaryNode(cStrRef_t xml, cStrRef_t key, const ArgDefs &argDefs)
{
}

BinaryNode::BinaryNode(cStrRef_t xml, cStrRef_t key, const ArgDefs &argDefs)
{
}

ListNode::ListNode(cStrRef_t xml, cStrRef_t key, const ArgDefs &argDefs)
{
}

LogNode::LogNode(cStrRef_t xml, const ArgDefs &argDefs)
  : UnaryNode(xml,"log",argDefs)
{
}

Number_t SubNode::eval(const Args_t &args) const
{
  Number_t v1 = op1_->eval(args);
  Number_t v2 = op2_->eval(args);

  return 
    ( v1.isInteger() && v2.isInteger() )
    ? Number_t( int(v1) - int(v2) )
    : Number_t( double(v1) - double(v2) ) ;
}

Number_t DivNode::eval(const Args_t &args) const
{
  Number_t v1 = op1_->eval(args);
  Number_t v2 = op2_->eval(args);

  return 
    ( v1.isInteger() && v2.isInteger() )
    ? Number_t( int(v1) / int(v2) )
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
    ? Number_t( int(v1) % int(v2) )
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
  int    isum(0);
  double dsum(0.);

  bool isInteger = true;

  for(vector<Node_t *>::const_iterator op=ops_.begin(); op!=ops_.end(); ++op)
  {
    Number_t t = (*op)->eval(args);

    isInteger = isInteger && t.isInteger();

    isum += int(t);
    dsum += double(t);
  }

  return isInteger ? Number_t(isum) : Number_t(dsum);
}

Number_t MultNode::eval(const Args_t &args) const
{
  int    iprod(1);
  double dprod(1.);

  bool isInteger = true;

  for(vector<Node_t *>::const_iterator op = ops_.begin(); op != ops_.end(); ++op)
  {
    Number_t t = (*op)->eval(args);

    isInteger = isInteger && t.isInteger();

    iprod *= int(t);
    dprod *= double(t);
  }

  return isInteger ? Number_t(iprod) : Number_t(dprod);
}

////////////////////////////////////////////////////////////////////////////////
// Support functions
////////////////////////////////////////////////////////////////////////////////

void invalid_xml(cStrRef_t s1,cStrRef_t s2,cStrRef_t s3,cStrRef_t s4,cStrRef_t s5) { invalid_xml(s1+" "+s2,s3,s4,s5); }
void invalid_xml(cStrRef_t s1,cStrRef_t s2,cStrRef_t s3,cStrRef_t s4)              { invalid_xml(s1+" "+s2,s3,s4);    }
void invalid_xml(cStrRef_t s1,cStrRef_t s2,cStrRef_t s3)                           { invalid_xml(s1+" "+s2,s3);       }
void invalid_xml(cStrRef_t s1,cStrRef_t s2)                                        { invalid_xml(s1+" "+s2);          }

void invalid_xml(cStrRef_t msg)
{
  string err = "Invalid XMLFunc: ";
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
string read_next_element(cStrRef_t xml, string &key, Attributes_t &attr, string &body)
{
  key = "";
  body = "";
  attr.clear();

  const size_t start_elem = xml.find("<");

  // if no "<" is found, then there are no more tags
  if(start_elem == string::npos) return xml;

  // determine the key for this element

  const size_t start_key = start_elem+1;

  size_t pos = start_key;

  while(pos<xml.length())
  {
    if( isspace(xml[pos]) || is_bracket(xml,pos) )
    {
      key = xml.substr(start_key,pos-start_key);
      break;
    }
    ++pos;
  }

  // Find the closing '>' or '/>', parsing attributes along the way
  //
  // There are three ways out of the following while loop
  //   - finding '/>' indicates an empty-body tag: exit is return from this function
  //   - finding '>'  indicates an opening tag: while loop is exited with a break;
  //   - finding neither: the loop exits when it runs out of string to parse (an exception will be thrown)

  size_t body_start(0);

  while( pos<xml.length() )
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
      if(pos==xml.length()-1 || xml[pos+1] != '>') invalid_xml("Missing closing '>' for",key);

      // We can return immediately after stripping out current tag
      return xml.substr(0,start_elem).append(xml.substr(pos+2));
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

  if(body_start==0) invalid_xml("Missing closing bracket for",key,"tag");

  // Here comes the tricky part... we need to find the closing tag.
  //   We need to handle nested tags including open/close pairs and empty-body tags
  //   We need to ignore anything in quotes

  char q(0);          // current quote char (' or ")
  int  depth(1);      // current nested tag depth
  bool in_tag(false); 
  bool closing_tag(false);

  size_t body_end(0);

  while( pos<xml.length() )
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
        if(closing_tag) --depth;  // closing tag
        else            ++depth;  // opening tag
        in_tag = false;

        if(depth==0) // exited closing tag for element we're reading
        {
          ++pos;
          break;
        }
      }
      else if(c=='/') // empty-body tag
      {
        if(pos+1 >= xml.length()) invalid_xml("Incomplete body for",key,"tag");
        if(xml[++pos]!='>')       invalid_xml("/ without /> in",key,"tag");
        in_tag = false;
      }
    }
    else if(c=='<') // starting new tag (don't know yet what type)
    {
      in_tag = true;
      if(pos+1 >= xml.length()) invalid_xml("Incomplete body for",key,"tag");

      closing_tag = (xml[pos+1]=='/');

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

  return xml.substr(0,start_elem).append(xml.substr(pos));
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


//{
//  size_t pos(0);
//  size_t attr_start(0);
//  size_t attr_end(0);
//
//  bool found(false);
//  while(found == false)
//  {
//    pos = s.find("<",pos);
//    if( pos == string::npos ) return string::npos;
//
//    pos += 1;
//
//    if( s.substr(pos,pos+tag.length()) == tag )
//    {
//      pos += tag.length();
//      attr_start = pos;
//
//      pos = s.find("/>",pos);
//      if( pos != string::npos )
//      {
//        attr_end = pos;
//        pos += 2;
//      }
//      else
//      {
//        pos = s.find(">",pos);
//        if( pos == string::npos) return string::npos;
//
//        attr_end = pos;
//        pos += 1;
//
//        size_t value_end = s.find(string("</")+tag+">");
//        if( value_end == string::npos ) return string::npos;
//
//        value = s.substr(pos,value_end);
//        pos = value_end + tag.length() + 3;
//      }
//
//      if( parse_attr( s, attr_start, attr_end, attr ) == false ) return string::npos;
//
//      found = true;
//    }
//  }
//
//  return pos;
//}
//
//bool parse_attr(cStrRef_t s, size_t attr_start, size_t attr_end, Attributes_t &attr)
//{
//  size_t a = attr_start;
//  while(a < attr_end)
//  {
//    while(isspace(s[a]))
//    {
//      ++a;
//      if( a >= attr_end ) return true;
//    }
//
//    if(isalpha(s[a])==false) return false;
//
//    size_t b = a+1;
//    while(isalnum(s[b]))
//    {
//      ++b;
//      if( b>=attr_end ) return false;
//    }
//
//    string key=s.substr(a,b);
//
//    a = b;
//    while(isspace(s[a]))
//    {
//      ++a;
//      if( a >= attr_end ) return false;
//    }
//
//    if(s[a] != '=') return false;
//
//    while(isspace(s[a]))
//    {
//      ++a;
//      if( a >= attr_end ) return false;
//    }
//
//    char q = s[a];
//    if(q!='"' && q!=''') return false;
//
//    ++a;
//
//    b = a+1;
//    while( s[b] != q )
//    {
//      ++b;
//      if(b >= attr_end) return false;
//    }
//
//    attr[key] = s.substr(a,b);
//
//    a=b+1;
//  }
//
//  return true;
//}
