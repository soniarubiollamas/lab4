// readable smallpt, a Path Tracer by Kevin Beason, 2008.  Adjusted
// for my particular readability sensitivities by Roger Allen, 2016
// Added C++11 multithreading & removed openmp.

// Make:
// smallpt_thd: smallpt_thd.cpp
//	g++ -Wall -std=c++11 -O3 smallpt_thd.cpp -o smallpt_thd
// thought uname -s 16384 would help, but it didn't.

// Usage: time ./smallpt 100
// N  real
// 1  151
// 2   81
// 3
// 4   43
// 5   40
// 6   40
// 7
// 8   32

#include <atomic>
#include <cmath>
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "include/thread_pool.hpp"

// Vec is a structure to store position (x,y,z) and color (r,g,b)
struct Vec {
    double x, y, z;
    explicit Vec(double x_=0.0, double y_=0.0, double z_=0.0){ x=x_; y=y_; z=z_; }
    Vec operator+(const Vec &b) const {
        return Vec(x+b.x,y+b.y,z+b.z);
    }
    Vec operator-(const Vec &b) const {
        return Vec(x-b.x,y-b.y,z-b.z);
    }
    Vec operator*(double b) const {
        return Vec(x*b,y*b,z*b);
    }
    Vec mult(const Vec &b) const {
        return Vec(x*b.x,y*b.y,z*b.z);
    }
    Vec& norm() {
        return *this = *this * (1/sqrt(x*x+y*y+z*z));
    }
    double dot(const Vec &b) const {
        return x*b.x+y*b.y+z*b.z;
    }
    // % is cross product
    // rallen ??? why no const here?
    Vec operator%(Vec&b){
        return Vec(y*b.z-z*b.y,z*b.x-x*b.z,x*b.y-y*b.x);
    }
};

struct Ray {
    Vec o, d;
    Ray(Vec o_, Vec d_) : o(o_), d(d_) {}
};

// material types, used in radiance()
enum Refl_t { DIFF, SPEC, REFR };

struct Sphere {
    double rad;       // radius
    Vec p, e, c;      // position, emission, color
    Refl_t refl;      // reflection type (DIFFuse, SPECular, REFRactive)
    Sphere(double rad_, Vec p_, Vec e_, Vec c_, Refl_t refl_):
        rad(rad_), p(p_), e(e_), c(c_), refl(refl_) {}
    double intersect(const Ray &r) const {
        // returns distance, 0 if nohit
        Vec op = p-r.o; // Solve t^2*d.d + 2*t*(o-p).d + (o-p).(o-p)-R^2 = 0
        double t, eps=1e-4, b=op.dot(r.d), det=b*b-op.dot(op)+rad*rad;
        if (det<0) {
            return 0;
        }
        else {
            det=sqrt(det);
        }
        return (t=b-det)>eps ? t : ((t=b+det)>eps ? t : 0);
    }
};

//Scene: radius, position, emission, color, material
Sphere spheres[] = {
    Sphere(1e5, Vec( 1e5+1,40.8,81.6), Vec(),Vec(.75,.25,.25),DIFF),//Left
    Sphere(1e5, Vec(-1e5+99,40.8,81.6),Vec(),Vec(.25,.25,.75),DIFF),//Rght
    Sphere(1e5, Vec(50,40.8, 1e5),     Vec(),Vec(.75,.75,.75),DIFF),//Back
    Sphere(1e5, Vec(50,40.8,-1e5+170), Vec(),Vec(),           DIFF),//Frnt
    Sphere(1e5, Vec(50, 1e5, 81.6),    Vec(),Vec(.75,.75,.75),DIFF),//Botm
    Sphere(1e5, Vec(50,-1e5+81.6,81.6),Vec(),Vec(.75,.75,.75),DIFF),//Top
    Sphere(16.5,Vec(27,16.5,47),       Vec(),Vec(1,1,1)*.999, SPEC),//Mirr
    Sphere(16.5,Vec(73,16.5,78),       Vec(),Vec(1,1,1)*.999, REFR),//Glas
    Sphere(600, Vec(50,681.6-.27,81.6),Vec(12,12,12),  Vec(), DIFF) //Lite
};

inline double clamp(double x){ return x<0 ? 0 : x>1 ? 1 : x; }

inline int toInt(double x){ return int(pow(clamp(x),1/2.2)*255+.5); }

inline bool intersect(const Ray &r, double &t, int &id) {
    double n=sizeof(spheres)/sizeof(Sphere), d, inf=t=1e20;
    for(int i=int(n);i--;) {
        if((d=spheres[i].intersect(r))&&d<t){
            t=d;
            id=i;
        }
    }
    return t<inf;
}

std::atomic<int> max_depth{0};

Vec radiance(const Ray &r, int depth, unsigned short *Xi){
    double t;                               // distance to intersection
    int id=0;                               // id of intersected object
    if (!intersect(r, t, id)) {
        return Vec();  // if miss, return black
    }
    const Sphere &obj = spheres[id];        // the hit object
    Vec x=r.o+r.d*t, n=(x-obj.p).norm(), nl=n.dot(r.d)<0?n:n*-1, f=obj.c;
    double p = f.x>f.y && f.x>f.z ? f.x : f.y>f.z ? f.y : f.z; // max refl
    if (++depth>5) {
        if (erand48(Xi)<p){
            f=f*(1/p);
        }
        else{
            return obj.e; //R.R.
        }
    }
#if 1
    // RALLEN this isn't a good CAS, but is "good enough"
    if(depth > max_depth) {
        max_depth = depth;
    }
#endif
    if (obj.refl == DIFF) {
        // Ideal DIFFUSE reflection
        double r1=2*M_PI*erand48(Xi), r2=erand48(Xi), r2s=sqrt(r2);
        Vec w=nl, u=((fabs(w.x)>.1?Vec(0,1):Vec(1))%w).norm(), v=w%u;
        Vec d = (u*cos(r1)*r2s + v*sin(r1)*r2s + w*sqrt(1-r2)).norm();
        return obj.e + f.mult(radiance(Ray(x,d),depth,Xi));
    } else if (obj.refl == SPEC) {
        // Ideal SPECULAR reflection
        return obj.e + f.mult(radiance(Ray(x,r.d-n*2*n.dot(r.d)),depth,Xi));
    }
    Ray reflRay(x, r.d-n*2*n.dot(r.d));     // Ideal dielectric REFRACTION
    bool into = n.dot(nl)>0;                // Ray from outside going in?
    double nc=1, nt=1.5, nnt=into?nc/nt:nt/nc, ddn=r.d.dot(nl), cos2t;
    if ((cos2t=1-nnt*nnt*(1-ddn*ddn))<0) {    // Total internal reflection
        return obj.e + f.mult(radiance(reflRay,depth,Xi));
    }
    Vec tdir = (r.d*nnt - n*((into?1:-1)*(ddn*nnt+sqrt(cos2t)))).norm();
    double a=nt-nc, b=nt+nc, R0=a*a/(b*b), c = 1-(into?-ddn:tdir.dot(n));
    double Re=R0+(1-R0)*c*c*c*c*c,Tr=1-Re,P=.25+.5*Re,RP=Re/P,TP=Tr/(1-P);
    return obj.e + f.mult(depth>2 ? (erand48(Xi)<P ?   // Russian roulette
                                     radiance(reflRay,depth,Xi)*RP:radiance(Ray(x,tdir),depth,Xi)*TP) :
                          radiance(reflRay,depth,Xi)*Re+radiance(Ray(x,tdir),depth,Xi)*Tr);
}


struct Region {
    int x0, x1, y0, y1;
    explicit Region(int x0, int x1, int y0, int y1) :
        x0(x0), x1(x1), y0(y0), y1(y1) {}
    void print() {
      std::cout << "x0: " << x0 << ", x1: " << x1 << std::endl;
      std::cout << "y0: " << y0 << ", y1: " << y1 << std::endl;
      std::cout << std::endl;
    }
};

void render(int w, int h, int samps, Ray cam,
            Vec cx, Vec cy, Vec *c,
            const Region reg
    ) {
    int y0 = reg.y0, y1 = reg.y1;
    int x0 = reg.x0, x1 = reg.x1;

    for (int y=y0; y<y1; y++) {                       // Loop over image rows
        for (unsigned short x=x0, Xi[3]={0,0,static_cast<unsigned short>(y*y*y)}; x<x1; x++) {   // Loop cols
            for (int sy=0, i=(h-y-1)*w+x; sy<2; sy++) {     // 2x2 subpixel rows
                for (int sx=0; sx<2; sx++) {        // 2x2 subpixel cols
                    Vec r{0.0, 0.0, 0.0};
                    for (int s=0; s<samps; s++) {
                        double r1=2*erand48(Xi), dx=r1<1 ? sqrt(r1)-1: 1-sqrt(2-r1);
                        double r2=2*erand48(Xi), dy=r2<1 ? sqrt(r2)-1: 1-sqrt(2-r2);
                        Vec d = cx*( ( (sx+.5 + dx)/2 + x)/w - .5) +
                                                       cy*( ( (sy+.5 + dy)/2 + y)/h - .5) + cam.d;
                        r = r + radiance(Ray(cam.o+d*140,d.norm()),0,Xi)*(1./samps);
                    } // Camera rays are pushed ^^^^^ forward to start in interior
                    c[i] = c[i] + Vec(clamp(r.x),clamp(r.y),clamp(r.z))*.25;
                }
            }
        }
    }
}

std::pair<size_t, size_t>
usage(int argc, char *argv[], size_t w, size_t h) {
    // read the number of divisions from the command line
    if (!((argc == 1) || (argc == 3))) {
        std::cerr << "Invalid syntax: smallpt_thread_pool <width_divisions> <height_divisions>" << std::endl;
        exit(1);
    }

    size_t w_div = argc == 1 ? 2 : std::stol(argv[1]);
    size_t h_div = argc == 1 ? 2 : std::stol(argv[2]);

    if (((w/w_div) < 4) || ((h/h_div) < 4)){
        std::cerr << "The minimum region width and height is 4" << std::endl;
        exit(1);
    }
    return std::make_pair(w_div, h_div);
}

void write_output_file(const std::unique_ptr<Vec[]>& c, size_t w, size_t h)
{
    std::ofstream ofile("image3.ppm", std::ios::out);
    ofile << "P3" << std::endl;
    ofile << w << " " << h << std::endl;
    ofile << "255" << std::endl;
    for (size_t i=0; i<w*h; i++) {
      ofile << toInt(c[i].x) << " " << toInt(c[i].y) << " " << toInt(c[i].z) << std::endl;
    }
}

int main(int argc, char *argv[]){
    size_t w=1024, h=768, samps = 2; // # samples

    Ray cam(Vec(50,52,295.6), Vec(0,-0.042612,-1).norm()); // cam pos, dir
    Vec cx=Vec(w*.5135/h), cy=(cx%cam.d).norm()*.5135;
    std::unique_ptr<Vec[]> c{new Vec[w*h]};

    auto p = usage(argc, argv, w, h);
    auto w_div = p.first;
    auto h_div = p.second;

    auto start = std::chrono::steady_clock::now();

    auto *c_ptr = c.get(); // raw pointer to Vector c

    // create a thread pool
    thread_pool pool;

    // launch the tasks
    for (size_t y = 0; y<h; y+=h_div){ // Loop over image rows
        for (size_t x = 0; x<w; x+=w_div){ // Loop cols
            Region reg(x, std::min(x+w_div, w), y, std::min(y+h_div, h)); // region
            pool.submit([=, &c_ptr]{ 
                // encapsulate the render function call in a lambda
                render(w, h, samps, cam, cx, cy, c_ptr, reg); 
            });
        }
    }

    // wait for completion
    pool.wait();
    auto stop = std::chrono::steady_clock::now();
    std::cout << "Execution time: " <<
      std::chrono::duration_cast<std::chrono::milliseconds>(stop-start).count() << " ms." << std::endl;

    write_output_file(c, w, h);
}


