#include "aether/math.hpp"
#include <stdexcept>

namespace aether {
Vec3 operator+(Vec3 a, Vec3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vec3 operator-(Vec3 a, Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vec3 operator*(Vec3 a, float s) { return {a.x*s,a.y*s,a.z*s}; }
Quat Normalize(Quat q) {
    float l=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
    if (!std::isfinite(l) || l<1e-8f) throw std::invalid_argument("Invalid pose quaternion");
    return {q.x/l,q.y/l,q.z/l,q.w/l};
}
Quat operator*(Quat a, Quat b) {
    return {a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
            a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
            a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
            a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};
}
Vec3 Rotate(Quat q, Vec3 v) {
    q=Normalize(q);
    Quat r=q*Quat{v.x,v.y,v.z,0}*Quat{-q.x,-q.y,-q.z,q.w};
    return {r.x,r.y,r.z};
}
Pose operator*(Pose a, Pose b) {
    return {Normalize(a.orientation*b.orientation),a.position+Rotate(a.orientation,b.position)};
}
Pose Inverse(Pose p) {
    auto q=Normalize(p.orientation); q={-q.x,-q.y,-q.z,q.w};
    return {q,Rotate(q,p.position*-1)};
}
Pose NeutralHeading(Pose head) {
    const auto f=Rotate(head.orientation,{0,0,-1});
    const float yaw=std::atan2(-f.x,-f.z);
    return {{0,std::sin(yaw/2),0,std::cos(yaw/2)},head.position};
}
Mat4 Mat4::Identity() { Mat4 a; a.m[0]=a.m[5]=a.m[10]=a.m[15]=1; return a; }
Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for(int c=0;c<4;++c) for(int row=0;row<4;++row)
        for(int k=0;k<4;++k) r.m[c*4+row]+=a.m[k*4+row]*b.m[c*4+k];
    return r;
}
Mat4 Matrix(Pose p) {
    auto r=Mat4::Identity();
    const Vec3 axes[]={Rotate(p.orientation,{1,0,0}),Rotate(p.orientation,{0,1,0}),Rotate(p.orientation,{0,0,1})};
    for(int i=0;i<3;++i) { r.m[i*4]=axes[i].x;r.m[i*4+1]=axes[i].y;r.m[i*4+2]=axes[i].z; }
    r.m[12]=p.position.x;r.m[13]=p.position.y;r.m[14]=p.position.z;
    return r;
}
Vec3 Transform(const Mat4& m, Vec3 p) {
    return {m.m[0]*p.x+m.m[4]*p.y+m.m[8]*p.z+m.m[12],
            m.m[1]*p.x+m.m[5]*p.y+m.m[9]*p.z+m.m[13],
            m.m[2]*p.x+m.m[6]*p.y+m.m[10]*p.z+m.m[14]};
}
Mat4 RigidInverse(const Mat4& m) {
    auto r=Mat4::Identity();
    for(int c=0;c<3;++c) for(int row=0;row<3;++row) r.m[c*4+row]=m.m[row*4+c];
    Vec3 t=Transform(r,{-m.m[12],-m.m[13],-m.m[14]});
    r.m[12]=t.x;r.m[13]=t.y;r.m[14]=t.z;return r;
}
Mat4 Projection(float left,float right,float up,float down,float nearZ,float farZ) {
    if(nearZ<=0 || farZ<=nearZ || left>=right || down>=up) throw std::invalid_argument("Invalid projection");
    float l=std::tan(left),r=std::tan(right),u=std::tan(up),d=std::tan(down);
    Mat4 m;
    m.m[0]=2/(r-l);m.m[5]=2/(u-d);m.m[8]=(r+l)/(r-l);m.m[9]=(u+d)/(u-d);
    m.m[10]=-(farZ+nearZ)/(farZ-nearZ);m.m[11]=-1;m.m[14]=-2*farZ*nearZ/(farZ-nearZ);
    return m;
}
Mat4 ComposeEye(Pose anchor,Pose neutral,Pose eye,const Mat4& basis,float scale) {
    if(!std::isfinite(scale)||scale<=0) throw std::invalid_argument("Invalid world scale");
    auto delta=Matrix(Inverse(neutral)*eye);
    delta.m[12]*=scale;delta.m[13]*=scale;delta.m[14]*=scale;
    return Matrix(anchor)*basis*delta*RigidInverse(basis);
}
}
