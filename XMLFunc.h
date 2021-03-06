#ifndef _XMLFUNC_H_
#define _XMLFUNC_H_

#include <ostream>
#include <vector>
#include <map>
#include <string>
#include <cmath>

/*!
 * \class XMLFunc
 * \brief XML function parser/evaluator
 *
 * XMLFunc is a C++ class that implments a mathematical function (of fairly arbitrary complexity)
 * given a description of that function in XML.
 */

class XMLFunc
{
  public:

    typedef std::map<std::string,size_t>  Xref_t;
    typedef std::pair<std::string,size_t> XrefEntry_t;

    /*!
     * \class XMLFunc::Number
     * \brief integer or double value
     *
     * Objects of this class can be used to pass/store arguments that may be
     * either double or integer values.  Each object "knows" which type it
     * contains based on its constructor.
     */

    class Number
    {
      public:
        typedef enum {Integer, Double} Type_t;

      public:
        /// \brief default constructor (0.0)
        Number(void)             : type_(Double),  ival_(0), dval_(0.0) {}
        /// \brief integer constructor
        Number(long v)           : type_(Integer), ival_(v), dval_(double(v)) {}
        /// \brief integer constructor
        Number(int v)            : type_(Integer), ival_(v), dval_(double(v)) {}
        /// \brief integer constructor
        Number(short v)          : type_(Integer), ival_(v), dval_(double(v)) {}
        /// \brief integer constructor
        Number(unsigned long v)  : type_(Integer), ival_(v), dval_(double(v)) {}
        /// \brief integer constructor
        Number(unsigned int v)   : type_(Integer), ival_(v), dval_(double(v)) {}
        /// \brief integer constructor
        Number(unsigned short v) : type_(Integer), ival_(v), dval_(double(v)) {}
        /// \brief double constructor
        Number(double v)         : type_(Double),  ival_(int(v)), dval_(v) {}
        /// \brief double constructor
        Number(float v)          : type_(Double),  ival_(int(v)), dval_(v) {}
        /// \brief copy constructor
        Number(const Number &x)  : type_(x.type_), ival_(x.ival_), dval_(x.dval_) {}
      
        /// \brief integer cast operator
        operator long(void)   const { return ival_;    }
        /// \brief double cast operator
        operator double(void) const { return dval_; }

        /// \brief Integer or Double
        Type_t type(void) const { return type_; }

        /// \brief Returns true if type is Integer
        bool isInteger(void) const { return type_ == Integer; }
        bool isDouble(void)  const { return type_ == Double;  }

        /// \brief Changes value to negative of current value
        const Number &negate(void) 
        { 
          ival_ = -ival_; 
          dval_ = -dval_; 
          return *this; 
        }

        /// \brief Changes value to absolute value of current value
        const Number &abs(void) 
        { 
          ival_    = std::abs(ival_); 
          dval_ = std::fabs(dval_); 
          return *this; 
        }

        void write(std::ostream &s) const
        {
          if(type_ == Integer) { s << ival_; }
          else                 { s << dval_; }
        }

      private:
        /// \cond PRIVATE
        Type_t type_;
        long   ival_;
        double dval_;
        /// \endcond
    };

    /*! 
     * \class XMLFunc::Args
     * \brief list of XML::Number objects, passed to eval calls
     *
     * This is nothing more than an extention of std::vector that adds the
     * add() method as a synonym to push_bak
     */

    class Args : public std::vector<Number>
    {
      public:
        void add(const Number &v) { push_back(v); }
    };


  public:

    /*!
     * \brief Constructor
     *
     * \param xml - may be either the path to a file containing XML or a string containing the XML
     *
     * \warning If a file path is provided, but that file cannot be read, a std::runtime_error
     *   exception will be thrown.
     *
     * \warning If the XML cannot be parsed, a std::runtime_error exception will be thrown.
     */
    XMLFunc(const std::string &xml);

    virtual ~XMLFunc() 
    { 
      for(std::vector<Function>::iterator i=funcs_.begin(); i!=funcs_.end(); ++i)
      {
        delete i->root;
      }
    }

    /*!
     * \brief Invocation method when only one function is defined
     *
     * \param args - list of values being passed to the function.
     *
     * \warning The length of the list must match or exceed the number of arguments identified in
     *   the <arglist> element in the XML or a std::lenth_error exeption will be thrown.
     */
    Number eval(const Args &args) const;

    /*!
     * \brief Invocation method specifying function by function (0 based) index
     *
     * \param args - list of values being passed to the function.
     *
     * \warning The length of the list must match or exceed the number of arguments identified in
     *   the <arglist> element in the XML or a std::lenth_error exeption will be thrown.
     */
    Number eval(size_t index, const Args &args) const;

    /*!
     * \brief Invocation method specifying function by name
     *
     * \param args - list of values being passed to the function.
     *
     * \warning The length of the list must match or exceed the number of arguments identified in
     *   the <arglist> element in the XML or a std::lenth_error exeption will be thrown.
     */
    Number eval(const std::string &name, const Args &args) const;

  public: // making these public allows Operation subclasses to exist outside XMLFunc scope

    /*!
     * \class XMLFunc::Operation
     * \brief value node that performs a unary, binary, or list operation/function
     *
     * This is an abstract base class for all operator nodes.  There are a number of
     * built-in subclasses that perform most of the standard mathematical operations.
     * If additional subclasses are needed, code will need to be added in XMLFunc.cpp
     */
    class Operation
    {
      /*!
       * The constructor is protected so that only subclasses can be instantiated
       */
      protected:
        Operation(void) {}

      public:
        virtual ~Operation() {}

      /*!
       * Evaluates and returns the value of the element node as defined by the
       * input XML file (or subset thereof)
       *
       * \param args is the list of argument values passed to the function being evaluated.
       */
      public:
        virtual XMLFunc::Number eval(const Args &args) const = 0;
    };

    ////////////////////////////////////////////////////////////
    // The ArgDefs class and all of attributes are only used 
    //   internally, no doxygen style documentation is provided
    ////////////////////////////////////////////////////////////
    /// \cond PRIVATE
    ////////////////////////////////////////////////////////////

  public: // to be useable by Operation and its subclasses

    class ArgDefs
    {
      public:
        ArgDefs(void) {}

        void add(Number::Type_t type, const std::string &name="");

        bool empty (void) const { return types_.empty(); }
        int  count (void) const { return int(types_.size()); }

        Number::Type_t type  (int i) const { return types_.at(i);  }

        size_t index(const std::string &name) const;

        std::pair<size_t,bool> find(const std::string &name) const;

        void clear(void) { types_.clear(); xref_.clear(); }

      private:

        std::vector<Number::Type_t> types_;
        Xref_t                      xref_;
    };

  private:

    struct Function
    {
      ArgDefs    argDefs;
      Operation *root;
      Function(void) : root(NULL) {}
      Function(const ArgDefs &a, Operation *o) : argDefs(a), root(o) {}
      Function(Operation *o, const ArgDefs &a) : argDefs(a), root(o) {}
    };

    Number _eval(const Function &, const Args &args) const;

  private:

    std::vector<Function> funcs_;
    Xref_t                funcXref_;

    /// \endcond
};

static std::ostream &operator<<(std::ostream &s,const XMLFunc::Number &x) { x.write(s); return s; }

#endif // _XMLFUNC_h_
