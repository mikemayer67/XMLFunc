#include <XMLFunc.h>

#include <fstream>
#include <stdexcept>

#include <sys/stat.h>
#include <errno.h>
#include <string.h>

using namespace std;
using namespace XMLFuncImp;

typedef map<string,string> Attributes_t;

// prototypes for support functions

size_t read_tag(const string &s, const string &tag, Attributes_t &, string &value);
bool   parse_attr(const string &s, size_t attr_start, size_t attr_end, Attributes_t &);


// XMLFunc constructor

XMLFunc::XMLFunc(string xml) : root_(NULL)
{
  // Let's see if xml is actually a file.  If so, load it.

  struct stat xml_stat;
  if( stat( io_xml.c_str(), &xml_stat ) == 0 )
  {
    int fd = open(xml.c_str(), O_RDONLY);

    if(fd<0)
    {
      stringstream err;
      err << "Failed to open " << xml << ": " << strerror(errno);
      throw runtime_error(err.str());
    }

    off_t eof = lseek(fd,0,SEEK_END);
    lseek(fd,0,SEED_SET);

    char *buffer = new char[1+eof];
    int toRead = eof;
    char *pos = buffer;
    while(toRead)
    {
      size_t nread = read(fd,pos,toRead);
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

  // Extract the arglist

  Attributes_t attr;
  string       args;
  size_t func_start = read_tag(xml,"arglist",attr,args)
  if(func_start == string::npos) throw runtime_error("XML does not contain a valid <arglist>");

  string value;
  size_t next_arg = read_tag(args,"arg",attr,value);
  while(next_arg != string::npos)
  {
    XMLFuncImp::ArgInfo info;

    for(Attributes_t::iterator i=attr.begin(); i!=attr.end(); ++i)
    {
      if(i->first == "name")
      {
        info.name = i->second;
      }
      else if(i->first == "type")
      {
        if     (i->second == "integer") { info.isInteger = true;  }
        else if(i->second == "double")  { info.isIntger  = false; }
        else throw runtime_error("Invalid XML: argument type must be either 'integer' or 'double'");
      }
      else
      {
        throw runtime_error("Invalid XML: argument attributes must be either 'name' or 'type'");
      }
    }

    size_t next_arg = read_tag(substr(args,next_arg),"arg",attr,value);
  }

  // Build the root node

  root_ = Node::build(func_start)

}

XMLFunc::Number eval(const std::vector<XMLFunc::Number> &args) const
{
  if( args.size() < argList_.size() )
  {
    stringstream err;
    err << "Insufficient arguments passed to eval.  Need " << argList_.size() << ". Only " << args.size() << " were provided";
    throw runtime_error(err.str());
  }

  return root_->eval(args);
}

XMLFunc::Number eval(const XMLFunc::Number *argArray) const
{
  // No exception will be thrown if argArray doesn't contain
  //   enough elements... 

  std::vector<XMLFunc::Number> args;
  for(int i=0; i<argList_.size(); ++i)
  {
    args.push_back(argArray[i]);
  }

  return root_->eval(args);
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

    if( s.substr(pos,pos+4) == "?xml" )
    {
      pos += 4;
      pos = s.find(">",pos);
      if( pos == string::npos ) return string::npos;
      pos += 1;
    }
    else if( s.substr(pos,pos+3) == "!--" )
    {
      pos += 3;
      pos = s.find("-->",pos);
      if( pos == string::npos ) return string::npos;
      pos += 3;
    }
    else if( s.substr(pos,pos+tag.length()) == tag )
    {
      pos += tag.length();
      attr_start = pos;

      pos = s.find("/>",pos));
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

    if(s[a] != '"') return false;

    ++a;

    b = a+1;
    while( s[b] != '"' )
    {
      ++b;
      if(b >= attr_end) return false;
    }

    attr[key] = s.substr(a,b);

    a=b+1;
  }

  return true;
}
