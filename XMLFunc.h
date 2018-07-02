#ifndef _XMLFUNC_h_
#define _XMLFUNC_H_

#include <XMLFuncImp.h>

#include <vector>

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
        Number(int v)    : int_(v), double_(double(v)) {}  ///< integer constructor
        Number(double v) : int_(int(v)), double_(v) {}     ///< double constructor
        
        operator int(void)    const { return int_;    }    ///< integer cast operator
        operator double(void) const { return double_; }    ///< double cast operator

      private:
        int    int_;
        double double_;
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

    /*!
     * \brief Evaluation operator form 1
     *
     * \param args - list of values being passed to the function.  
     *
     * \warning The length of the list must match or exceed the number of arguments identified in 
     *   the <arglist> element in the XML or a std::lenth_error exeption will be thrown.
     */
    Number operator()(const std::vector<Number> &args) const { return root_->eval(args); }

    /*!
     * \brief Evaluation operator form 2
     *
     * \param args - list of values being passed to the function.
     *   
     * \warning The length of the array must be sufficient to cover all of the arguments identified
     *   in the <arglist> element of the XML.  Because this cannot be verified at runtime, failure
     *   to enforce this may result in unpredictable results, including the very real possibility
     *   of a segmentation fault.
     */
    Number operator()(const Number *args) const { return root_->eval(args); }

  private:

    ////////////////////////////////////////////////////////////
    // As all of the attributes are only used internally, no
    //   doxygen style documentation is provided
    ////////////////////////////////////////////////////////////
    /// \cond PRIVATE
    ////////////////////////////////////////////////////////////

    XMLFuncImp::Node      *root_;
    XMLFuncImp::ArgList_t  argList_;

    /// \endcond
};

#endif // _XMLFUNC_h_
