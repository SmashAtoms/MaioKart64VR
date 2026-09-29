#pragma once
#include <array>
#include <cmath>

namespace aether {
struct Vec3 { float x=0, y=0, z=0; };
Vec3 operator+(Vec3 a, Vec3 b);
Vec3 operator-(Vec3 a, Vec3 b);
Vec3 operator*(Vec3 a, float s);
struct Quat { float x=0, y=0, z=0, w=1; };
Quat Normalize(Quat q);
Quat operator*(Quat a, Quat b);
Vec3 Rotate(Quat q, Vec3 v);
struct Pose { Quat orientation{}; Vec3 position{}; };
Pose operator*(Pose a, Pose b);
Pose Inverse(Pose p);
Pose NeutralHeading(Pose head);
struct Mat4 {
    // Column-major matrices and column vectors throughout the public interface.
    std::array<float,16> m{};
    static Mat4 Identity();
};
Mat4 operator*(const Mat4& a, const Mat4& b);
Mat4 Matrix(Pose p);
Vec3 Transform(const Mat4& m, Vec3 p);
Mat4 Projection(float left, float right, float up, float down, float nearZ, float farZ);
// C can reflect handedness. Translation is scaled; orientation is conjugated by C.
Mat4 ComposeEye(Pose worldAnchor, Pose referenceNeutral, Pose referenceEye,
                const Mat4& gameFromXrBasis, float gameUnitsPerMeter);
Mat4 RigidInverse(const Mat4& m);
}
