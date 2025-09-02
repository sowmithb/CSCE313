#include <iostream>
#include <cstring>
using namespace std;

struct Point {
    int x, y;

    Point () : x(), y() {}
    Point (int _x, int _y) : x(_x), y(_y) {}
};

class Shape {
    int vertices;
    Point** points;
public: //made following functions public so main can access them.
    Shape (int _vertices) {
        vertices = _vertices;
        points = new Point*[vertices];
    }

    ~Shape () {
        for(int i = 0; i < vertices; i++) {
            delete points[i];
        }
        delete[] points;
    }

    void addPoints (Point pts[]/* formal parameter for unsized array called pts */) {
        for (int i = 0; i < vertices; i++) {
            //memcpy(points[i], &pts[i%vertices], sizeof(Point)); this line caused seg fault as memcpy directly copies memory
            points[i] = new Point(pts[i]); //this creates new points with same value of pts and adds it to the array points in shape
        }
    }

    double area () {//we want a number not a pointer
        int temp = 0;
        for (int i = 0; i < vertices; i++) {
            // FIXME: there are two methods to access members of pointers
            //        use one to fix lhs and the other to fix rhs
            int lhs = (*points[i]).x * (*points[(i+1) % vertices]).y;
            int rhs = points[(i+1) % vertices]->x * points[i]->y;
            temp += (lhs - rhs);
        }
        double area = abs(temp)/2.0;
        return area;//return the number area holds
    }
};

int main () {
    // FIXME: create the following points using the three different methods
    //        of defining structs:
    //          tri1 = (0, 0)
    //          tri2 = (1, 2)
    //          tri3 = (2, 0)
    Point tri1 = {0, 0};
    Point tri2;
    tri2.x = 1;
    tri2.y = 2;
    Point tri3(2,0);
    

    // adding points to tri
    Point triPts[3] = {tri1, tri2, tri3};
    Shape* tri = new Shape(3);
    tri->addPoints(triPts);//tri is a pointer so needs to be accessed like one

    // FIXME: create the following points using your preferred struct
    //        definition:
    //          quad1 = (0, 0)
    //          quad2 = (0, 2)
    //          quad3 = (2, 2)
    //          quad4 = (2, 0)

    Point quad1 = {0, 0};
    Point quad2 = {0, 2};
    Point quad3 = {2, 2};
    Point quad4 = {2, 0};

    // adding points to quad
    Point quadPts[4] = {quad1, quad2, quad3, quad4};
    Shape* quad = new Shape(4);
    quad->addPoints(quadPts); //pointer again

    
    // FIXME: print out area of tri and area of quad
    cout<<"Tri area:"<<tri->area()<< endl;
    delete tri;
    cout<<"Quad area:"<<quad->area()<< endl;
    delete quad;

    

}
