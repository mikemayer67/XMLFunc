#ifndef _XMLFUNC_H_
#define _XMLFUNC_H_

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
        Number(void)     : type_(Double),  ival_(0), dval_(0.0) {}
        /// \brief integer constructor
        Number(long v)   : type_(Integer), ival_(v), dval_(double(v)) {}
        /// \brief double constructor
        Number(double v) : type_(Double),  ival_(int(v)), dval_(v) {}
        /// \brief copy constructor
        Number(const Number &x) : type_(x.type_), ival_(x.ival_), dval_(x.dval_) {}
      
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

      private:
        /// \cond PRIVATE
        Type_t type_;
        long   ival_;
        double dval_;
        /// \endcond
    };

    typedef std::vector<Number> Args_t;

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

    virtual ~XMLFunc() { if( root_ != NULL ) delete root_; }

    /*!
     * \brief Invocation method
     *
     * \param args - list of values being passed to the function.
     *
     * \warning The length of the list must match or exceed the number of arguments identified in
     *   the <arglist> element in the XML or a std::lenth_error exeption will be thrown.
     */
    Number eval(const Args_t &args) const;

  public: // making these public allows Op subclasses to exist outside XMLFunc scope

    /*!
     * \class XMLFunc::Op
     * \brief value node that performs a unary, binary, or list operation/function
     *
     * This is an abstract base class for all operator nodes.  There are a number of
     * built-in subclasses that perform most of the standard mathematical operations.
     * If additional subclasses are needed, code will need to be added in XMLFunc.cpp
     */
    class Op
    {
      /*!
       * The constructor is protected so that only subclasses can be instantiated
       */
      protected:
        Op(void) {}

      public:
        virtual ~Op() {}

      /*!
       * Evaluates and returns the value of the element node as defined by the
       * input XML file (or subset thereof)
       *
       * \param args is the list of argument values passed to the function being evaluated.
       */
      public:
        virtual XMLFunc::Number eval(const Args_t &args) const = 0;
    };

  private:

    ////////////////////////////////////////////////////////////
    // As all of the attributes are only used internally, no
    //   doxygen style documentation is provided
    ////////////////////////////////////////////////////////////
    /// \cond PRIVATE
    ////////////////////////////////////////////////////////////

    Op    *root_;
    int    numArgs_;

    /// \endcond
};

#endif // _XMLFUNC_h_
