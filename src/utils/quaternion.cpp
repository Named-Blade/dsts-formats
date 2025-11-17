#pragma once

#include <array>
#include <cmath>

// Quaternion multiplication for [x, y, z, w]
void quat_mul(const float a[4], const float b[4], float out[4]) {
    float ax = a[0], ay = a[1], az = a[2], aw = a[3];
    float bx = b[0], by = b[1], bz = b[2], bw = b[3];

    out[0] = aw*bx + ax*bw + ay*bz - az*by;
    out[1] = aw*by - ax*bz + ay*bw + az*bx;
    out[2] = aw*bz + ax*by - ay*bx + az*bw;
    out[3] = aw*bw - ax*bx - ay*by - az*bz;
}

// Quaternion inverse (conjugate) for [x, y, z, w]
void quat_inverse(const float q[4], float out[4]) {
    out[0] = -q[0];
    out[1] = -q[1];
    out[2] = -q[2];
    out[3] =  q[3];
}

// Rotate vector v by quaternion q ([x, y, z, w])
void quat_rotate(const float q[4], const float v[3], float out[3]) {
    float x = q[0], y = q[1], z = q[2], w = q[3];

    // t = 2 * cross(q_xyz, v)
    float t[3] = {
        2 * ( y*v[2] - z*v[1] ),
        2 * ( z*v[0] - x*v[2] ),
        2 * ( x*v[1] - y*v[0] )
    };

    // v' = v + w*t + cross(q_xyz, t)
    out[0] = v[0] + w*t[0] + ( y*t[2] - z*t[1] );
    out[1] = v[1] + w*t[1] + ( z*t[0] - x*t[2] );
    out[2] = v[2] + w*t[2] + ( x*t[1] - y*t[0] );
}