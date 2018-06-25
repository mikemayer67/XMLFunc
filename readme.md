Overview
========

XMLFunc is a C++ class that implments a mathematical function (of fairly arbitrary complexity)
given a description of that function in XML.

To use this class, you must first understand the recognized XML tags, attributes, and values
which define the function to be implemented AND the C++ interface for using the generated
function.  We will start with the latter as that is the simpler of the two:

The C++ interface
=================

Constructor
-----------

There are is a single constructors for an XMLFunc object:

    XMLFunc(const std::string xml)


where

> xml is EITHER the name of a file containing the XML or the XML string itself.
- If the string appears to be a valid file path and if the file exists, it
    will be assumed that the XML is provided in that file.  
- Otherwise, it
    will attempt to parse the string as XML.
- *If anyone can think of a case where this could be ambigious, please let me know... 
    I certainly cannot think of such a scenario.)

Invocation
----------

There is a single invocation method for an XMLFunc object:

    Number eval(const std::vector<XMLFunc::Number> &args) const

where

> args is the list of values being passed to the function.  
- The length must match or exceed the number of arguments identified in the 
    '<arglist>' element in the input XML.

*See the description of the XMLFunc::Number class below.*

Convenience Functions
---------------------

There is also a few convenience functions/operators which wraps the eval method.
These have exactly the same functionality and the same caveat onthe number
of arguments passed.  

    XMLFunc::Number eval(const XMLFunc::Number *args) const

    XMLFunc::Number operator()(const std::vector<XMLFunc::Number> &args) const
    XMLFunc::Number operator()(const XMLFunc::Number *args) const

Note: passing a std::vector allows the code to verify the number of provided 
arguments and throw a std::runtime_error exception if not.  Passing a pointer
to an array, however, does not allow this verification and it will be assumed
that the content is correct.  If not, there exists a very real possibility of 
creating a segmentation fault or simply getting a very surprising result 
due to feeding random values into the function.

XMLFunc::Number class
---------------------

MThe XMLFunc::Number class is provided to support both double and integer
operations with native C/C++ accuracies.  An object of this class can be
constructed from either an integer or a double and can be cast to either
an integer or a double.  Casting to the same type as used when constructing
the XML::Number object has lossless accuracy.  Casting an integer constructed
object to a double value or vice-versa introduces the same accuracy loss (if
any associated with the native C/C++ casts.

    XMLFunc::Number iv(1234);    // inherently integer value
    XMLFunc::Number dv(12.34);   // inherently double value
    
    int i1 = int(iv);   // 1234 (lossless)
    int i2 = int(dv);   // 12   (truncated)
    
    double d1 = double(iv);   // 1234.0 (lossless in this case)
    double d2 = double(dv);   // 12.34  (lossless)

Thats it!
---------

The XML interface
=================

And now onto the fun stuff... the XML which defines the XMLFunc...

First and foremost, this is not a full/robust XML parser.  It will recognize
and ignore the XML declaration (i.e. <?xml......>.  It will recognize and 
ignore XML comments (i.e. <!--    -->).  It will recognize the elements described 
below, which can consist of either the open/close tags (e.g. <add>...</add>) or a 
self-contained tag (e.g. <integer value=3/> or <argument index=2/>).  The parser 
does not recognize unicode as there is no need for this (all tags are defined 
in ASCII and all values are numeric).

* Also, I have not been able to figure out how to describe the legal XMLFunc
syntax using XSchema due to the recursive nature of the value elements.
If someone can tip me off how to implement an "is-a" definition, I will add
the appropriate .xsd file to this project. *

Argument List
-------------

The first element defines the arguments being passed.  It is a container
element whose contents define the names and types of each argument.
These must appear in the same order that they will be passed in the
Number vector or array in the native C/C++ code.

Each argument is defined with the <arg> tag.

>  <arg name=var-name type=var-type/>

where

>  var-name is any string consisting of alphanumeric characters (no whitespace and no unicode).

>  var-type is either the literal string "integer" or "double"

### Example:

>  <arglist>
>    <arg name="exponent" type="integer"/>
>    <arg name="base"     type="double"/>
>  </arglist>

Value Elements
--------------

The remaining elements in the XML struture each is/has a value.  There may be 
  only one top level value element, which may contain one or more value elements,
  which may in turn contain one or more value elements, which may ....

There are two basic types of value elements: inputs and operators.  Inputs are
  always leaf nodes in the XML.  Operators may be leaf nodes (if all of its 
  operaands are specified via attributes) or, more often, as composite elements 
  containing one or more other value elements.  
  
  There are no attributes which apply to all value elements.

  (value) := (input)|(operator)

--------------------------------------------------------------------------------

There are two types of input elements: arguments and constants.  Arguments
  reference one of the elements in the input array of values.  These can change
  with every invocation of the XMLFunction.  Constants represent a numerical value 
  that do not change with every invocation and are (no surprise here) constant
  throughout the life of the XMLFunction.

  Arguments are specified using the <arg> tag (different from above) with using
    either the name or index attributes.  The name must match one of the names
    specified in the <arglist>.  The index must a non-negative integer in the 
    range 0 to (num_args)-1.

  Constants are specified using either the <integer> or <double> tag.  The
    value of the constant is specifed either using the 'value' attribute or
    by placing it between <integer>...</integer>  or <double>...</double>
    tags.  The specified value should be an integer for the <integer> tag, but
    may be either a double or an integer for the <double> tag.

    (input) := (arg)|integer|double

  Examples:
    
    <arg name="exponent"/>        (these two are identical given arglist above)
    <arg index="0"/>          

    <integer value="3"/>          (these two are identical)
    <integer>3</integer>

    <double value="1214.1967"/>   (these two are identical)
    <double>1213.1967</double>


--------------------------------------------------------------------------------

There are many operator elements, but they fall into three categories: unary,
  binary, and list.  As these category names suggest, the first takes
  a single operand, the second takes two operands, and the last takes two
  or more operands.

Unless otherwise noted, the value type associated with the operand is determined
  by the types of the operands.  If all operands are integer types, then the 
  operator will have an integer value.  If any operand is a double type, then
  the opeartor will have a double value.

When specifying operand values by attribute, the value must be either a numeric
  value or the name of an argument from arglist.  Attempting to specify an 
  argument by index will not work as the index will be interpreted as a numeric
  value.

  (operator) := (unary-op)|(binary-op)|(list-op)

--------------------------------------------------------------------------------

Unary operators take a single operand.  Note that this is a bit of a misnomer
  in that many of thes 'operands' are actually functions.  The value of the
  operand may be provided either via the 'arg' attribute or as a single value 
  element between the operator tags. 

The list of available unary operators is as follows:

  neg   (changes the sign of the argument)
  sin   (sine trig function)
  cos   (cosine trig function)
  tan   (tangent trig function)
  asin  (arcsine trig function)
  acos  (arccosine trig function)
  atan  (arctangent trig function)
  deg   (converts a radian value to degrees)
  rad   (converts a degree value to radians)
  abs   (absolute value)
  sqrt  (square root)
  exp   (e raised to the specified power)
  ln    (natural log)
  log   (* see note)

  * While log is listed as a unary-operator, it has a slight differeence
    from the other unary operators in that it accepts 'base' as an optional 
    attribute (base=10 if not specified)

  * All trig functions use radians

  Examples:

    -cos(angle) :=

      <neg>
        <cos>
          <rad><arg name="angle"></rad>
        </cos>
      </neg>

      or

      <neg>
        <cos>
          <rad arg="angle"></rad>
        </cos>
      </neg>

    | log_2(10.0) | :=

      <abs>
        <log arg="10.0" base="2"/>
      </abs>

--------------------------------------------------------------------------------

Binary operators take two operands. Again, one of these operators is actually
  a function. The values of the opearands may be provieded either via the
  'arg1' and 'arg2' attributes, a pair of value elements between the operator
  tags, or one of each.  Note that if arg1 is provided as an attribute, the
  value between the tags will be used as arg2 and vice versa.

The list of available binary operators is as follows:

  sub    (subtracts arg2 from arg1)
  div    (divides arg1 by arg2, * see note)
  pow    (raises arg1 to the power arg2)
  mod    (returns the arg2 modulus of arg1)
  atan2  (returns the arctangengent of arg2/arg1)

  * If both arg1 and arg2 are integers, the result will be an integer with
    the normal C/C++ truncation rules applied

  * All trig functions use radians

  Examples:

  e ^ ( -x^2 / 5 ) :=

    <exp>
      <neg>
        <div arg2="5">
          <pow arg1="x" arg2="2"/>
        </div>
      </neg>
    </exp>

    

--------------------------------------------------------------------------------

List operators take two or more operands.  Here, the values must be provided
  as value elements between the operator tags.

