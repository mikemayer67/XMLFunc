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

where xml is **either** the name of a file containing the XML or the XML string itself.

- If the string appears to be a valid path to an XML file, it will be assumed that the XML is provided in that file.  
- Otherwise, it will be assumed that the string is the XML.

  *If anyone can think of a case where this could be ambigious, please let me know... 
  I cannot think of any such scenario.*

Invocation
---------

There are two variations on the invocation method for an XMLFunc object:

    XMLFunc::Number eval(const std::vector<XMLFunc::Number> &args) const
    XMLFunc::Number eval(const XMLFunc::Number *args) const

- In both cases, **args** is a list of values being passed to the function for evaluation.  
- In both cases, the length of the list must match or exceed the number of arguments identified
    in the \<arglist> element in the input XML
- In the first case, having insufficient values results in a std::runtime_error being thrown.
- In the second case, having insufficient values will almost certainly result in unpredictable results
    and may very will likely lead to a segmentation fault.

_See the description of the XMLFunc::Number class below._


XMLFunc::Number class
---------------------

The XMLFunc::Number class is provided to support both double and integer
operations with native C/C++ accuracies.  An object of this class can be
constructed from either an integer or a double and can be cast to either
an integer or a double.  Casting to the same type as used when constructing
the XML::Number object has lossless accuracy.  Casting an integer constructed
object to a double value or vice-versa introduces the same accuracy loss (*if
any*) associated with the native C/C++ casts.

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

And now onto the **fun** stuff... the XML which defines the XMLFunc...

First and foremost, this is not a full/robust XML parser.  

- It will recognize and ignore the XML declaration (i.e. <?xml......>.  
- It will recognize and ignore XML comments (i.e. \<!--    -->).  
- It will recognize the elements described below, which can consist of either:
  - a matched pair of open/close tags (e.g. \<add>...\</add>)
  - a self-contained tag (e.g. \<integer value=3/> or \<argument index=2/>).
- The parser does not recognize unicode (*there is no need for it*).

_Also, I have not been able to figure out how to describe the legal XMLFunc
syntax using XSchema due to the recursive nature of the value elements.
If someone can tip me off how to implement an "is-a" definition, I will add
the appropriate .xsd file to this project._

Argument List
-------------

The first element defines the arguments being passed.  It is a container
element whose contents define the names and types of each argument.
These must appear in the same order that they will be passed in the
Number vector or array in the native C/C++ code.

Each argument is defined with the \<arg> tag.

    <arg name=var-name type=var-type/>

where

>  var-name is any string consisting of alphanumeric characters (no whitespace or unicode, and must not start with a digit).  *This attribute is optional.  The argument must be referenced by index if not specified.*

>  var-type is either the literal string "integer" or "double".  *This attribute is optional. It defaults to double if not specified.*

### Example:

    <arglist>
      <arg name="exponent" type="integer"/>
      <arg name="base"     type="double"/>
    </arglist>

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

    value := input|operator

--------------------------------------------------------------------------------

There are two types of input elements: arguments and constants.

    input := <arg>|constant

- Arguments reference one of the elements in the input array of values.  These can change
  with every invocation of the XMLFunc object in the C++ code.  
- Constants represent a numerical value 
  that do not change with every invocation and are (no surprise here) constant
  throughout the life of the XMLFunc object.

Arguments are specified using the \<arg> tag (different from above) using
    either a name or index attributes.  A specified name value must match one of the names
    in the <arglist>.  An index value must be a non-negative integer in the 
    range 0 to (num_args)-1.
  
**Example**  (these two are identical given the arglist above)

    <arg name="exponent"/>
    <arg index="0"/>  

Constants may be specified either directly or by using either the \<integer> or \<double> tag.

    constant := number_sting|<integer>|<double>

- If specified directly
  - If it contains nothing but digits, it will be considered an integer value.  
  - Otherwise, it will be considered a double value.
- If specified using the \<integer> or \<double> tags
   - The actual value may be specified using the value attribute or may be
     specified between opening/closing tags
   - The value will be cast to the specified type.

**Example**  (these two are identical given the arglist above)

    <add>   <!-- adds four integer values of 3 -->
      3
      \<integer value="3"/>
      \<integer>3</integer>
      \<integer value="3.14159">
    </add>  
    
    <add>   <!-- adds four double values (ALMOST 4pi) -->
      3.14159
      \<double value="3.14159"/>
      \<double>3.14159</double>
      \<double value="3">
    </add> 

--------------------------------------------------------------------------------

There are many operator elements, but they fall into three categories: 

- unary
- binary
- list  

As these category names suggest, the first takes
  a single operand, the second takes two operands, and the last takes two
  or more operands.

Unless otherwise noted, the value type associated with the operand is determined
  by the types of the operands.  If all operands are integer types, then the 
  operator will have an integer value.  If any operand is a double type, then
  the operator will have a double value.

When specifying operand values by attribute, the value must be either a numeric
  value or the name of an argument from arglist.  Attempting to specify an 
  argument by index will not work as the index will be interpreted as a numeric
  value.

    operator := unary-op|binary-op|list-op

--------------------------------------------------------------------------------

**Unary** operators take a single operand.  Note that this is a bit of a misnomer in that many of these 'operands' are actually functions

- The value of the operand may be provided via the 'arg' attribute.  
- The value of the operand may be provided as a single value element between the 
    opening/closing operator tags
- The value may be numeric, the name of one of the arguments in the \<arglist>, 
    or a value element.

The list of available unary operators is as follows: (*items marked with a D always return a double value*)

<pre>
neg      changes the sign of the argument
sin  (D) sine trig function
cos  (D) cosine trig function
tan  (D) tangent trig function
asin (D) arcsine trig function
acos (D) arccosine trig function
atan (D) arctangent trig function
deg  (D) converts a radian value to degrees
rad  (D) converts a degree value to radians
abs      absolute value
sqrt (D) square root
exp  (D) e raised to the specified power
ln   (D) natural log
log  (D) * see note
</pre>

*While log is listed as a unary-operator, it has a slight differeence
    from the other unary operators in that it accepts 'base' as an optional 
    attribute (base=10 if not specified)
    
All trig functions use radians

**Examples:**  *these are all*  -cos(angle)

    <neg>  <!-- -cos(angle) -->
      <cos>
        <rad><arg name="angle"></rad>
      </cos>
    </neg>
    
    <neg>
      <cos>
        <rad arg="angle"></rad>
      </cos>
    </neg>
   
    <neg>
      <cos>
        <rad>angle</rad>
      </cos>
    </neg>

*these are all*  | log_2(10.0) |

    <abs>
      <log base="2">
        <double value="10.0"/>
      </log>
    </abs>
    
    <abs>
      <log base="2">10.0</log>
    </abs>
    
    <abs>
      <log arg="10.0" base="2"/>
    </abs>

--------------------------------------------------------------------------------

**Binary** operators take two operands. Again, one of these operators is actually
  a function. 
  
- The values of the operands may be provided via the 'arg1' and 'arg2' attributes.  
    *(the value may be either a number or the name of one of the arguments listed in \<arglist>)*
- The values of the operands may be provided as a pair of value elements between the opening/closing tags
- The values of the operands may be provided using a mixture of these two approaches.  
    *(if arg1 is provided as an attribute, the value between the tags will be used 
    as arg2 and vice versa)*


The list of available binary operators is as follows: (*items marked with a D always return a double value*)

<pre>
sub       subtracts arg2 from arg1
div       divides arg1 by arg2, * see note
pow   (D) raises arg1 to the power arg2
mod   (D) returns the arg2 modulus of arg1
atan2 (D) returns the arctangengent of arg2/arg1
</pre>

If both arg1 and arg2 are integers, the result will be an integer with the normal C/C++ truncation rules applied

All trig functions use radians

**Examples:**  *these are all exp( -x^2 / 5)*

    <exp>
      <neg>
        <div arg2="5">
          <pow arg1="x" arg2="2"/>
        </div>
      </neg>
    </exp>
    
    <exp>
      <neg>
        <div arg2="5">
          <pow>
            <arg name="x">
            2
          </pow>
        </div>
      </neg>
    </exp>

    

--------------------------------------------------------------------------------

**List** operators take two or more operands.  Here, the values must be provided
  between the opening/closing operator tags.
  
The list of available list operators is much shorter:

<pre>
add      returns the sum of all of the values
mult     returns the product of all of the values
</pre>

See the section on input elements above for an example.

