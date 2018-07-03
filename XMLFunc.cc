#include "XMLFunc.h"

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

using namespace std;

typedef map<string,string> Attributes_t;

// prototypes for support functions

string  cleanup_xml(const string &xml);
size_t  read_tag(const string &s, const string &tag, Attributes_t &, string &value);
bool    parse_attr(const string &s, size_t attr_start, size_t attr_end, Attributes_t &);

typedef XMLFunc::Number      Number_t;
typedef XMLFunc::Node        Node_t;
typedef XMLFunc::NodeFactory NodeFactory_t;

typedef vector<Number_t> &Args_t;


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
    UnaryNode(const string &xml, const string &tag, const NodeFactory_t &);

    ~UnaryNode() { if(op_!=NULL) delete op_; }

  private:
    Node_t *op_;
};

class BinaryNode : public Node_t
{
  public:
    BinaryNode(const string &xml, const string &tag, const NodeFactory_t &);

    ~BinaryNode() 
    { 
      if(op1_!=NULL) delete op1_;
      if(op2_!=NULL) delete op2_;
    }

  private:
    Node_t *op1_;
    Node_t *op2_;
};

class ListNode : public Node_t
{
  public:
    ListNode(const string &xml, const string &tag, const NodeFactory_t &factory);

    ~ListNode() 
    { 
      for(vector<Node_t *>::iterator i=ops_.begin(); i!=ops_.end(); ++i)
      {
        delete *i;
      }
    }

  private:
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
  public: Number_t eval(const Args_t &args) const;
};

class CosNode : public UnaryNode
{
  public: Number_t eval(const Args_t &args) const;
};

class TanNode : public UnaryNode
{
  public: Number_t eval(const Args_t &args) const;
};

class AsinNode : public UnaryNode
{
  public: Number_t eval(const Args_t &args) const;
};

class AcosNode : public UnaryNode
{
  public: Number_t eval(const Args_t &args) const;
};

class AtanNode : public UnaryNode
{
  public: Number_t eval(const Args_t &args) const;
};

class DegNode : public UnaryNode
{
  public: Number_t eval(const Args_t &args) const;
};

class RadNode : public UnaryNode
{
  public: Number_t eval(const Args_t &args) const;
};

class AbsNode : public UnaryNode
{
  public: Number_t eval(const Args_t &args) const;
};

class SqrtNode : public UnaryNode
{
  public: Number_t eval(const Args_t &args) const;
};

class ExpNode : public UnaryNode
{
  public: Number_t eval(const Args_t &args) const;
};

class LnNode : public UnaryNode
{
  public: Number_t eval(const Args_t &args) const;
};

class LogNode : public UnaryNode
{
  public:
    LogNode(const string &xml, const NodeFactory_t &factory);
    Number_t eval(const Args_t &args) const;
  private:
    Number_t fac_;
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

XMLFunc::XMLFunc(const string &xml_src) : root_(NULL)
{
  string xml = xml_src;
  // Let's see if xml is actually a file.  If so, load it.

  struct stat xml_stat;
  if( stat( xml_src.c_str(), &xml_stat ) == 0 )
  {
    int fd = open(xml.c_str(), O_RDONLY);

    if(fd<0)
    {
      stringstream err;
      err << "Failed to open " << xml << ": " << strerror(errno);
      throw runtime_error(err.str());
    }

    off_t eof = lseek(fd,0,SEEK_END);
    lseek(fd,0,SEEK_SET);

    char *buffer = new char[1+eof];
    int toRead = eof;
    char *pos = buffer;
    while(toRead)
    {
      ssize_t nread = read(fd,pos,toRead);
      if(nread < 0)
      {
        stringstream err;
        err << "Failed to read " << xml << ": " << strerror(errno);
        throw runtime_error(err.str());
      }

      toRead -= nread;
      pos    += nread;
    }
    *pos = '\0';

    xml = buffer;
    delete[] buffer;
  }

  xml = cleanup_xml(xml);

  // Extract the arglist

  Attributes_t attr;
  string       args;
  size_t func_start = read_tag(xml,"arglist",attr,args);
  if(func_start == string::npos) throw runtime_error("XML does not contain a valid <arglist>");

  while(true)
  {
    string value;
    size_t next_arg = read_tag(args,"arg",attr,value);

    if(next_arg==string::npos) break;

    if(value.empty() == false)
      throw runtime_error("Invalid XML: The <arg> tag does not expect a value");

    ArgInfo info;

    for(Attributes_t::iterator i=attr.begin(); i!=attr.end(); ++i)
    {
      if(i->first == "name")
      {
        info.name = i->second;
      }
      else if(i->first == "type")
      {
        if     (i->second == "integer") { info.type = Number::Integer;  }
        else if(i->second == "double")  { info.type = Number::Double;   }
        else throw runtime_error("Invalid XML: argument type must be either 'integer' or 'double'");
      }
      else
      {
        throw runtime_error("Invalid XML: argument attributes must be either 'name' or 'type'");
      }
    }

    argList_.push_back(info);
    args = args.substr(next_arg);
  }

  // Build the root node

  NodeFactory factory(argList_);
  root_ = factory.build(xml.substr(func_start));
}

XMLFunc::Number XMLFunc::eval(const std::vector<XMLFunc::Number> &args) const
{
  if( args.size() < argList_.size() )
  {
    stringstream err;
    err << "Insufficient arguments passed to eval.  Need " << argList_.size() << ". Only " << args.size() << " were provided";
    throw runtime_error(err.str());
  }

  return root_->eval(args);
}

XMLFunc::Number XMLFunc::eval(const XMLFunc::Number *argArray) const
{
  // No exception will be thrown if argArray doesn't contain
  //   enough elements... which may lead to unpredictable results
  //   including the real possiblity of a segmentation fault

  std::vector<XMLFunc::Number> args;
  for(int i=0; i<argList_.size(); ++i)
  {
    args.push_back(argArray[i]);
  }

  return root_->eval(args);
}

////////////////////////////////////////////////////////////////////////////////
// XMLFunc::Node subclass methods
////////////////////////////////////////////////////////////////////////////////

UnaryNode::UnaryNode(const string &xml, const string &tag, const NodeFactory_t &factory)
{
}

BinaryNode::BinaryNode(const string &xml, const string &tag, const NodeFactory_t &factory)
{
}

ListNode::ListNode(const string &xml, const string &tag, const NodeFactory_t &factory)
{
}

Number_t SinNode::eval(const Args_t &args) const
{
  return sin(double(op_->eval(args)));
}

Number_t CosNode::eval(const Args_t &args) const
{
  return cos(double(op_->eval(args)));
}

Number_t TanNode::eval(const Args_t &args) const
{
  return tan(double(op_->eval(args)));
}

Number_t AsinNode::eval(const Args_t &args) const
{
  return asin(double(op_->eval(args)));
}

Number_t AcosNode::eval(const Args_t &args) const
{
  return acos(double(op_->eval(args)));
}

Number_t AtanNode::eval(const Args_t &args) const
{
  return atan(double(op_->eval(args)));
}

Number_t DegNode::eval(const Args_t &args) const
{
  static double f = 45./atan(1.0);
  return Number_t( f * double(op_->eval(args)) );
}

Number_t RadNode::eval(const Args_t &args) const
{
  static double f = atan(1.0)/45.;
  return Number_t( f * double(op_->eval(args)) );
}

Number_t AbsNode::eval(const Args_t &args) const
{
  return op_->eval(args).abs();
}

Number_t SqrtNode::eval(const Args_t &args) const
{
  retun Number_t( sqrt(double(op_->eval(args))) );
}

Number_t ExpNode::eval(const Args_t &args) const
{
  return Number_t( exp(double(op_->eval(args))) );
}

Number_t LnNode::eval(const Args_t &args) const
{
  return Number_t( log(double(op_->eval(args))) );
}

Number_t LogNode::eval(const Args_t &args) const
{
  return Number_t( fac_ * log( double(op_->eval(args))) );
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
    : Number_t( modf(double(v1),double(v2)) ) ;
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

  bool isInteger = v1.isInteger() && v2.isInteger();

  for(vector<Node_t *>::iterator op = ops_.begin(); op != ops_.end(); ++op)
  {
    Number_t t = op->eval(args);

    isum += int(t);
    dsum += double(t);
  }

  return isInteger ? Number_t(isum) : Number_t(dsum);
}

Number_t MultNode::eval(const Args_t &args) const
{
  int    iprod(1);
  double dprod(1.);

  bool isInteger = v1.isInteger() && v2.isInteger();

  for(vector<Node_t *>::iterator op = ops_.begin(); op != ops_.end(); ++op)
  {
    Number_t t = op->eval(args);

    isum *= int(t);
    dsum *= double(t);
  }

  return isInteger ? Number_t(isum) : Number_t(dsum);
}

////////////////////////////////////////////////////////////////////////////////
// XMLFunc::Factory methods
////////////////////////////////////////////////////////////////////////////////

XMLFunc::Node *XMLFunc::NodeFactory::buildNode(const std::string &xml)
{
}


////////////////////////////////////////////////////////////////////////////////
// Support functions
////////////////////////////////////////////////////////////////////////////////

// Removes XML declaration and comments from XML
string cleanup_xml(const string &orig_xml)
{
  string xml = orig_xml;

  // remove XML prolog(s)
  //   should only be one, but removes all just in case

  char q = '\0';
  size_t start_del(-1);
  size_t start_tag(-1);
  for(size_t i=0; i<xml.length(); ++i)
  {
    char c = xml[i];

    if(q) // in quote
    {
      if(c==q) q='\0';  // exit quote
    }
    else if(c=='"' || c==''') // enter quote
    {
      q=c;
    }
    else if(xml.compare(i,5,"<?xml")==0)
    {
      if(start_del>=0) throw runtime_error("Invalid XML: nested <?xml tags");
      start_del = i;
    }
    else if(xml.compare(i,2,"?>")==0)
    {
      if(start_del<0) throw runtime_error("Invalid XML: ?> found without <?xml");
      xml.erase(start_del, i+2 - start_del);
      start_del = -1;
      i -= 1;
    }
    if(c=='<')
    {
      if(start_del >= 0 || start_tag >= 0)
        throw runtime_error("Invalid XML: '<' found unquoted within a tag");
      start_tag = i;
    }
    else if(c=='>')
      // WORK HERE
    {
      throw runtime_error("Invalid XML: nested tags within <?xml ... ?>");
    }
  }

  // remove comments
  
  start_del=0;
  while(start_del != string::npos)
  {
    start_del = xml.find("<!--");
    if( start_del != string::npos )
    {
      size_t end_del = xml.find("-->");
      if(end_del==string::npos)
        throw runtime_error("Invalid XML: <!-- is missing closing -->");

      xml.erase(start_del, end_del + 3 - start_del);
    }
  }

  return xml;
}


// Searches the specified string for the specified tag.
//   Populates the attributes map and value string based on content of the located tag
//   Returns the position AFTER the closing tag
//   Returns string::npos on failure to locate the tag
size_t read_tag(const string &s, const string &tag, Attributes_t &attr, string &value)
{
  size_t pos(0);
  size_t attr_start(0);
  size_t attr_end(0);

  bool found(false);
  while(found == false)
  {
    pos = s.find("<",pos);
    if( pos == string::npos ) return string::npos;

    pos += 1;

    if( s.substr(pos,pos+tag.length()) == tag )
    {
      pos += tag.length();
      attr_start = pos;

      pos = s.find("/>",pos);
      if( pos != string::npos )
      {
        attr_end = pos;
        pos += 2;
      }
      else
      {
        pos = s.find(">",pos);
        if( pos == string::npos) return string::npos;

        attr_end = pos;
        pos += 1;

        size_t value_end = s.find(string("</")+tag+">");
        if( value_end == string::npos ) return string::npos;

        value = s.substr(pos,value_end);
        pos = value_end + tag.length() + 3;
      }

      if( parse_attr( s, attr_start, attr_end, attr ) == false ) return string::npos;

      found = true;
    }
  }

  return pos;
}

bool parse_attr(const string &s, size_t attr_start, size_t attr_end, Attributes_t &attr)
{
  size_t a = attr_start;
  while(a < attr_end)
  {
    while(isspace(s[a]))
    {
      ++a;
      if( a >= attr_end ) return true;
    }

    if(isalpha(s[a])==false) return false;

    size_t b = a+1;
    while(isalnum(s[b]))
    {
      ++b;
      if( b>=attr_end ) return false;
    }

    string key=s.substr(a,b);

    a = b;
    while(isspace(s[a]))
    {
      ++a;
      if( a >= attr_end ) return false;
    }

    if(s[a] != '=') return false;

    while(isspace(s[a]))
    {
      ++a;
      if( a >= attr_end ) return false;
    }

    char q = s[a];
    if(q!='"' && q!=''') return false;

    ++a;

    b = a+1;
    while( s[b] != q )
    {
      ++b;
      if(b >= attr_end) return false;
    }

    attr[key] = s.substr(a,b);

    a=b+1;
  }

  return true;
}
