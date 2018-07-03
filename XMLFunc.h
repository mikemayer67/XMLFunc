#ifndef _XMLFUNC_H_
#define _XMLFUNC_H_

#include <vector>
#include <string>

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
        /// \brief integer constructor
        Number(int v)    : type_(Integer), int_(v), double_(double(v)) {}
        /// \brief double constructor
        Number(double v) : type_(Double),  int_(int(v)), double_(v) {}
        
        /// \brief integer cast operator
        operator int(void)    const { return int_;    }
        /// \brief double cast operator
        operator double(void) const { return double_; }

        /// \brief Integer or Double
        Type_t type(void) const { return type_; }

        /// \brief Returns true if type is Integer
        bool isInteger(void) const { return type_ == Integer; }

        /// \brief Changes value to negative of current value
        void negate(void) { int_ = -int_; double_ = -double_; }

        /// \brief Changes value to absolute value of current value
        void abs(void) { int_ = abs(int_); double_ = fabs(double_); }

      private:
        /// \cond PRIVATE
        Type_t type_;
        int    int_;
        double double_;
        /// \endcond
    };

    class Node;         // defined below
    class NodeFactory;  // defined below

    struct ArgInfo
    {
      Number::Type_t type;
      std::string    name;

      ArgInfo(void) : type(Number::Double) {}
    };

    typedef std::vector<ArgInfo>      ArgList_t;
    typedef ArgList_t::const_iterator ArgIter_t;

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
     * \brief Invocation form 1
     *
     * \param args - list of values being passed to the function.  
     *
     * \warning The length of the list must match or exceed the number of arguments identified in 
     *   the <arglist> element in the XML or a std::lenth_error exeption will be thrown.
     */
    Number eval(const std::vector<Number> &args) const;

    /*!
     * \brief Invocation form 2
     *
     * \param args - list of values being passed to the function.
     *   
     * \warning The length of the array must be sufficient to cover all of the arguments identified
     *   in the <arglist> element of the XML.  Because this cannot be verified at runtime, failure
     *   to enforce this may result in unpredictable results, including the very real possibility
     *   of a segmentation fault.
     */
    Number eval(const Number *args) const;

  public:

    /*!
     * \class XMLFunc::Node
     * \brief value node that performs a unary, binary, or list operation/function
     *
     * This is an abstract base class for all operator nodes.  There are a number of
     * built-in subclasses that perform most of the standard mathematical operations.
     * If additional subclasses are needed, code will need to be added in XMLFunc.cpp
     */
    class Node
    {
      /*!
       * The constructor is protected so that only subclasses can be instantiated
       *
       * \param xml contains the XML definition of the nodes' function
       * \param factory references the XML::NodeFactory being used to construct
       *   the XML function. (Needed for building subnodes).
       */
      protected:
        Node() {}

      public:
        virtual ~Node() {}

      /*!
       * Evaluates and returns the value of the element node as defined by the
       * input XML file (or subset thereof)
       *
       * \param args is the list of argument values passed to the function being evaluated.
       */ 
      public:
        virtual XMLFunc::Number eval(const std::vector<XMLFunc::Number> &args) const = 0;
    };

    /*!
     * \class XMLFunc::NodeFactory
     * \brief Converts XML into XMLFunc::Node objects
     */
    class NodeFactory
    {
      public:
        /*!
         * \brief Constructor
         * \param args defines the argument types and names used by the XML function
         */
        NodeFactory(const ArgList_t &args) : args_(args) {}

        /*!
         * \brief Node constructor
         *
         * Parses the XML and constructs an object of the correct Node subclass.
         */

        Node *buildNode(const std::string &xml);

      private:
        /// \cond PRIVATE
        ArgList_t args_;
        /// \endcond
    };

  private:

    ////////////////////////////////////////////////////////////
    // As all of the attributes are only used internally, no
    //   doxygen style documentation is provided
    ////////////////////////////////////////////////////////////
    /// \cond PRIVATE
    ////////////////////////////////////////////////////////////

    Node      *root_;
    ArgList_t  argList_;

    /// \endcond
};

#endif // _XMLFUNC_h_
