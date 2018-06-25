
using namespace std;


class XMLFunc
{
  public:
    class Number
    {
      public:
        Number(int v)    : int_(v), double_(double(v)) {}
        Number(double v) : int_(int(v)), double_(v) {}
        
        operator int(void)    const { return int_;    }
        operator double(void) const { return double_; }
      private:
        int    int_;
        double double_;
    };
};

