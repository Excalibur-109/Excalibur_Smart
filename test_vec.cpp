#include "Math/Vector.hpp"
int main() {
    using V = Math::Vector<float, 3>;
    V a(1,2,3), b(4,5,6);
    V c = a + b;
    V d = a - b;
    V e = a * 2.0f;
    V f = 2.0f * a;
    V g = -a;
    (void)c;(void)d;(void)e;(void)f;(void)g;
    Math::Vector<int,2> vi(1,2);
    auto h = vi + 3;
    auto i = 3 + vi;
    (void)h;(void)i;
    return 0;
}
