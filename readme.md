# Overview

XMLFunc is a C++ class that implments a mathematical function (*of fairly arbitrary complexity*) given a description of that function in XML.

To use this class, you must first understand the recognized XML tags, attributes, and values which define the function to be implemented AND the C++ interface for using the 
generatedfunction.  We will start with the latter as that is the simpler of the two:

-----

# The C++ interface

## XMLFunc Class

### Constructor

There are is a single constructors for an XMLFunc object:

    XMLFunc(const std::string xml)

**xml** is either the name of a file containing the XML or the XML string itself.

- If the string is a valid path to an XML file, it will be assumed that the XML is provided in that file.  
- Otherwise, it will be assumed that the string is the XML.

*If anyone can think of a case where this could be ambigious, please let me know... I cannot think of any such scenario.*

### Invocation

There is a single invocation method for an XMLFunc object:

    XMLFunc::Number eval(const std::vector<XMLFunc::Number> &args) const

**args** is a list of values being passed to the function for evaluation. 

The length of the list must match or exceed the number of arguments identified
in the \<arglist> element in the input XML having insufficient values results 
in a std::runtime_error being thrown.

_See the description of the XMLFunc::Number class below._


## XMLFunc::Number class

The XMLFunc::Number class is provided to support both double and integer
operations with native C/C++ accuracies.  

### Constructors

A XMLFunc::Number object can be constructed from either an integer or a double.

    XMLFunc::Number iv(long);    // inherently integer value
    XMLFunc::Number dv(double);  // inherently double value

Once constructed, the object retains knowledge of type type of number it represents.

### Public Enums

    enum XMLFunc::Number::Type_t { Integer, Double }

### Accessors

    XMLFunc::Number::Type_t XMLFunc::type(void) const;
    
    bool XMLFunc::isInteger(void) const;
    bool XMLFunc::isDouble(void) const;
    
### Casting operator

A XMLFunc::Number object can be either explicitly or implicitly cast to either a *long* or a *double*.  The accuracy of this cast is the same as associated with native C/C++ casting between *long* and *double*
    
    long   XMLFunc::intValue(void) const;
    double XMLFunc::doubleValue(void) const;
    
    operator long(void) const;
    operator double(void) const;

> **Example**<br/>
> <pre>
> XMLFunc::Number iv(1234);
> XMLFunc::Number dv(12.34)<br/>
> int i1 = int(iv);   // 1234
> int i2 = int(dv);   // 12<br/>
> double d1 = double(iv);   // 1234.0
> double d2 = double(dv);   // 12.34
> </pre>

### Thats it!

-----

# The XML interface

And now onto the **fun** stuff... the XML which defines the XMLFunc...

First and foremost, this is not a full/robust XML parser.  

- It recognizes (*and ignores*) the XML declaration (i.e. <?xml......>.  
- It recognizes (*and ignores*) XML comments (i.e. \<!--    -->).  
- It recognize only the tag keywords described below.
- It recognize both 
  - open/close tag pairs (i.e. \<tag ... >...\</tag>) 
  - bodyless tags (i.e. \<tag ... /\> )
- It does not support untagged data
  - the content between open/close pairs must be tagged
  - all raw data must appear in attributes
- It requires all attribute values to be quoted (either \" or ' )
- It does **not** recognize unicode (*there is no need for it* ).

*I have not been able to figure out how to describe the legal XMLFunc
syntax using XSchema due to the recursive nature of the value elements.
If someone can tip me off how to implement an "is-a" definition, I will add
the appropriate .xsd file to this project.*

## Argument List

The first XML element must define the arguments that will be passed to the XMLFunc
eval method.  

- It is identified by the \<arglist> tag
- It is a container element whose contents define the names and types of each argument.
- There are no attributes defined for \<arglist>

Each argument in the \<arglist> is defined with the \<arg> tag.  These must be specified in the order they will be passed to XMLFunc::eval()

    \<arg name=var-name type=var-type/>

- The optional type attribute may have a value of either *integer* or *double*
- The optional name attribute may be used to reference arguments in an operation element
  - var-name is any string consisting of alphanumeric characters
  - var-name cannot start with a digit

> **Example**<br/>
> <pre>
> \<arglist>
>   <\arg name="exponent" type="integer"/>
>   <\arg name="base"     type="double"/>
> \</arglist>
> </pre>

## Value Elements

The remaining elements in the XML struture each has a value.  There can be 
  only one top level value element, which may contain one or more value elements,
  which may in turn contain one or more value elements, which may in turn ....

There are two basic types of value elements: inputs and operators.  Inputs are
  always leaf nodes in the XML.  Operators may be leaf nodes (if all of its 
  operands are specified via attributes) or, more often, as composite elements 
  containing one or more other value elements.  
  
> **value** := input | operator

---

### Input elements

There are two types of input elements: arguments and constants.

> **input** := constant | argument

#### Constant elements
 
Constants elements represent numerical values that (*no surprise here*) don't change.

- do not change between invocations of XMLFunc::eval()
- are identified with the \<integer> or \<double> tag
- must be qualified with the value attribute

> **constant** := \<integer value="value"/> | \<double value="value"/>
> 
> - the value must be parsable as the specified type<br/>
> - a double can take any valid number format in the range of a C++ double<br/>
> - an integer can only take a valid integer format in the range of a C++ long int<br/>
> - there are no unsigned integer values

#### Argument elements 

Argument elements reference one of the elements in the input array of values. 

- can change with every invocation of XMLFunc::eval()
- are identified with the <arg> tag
- must be qualified with either a name or index attribute (but not both)

> **argument** := \<arg name="name"> | \<arg index="index">
> 
> - if qualified by name, the name must exactly match one of the named arguments in <arglist><br/>
> - if qualified by index, the index must be an integer in the range 0 to (num_args-1)


#### Examples

<pre>
&lt;integer value="3"/>
&lt;integer value="3.14159"/>   // INVALID
&lt;double  value="3"/>
&lt;double  value="3.14159"/><br>
&lt;arg name="exponent"/>      // index=0
&lt;arg index="1"/>            // name=base
</pre>

**Note**: most of the operator elements listed below are capable of taking input values
directly via attributes. This will often be cleaner than creating input elements.

<pre>
&lt;add arg1="5" arg2="base"/>
</pre>

is easier to read than

<pre>
&lt;add>
  &lt;integer value="5"/>
  &lt;arg name="base"/>
&lt;/add>
</pre>

---

### Operator elements

There are many defined operator elements, but they fall into three categories: 

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

> **operator** := unary-op | binary-op | list-op

#### Unary operators

Unary operators take a single operand. (*Note that this is a bit of a misnomer in that many of these 'operands' are actually functions*.)

The operand value may be provided either:

- as a single value element between the opening/closing tags
- via the arg attribute, which
  - if parses as an integer, is equivalent to providing an \<integer> element
  - if parses as a double, is equivalent to providing a \<double> element
  - if appears as a name in \<arglist>, is equivalent to providing an \<arg> element
  - otherwise, throws an exception
  
> **unary-op** := \<op arg="value"/> | \<op>\<.../>\</op>

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

*While log is listed as a unary-operator, it has a slight difference
    from the other unary operators in that it accepts 'base' as an optional 
    attribute (base=10 if not specified)
    
*All trig functions use radians*

**Examples:**  *these are both*  -cos(angle)

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

*these are both*  | log_2(10.0) |

    <abs>
      <log base="2">
        <double value="10.0"/>
      </log>
    </abs>
    
    <abs>
      <log arg="10.0" base="2"/>
    </abs>

#### Binary operators

Binary operators take two operands. (*One of these operators is actually a function*.)
  
The values of the operands may be provided:

- value elements between the opening/closing tags
- via the arg1 and arg2 attributes
- a combination thereof

The rules for interpreting the arg attributes are the same as for the unary operators.

- If both values are specified with attributes, the element must not have any content between
    the open/close tags.
- If neither is specified with attributes, there must be two value elements included between
    the open/close tags.
- If one of the values is specified through an attribute, there must be one value element 
    included between the open/close tags.  It will be assume to be the other operand value.

> **binary-op** := <br/>
> \<op arg1="value" arg2="value"/><br/>
> | \<op arg1="value">\<arg2-op/>\</op><br/>
> | \<op arg2="value">\<arg1-op/>\</op><br/>
> | \<op>\<arg1-op/>\<arg2-op/>\</op> 

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
      <mult>
        <integer value="-1"/>
        <div>
          <pow arg2="2"><arg name="x"></pow>
          <integer value="5"/>
        </div>
      </mult>
    </exp>

#### List operators

List operators take two or more operands. 

The values of the first two operands may be provided with the arg1 and arg2 attributes
using the same rules as for a binary operator.  All other operands must be provided
as value elements between the opening/closing tags.
  
The list of available list operators is much shorter:

<pre>
add      returns the sum of all of the values
mult     returns the product of all of the values
</pre>

See the section on input elements above for an example.

