#pragma once

#include <vector>
#include <cmath>
#include <stdexcept>
#include <iomanip>

class Matrix {
public:
    float m[4][4];
    
    Matrix() { for(int i=0;i<4;i++) for(int j=0;j<4;j++) m[i][j]=(i==j?1.0f:0.0f); }
    float& operator()(int r,int c) { return m[r][c]; }
    float operator()(int r,int c) const { return m[r][c]; }

    // Matrix multiplication (row-major)
    Matrix multiply(const Matrix& other) const {
        Matrix result;
        for(int i=0;i<4;i++)
            for(int j=0;j<4;j++){
                result(i,j)=0.0f;
                for(int k=0;k<4;k++)
                    result(i,j) += m[i][k]*other(k,j);
            }
        return result;
    }

    // Translation
    static Matrix translation(float x, float y, float z){
        Matrix T;
        T(0,3)=x; T(1,3)=y; T(2,3)=z;
        return T;
    }

    // Scaling
    static Matrix scale(float x, float y, float z){
        Matrix S;
        S(0,0)=x; S(1,1)=y; S(2,2)=z;
        return S;
    }

    // Rotation from quaternion [x, y, z, w]
    static Matrix rotationFromQuaternion(float qx, float qy, float qz, float qw){
        Matrix R;
        float xx=qx*qx, yy=qy*qy, zz=qz*qz;
        float xy=qx*qy, xz=qx*qz, yz=qy*qz;
        float wx=qw*qx, wy=qw*qy, wz=qw*qz;

        R(0,0)=1-2*(yy+zz); R(0,1)=2*(xy - wz); R(0,2)=2*(xz + wy);
        R(1,0)=2*(xy + wz); R(1,1)=1-2*(xx+zz); R(1,2)=2*(yz - wx);
        R(2,0)=2*(xz - wy); R(2,1)=2*(yz + wx); R(2,2)=1-2*(xx+yy);

        return R;
    }

    // Inverse of affine matrix (row-major, last row = [0 0 0 1])
    Matrix inverseAffine() const {
        Matrix inv;
        // Copy 3x3 rotation/scale
        float R[3][3];
        for(int i=0;i<3;i++)
            for(int j=0;j<3;j++)
                R[i][j] = m[i][j];

        // Determinant
        float det = R[0][0]*(R[1][1]*R[2][2]-R[2][1]*R[1][2])
                  - R[0][1]*(R[1][0]*R[2][2]-R[2][0]*R[1][2])
                  + R[0][2]*(R[1][0]*R[2][1]-R[2][0]*R[1][1]);
        if (fabs(det)<1e-9f) throw std::runtime_error("Matrix not invertible");
        float invDet = 1.0f/det;

        // Inverse 3x3
        for(int i=0;i<3;i++)
            for(int j=0;j<3;j++)
                inv(i,j) = invDet * (
                    R[(j+1)%3][(i+1)%3]*R[(j+2)%3][(i+2)%3]
                  - R[(j+1)%3][(i+2)%3]*R[(j+2)%3][(i+1)%3]);

        // Inverse translation
        float tx=m[0][3], ty=m[1][3], tz=m[2][3];
        inv(0,3) = -(inv(0,0)*tx + inv(0,1)*ty + inv(0,2)*tz);
        inv(1,3) = -(inv(1,0)*tx + inv(1,1)*ty + inv(1,2)*tz);
        inv(2,3) = -(inv(2,0)*tx + inv(2,1)*ty + inv(2,2)*tz);

        // Bottom row
        inv(3,0)=inv(3,1)=inv(3,2)=0.0f; inv(3,3)=1.0f;

        return inv;
    }

    void print() const {
        std::cout << std::fixed << std::setprecision(6);
        for(int i=0;i<4;i++){
            for(int j=0;j<4;j++) std::cout << m[i][j] << (j==3?"":" ");
            std::cout << std::endl;
        }
    }
};

Matrix MirrorMatrix(const Matrix& input, const Matrix& center, bool mirrorX = true, bool mirrorY = false, bool mirrorZ = false) {
    Matrix result = input;

    // Compute translation relative to center
    float tx = input(0,3) - center(0,3);
    float ty = input(1,3) - center(1,3);
    float tz = input(2,3) - center(2,3);

    // Mirror translation axes
    if (mirrorX) tx = -tx;
    if (mirrorY) ty = -ty;
    if (mirrorZ) tz = -tz;

    // Set mirrored translation back
    result(0,3) = tx + center(0,3);
    result(1,3) = ty + center(1,3);
    result(2,3) = tz + center(2,3);

    // Mirror rotation axes (simple axis inversion)
    // Only mirror axes that correspond to translation flips
    if (mirrorX) {
        result(0,0) =  input(0,0);
        result(0,1) = -input(0,1);
        result(0,2) = -input(0,2);

        result(1,0) = -input(1,0);
        result(1,1) =  input(1,1);
        result(1,2) =  input(1,2);

        result(2,0) = -input(2,0);
        result(2,1) =  input(2,1);
        result(2,2) =  input(2,2);
    }

    // TODO: Extend for mirrorY / mirrorZ if needed

    return result;
}