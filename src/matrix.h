/* Copyright (C) 2015 Coos Baakman

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/


#ifndef MATRIX_H
#define MATRIX_H

#include "vec.h"
#include "quat.h"

struct matrix4
{
    union
    {
        float m[16];
        struct
        {
            float   m11, m21, m31, m41,
                    m12, m22, m32, m42,
                    m13, m23, m33, m43,
                    m14, m24, m34, m44;
        };
    };

    matrix4 (){}
    matrix4 (   const float Am11, const float Am12, const float Am13, const float Am14,
                const float Am21, const float Am22, const float Am23, const float Am24,
                const float Am31, const float Am32, const float Am33, const float Am34,
                const float Am41, const float Am42, const float Am43, const float Am44    )
    {
        m11=Am11;    m12=Am12;    m13=Am13;    m14=Am14;
        m21=Am21;    m22=Am22;    m23=Am23;    m24=Am24;
        m31=Am31;    m32=Am32;    m33=Am33;    m34=Am34;
        m41=Am41;    m42=Am42;    m43=Am43;    m44=Am44;
    }

    matrix4 (const matrix4 &m)
    {
        m11=m.m11;    m12=m.m12;    m13=m.m13;    m14=m.m14;
        m21=m.m21;    m22=m.m22;    m23=m.m23;    m24=m.m24;
        m31=m.m31;    m32=m.m32;    m33=m.m33;    m34=m.m34;
        m41=m.m41;    m42=m.m42;    m43=m.m43;    m44=m.m44;
    }

    void GetRow (float* row,int number)
    {
        switch (number)
        {
        case 1:
            row[0]=m11; row[1]=m12; row[2]=m13; row[3]=m14;
            break;
        case 2:
            row[0]=m21; row[1]=m22; row[2]=m23; row[3]=m24;
            break;
        case 3:
            row[0]=m31; row[1]=m32; row[2]=m33; row[3]=m34;
            break;
        case 4:
            row[0]=m41; row[1]=m42; row[2]=m43; row[3]=m44;
            break;
        }
    }
    void GetColumn (float* column, int number)
    {
        switch (number)
        {
        case 1:
            column[0]=m11; column[1]=m21; column[2]=m31; column[3]=m41;
            break;
        case 2:
            column[0]=m12; column[1]=m22; column[2]=m32; column[3]=m42;
            break;
        case 3:
            column[0]=m13; column[1]=m23; column[2]=m33; column[3]=m43;
            break;
        case 4:
            column[0]=m14; column[1]=m24; column[2]=m34; column[3]=m44;
            break;
        }
    }

    matrix4 operator* (const matrix4 &m)
    {
        matrix4 c;

        c.m11 = m11*m.m11 + m12*m.m21 + m13*m.m31 + m14*m.m41;
        c.m12 = m11*m.m12 + m12*m.m22 + m13*m.m32 + m14*m.m42;
        c.m13 = m11*m.m13 + m12*m.m23 + m13*m.m33 + m14*m.m43;
        c.m14 = m11*m.m14 + m12*m.m24 + m13*m.m34 + m14*m.m44;

        c.m21 = m21*m.m11 + m22*m.m21 + m23*m.m31 + m24*m.m41;
        c.m22 = m21*m.m12 + m22*m.m22 + m23*m.m32 + m24*m.m42;
        c.m23 = m21*m.m13 + m22*m.m23 + m23*m.m33 + m24*m.m43;
        c.m24 = m21*m.m14 + m22*m.m24 + m23*m.m34 + m24*m.m44;

        c.m31 = m31*m.m11 + m32*m.m21 + m33*m.m31 + m34*m.m41;
        c.m32 = m31*m.m12 + m32*m.m22 + m33*m.m32 + m34*m.m42;
        c.m33 = m31*m.m13 + m32*m.m23 + m33*m.m33 + m34*m.m43;
        c.m34 = m31*m.m14 + m32*m.m24 + m33*m.m34 + m34*m.m44;

        c.m41 = m41*m.m11 + m42*m.m21 + m43*m.m31 + m44*m.m41;
        c.m42 = m41*m.m12 + m42*m.m22 + m43*m.m32 + m44*m.m42;
        c.m43 = m41*m.m13 + m42*m.m23 + m43*m.m33 + m44*m.m43;
        c.m44 = m41*m.m14 + m42*m.m24 + m43*m.m34 + m44*m.m44;
        return c;
    }

    float Det ()
    {
        float
            fA0 = m[ 0]*m[ 5] - m[ 1]*m[ 4],
            fA1 = m[ 0]*m[ 6] - m[ 2]*m[ 4],
            fA2 = m[ 0]*m[ 7] - m[ 3]*m[ 4],
            fA3 = m[ 1]*m[ 6] - m[ 2]*m[ 5],
            fA4 = m[ 1]*m[ 7] - m[ 3]*m[ 5],
            fA5 = m[ 2]*m[ 7] - m[ 3]*m[ 6],
            fB0 = m[ 8]*m[13] - m[ 9]*m[12],
            fB1 = m[ 8]*m[14] - m[10]*m[12],
            fB2 = m[ 8]*m[15] - m[11]*m[12],
            fB3 = m[ 9]*m[14] - m[10]*m[13],
            fB4 = m[ 9]*m[15] - m[11]*m[13],
            fB5 = m[10]*m[15] - m[11]*m[14];

        return (fA0*fB5-fA1*fB4+fA2*fB3+fA3*fB2-fA4*fB1+fA5*fB0);

    }
};
inline vec3 operator*( const matrix4& m, const vec3 &v )
{
    return vec3 (m.m11 * v.x + m.m12 * v.y + m.m13 * v.z + m.m14,  // x
                 m.m21 * v.x + m.m22 * v.y + m.m23 * v.z + m.m24,  // y
                 m.m31 * v.x + m.m32 * v.y + m.m33 * v.z + m.m34); // z
}
inline vec3 operator*( const vec3 &v, const matrix4& m )
{
    return v*m;
}
inline void operator*=( vec3& v, const matrix4& m )
{
    v=m*v;
}
inline void operator*=( matrix4& m1, const matrix4& m2 )
{
    m1=m1*m2;
}
inline matrix4 matID()
{
    matrix4 m;

    m.m11=1;    m.m12=0;    m.m13=0;    m.m14=0;
    m.m21=0;    m.m22=1;    m.m23=0;    m.m24=0;
    m.m31=0;    m.m32=0;    m.m33=1;    m.m34=0;
    m.m41=0;    m.m42=0;    m.m43=0;    m.m44=1;

    return m;
}
inline matrix4 matInverse(const matrix4 &m)
{
    float
            fA0 = m.m[ 0]*m.m[ 5] - m.m[ 1]*m.m[ 4],
            fA1 = m.m[ 0]*m.m[ 6] - m.m[ 2]*m.m[ 4],
            fA2 = m.m[ 0]*m.m[ 7] - m.m[ 3]*m.m[ 4],
            fA3 = m.m[ 1]*m.m[ 6] - m.m[ 2]*m.m[ 5],
            fA4 = m.m[ 1]*m.m[ 7] - m.m[ 3]*m.m[ 5],
            fA5 = m.m[ 2]*m.m[ 7] - m.m[ 3]*m.m[ 6],
            fB0 = m.m[ 8]*m.m[13] - m.m[ 9]*m.m[12],
            fB1 = m.m[ 8]*m.m[14] - m.m[10]*m.m[12],
            fB2 = m.m[ 8]*m.m[15] - m.m[11]*m.m[12],
            fB3 = m.m[ 9]*m.m[14] - m.m[10]*m.m[13],
            fB4 = m.m[ 9]*m.m[15] - m.m[11]*m.m[13],
            fB5 = m.m[10]*m.m[15] - m.m[11]*m.m[14];

    float det = fA0 * fB5 - fA1 * fB4 + fA2 * fB3 + fA3 * fB2 - fA4 * fB1 + fA5 * fB0;

    if (det == 0) // not invertible
        return matrix4 (0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);

    matrix4 kInv;
    kInv.m[ 0] =  m.m[ 5]*fB5 - m.m[ 6]*fB4 + m.m[ 7]*fB3;
    kInv.m[ 4] = -m.m[ 4]*fB5 + m.m[ 6]*fB2 - m.m[ 7]*fB1;
    kInv.m[ 8] =  m.m[ 4]*fB4 - m.m[ 5]*fB2 + m.m[ 7]*fB0;
    kInv.m[12] = -m.m[ 4]*fB3 + m.m[ 5]*fB1 - m.m[ 6]*fB0;
    kInv.m[ 1] = -m.m[ 1]*fB5 + m.m[ 2]*fB4 - m.m[ 3]*fB3;
    kInv.m[ 5] =  m.m[ 0]*fB5 - m.m[ 2]*fB2 + m.m[ 3]*fB1;
    kInv.m[ 9] = -m.m[ 0]*fB4 + m.m[ 1]*fB2 - m.m[ 3]*fB0;
    kInv.m[13] =  m.m[ 0]*fB3 - m.m[ 1]*fB1 + m.m[ 2]*fB0;
    kInv.m[ 2] =  m.m[13]*fA5 - m.m[14]*fA4 + m.m[15]*fA3;
    kInv.m[ 6] = -m.m[12]*fA5 + m.m[14]*fA2 - m.m[15]*fA1;
    kInv.m[10] =  m.m[12]*fA4 - m.m[13]*fA2 + m.m[15]*fA0;
    kInv.m[14] = -m.m[12]*fA3 + m.m[13]*fA1 - m.m[14]*fA0;
    kInv.m[ 3] = -m.m[ 9]*fA5 + m.m[10]*fA4 - m.m[11]*fA3;
    kInv.m[ 7] =  m.m[ 8]*fA5 - m.m[10]*fA2 + m.m[11]*fA1;
    kInv.m[11] = -m.m[ 8]*fA4 + m.m[ 9]*fA2 - m.m[11]*fA0;
    kInv.m[15] =  m.m[ 8]*fA3 - m.m[ 9]*fA1 + m.m[10]*fA0;

    float fInvDet = ((float)1.0) / det;
    kInv.m[ 0] *= fInvDet;
    kInv.m[ 1] *= fInvDet;
    kInv.m[ 2] *= fInvDet;
    kInv.m[ 3] *= fInvDet;
    kInv.m[ 4] *= fInvDet;
    kInv.m[ 5] *= fInvDet;
    kInv.m[ 6] *= fInvDet;
    kInv.m[ 7] *= fInvDet;
    kInv.m[ 8] *= fInvDet;
    kInv.m[ 9] *= fInvDet;
    kInv.m[10] *= fInvDet;
    kInv.m[11] *= fInvDet;
    kInv.m[12] *= fInvDet;
    kInv.m[13] *= fInvDet;
    kInv.m[14] *= fInvDet;
    kInv.m[15] *= fInvDet;

    return kInv;
}
inline matrix4 matTranspose (const matrix4 &m)
{
    matrix4 t;

    t.m11=m.m11;    t.m12=m.m21;    t.m13=m.m31;    t.m14=m.m41;
    t.m21=m.m12;    t.m22=m.m22;    t.m23=m.m32;    t.m24=m.m42;
    t.m31=m.m13;    t.m32=m.m23;    t.m33=m.m33;    t.m34=m.m43;
    t.m41=m.m14;    t.m42=m.m24;    t.m43=m.m43;    t.m44=m.m44;

    return t;
}
inline matrix4 matRotX( const float angle )// radians
{
    matrix4 m;
    const float c=cos(angle);
    const float s=sin(angle);

    m.m11=1;    m.m12=0;    m.m13=0;    m.m14=0;
    m.m21=0;    m.m22=c;    m.m23=-s;    m.m24=0;
    m.m31=0;    m.m32=s;    m.m33=c;    m.m34=0;
    m.m41=0;    m.m42=0;    m.m43=0;    m.m44=1;

    return m;
}
inline matrix4 matRotY( const float angle )// radians
{
    matrix4 m;
    const float c=cos(angle);
    const float s=sin(angle);

    m.m11=c;    m.m12=0;    m.m13=s;    m.m14=0;
    m.m21=0;    m.m22=1;    m.m23=0;    m.m24=0;
    m.m31=-s;    m.m32=0;    m.m33=c;    m.m34=0;
    m.m41=0;    m.m42=0;    m.m43=0;    m.m44=1;

    return m;
}
inline matrix4 matRotZ( const float angle )// radians
{
    matrix4 m;
    const float c=cos(angle);
    const float s=sin(angle);

    m.m11=c;    m.m12=-s;    m.m13=0;    m.m14=0;
    m.m21=s;    m.m22=c;    m.m23=0;    m.m24=0;
    m.m31=0;    m.m32=0;    m.m33=1;    m.m34=0;
    m.m41=0;    m.m42=0;    m.m43=0;    m.m44=1;

    return m;
}
inline matrix4 matQuat(const Quaternion &q)
{
    float f = 2.0f / q.Length();

    matrix4 m;
    float y2 = q.y*q.y, z2 = q.z*q.z, x2 = q.x*q.x,
          xy = q.x*q.y, xz = q.x*q.z, yz = q.y*q.z,
          xw = q.x*q.w, yw = q.y*q.w, zw = q.z*q.w;

    m.m11=1.0f-f*(y2+z2); m.m12=     f*(xy+zw); m.m13=     f*(xz-yw); m.m14=0;
    m.m21=     f*(xy-zw); m.m22=1.0f-f*(x2+z2); m.m23=     f*(yz+xw); m.m24=0;
    m.m31=     f*(xz+yw); m.m32=     f*(yz-xw); m.m33=1.0f-f*(x2+y2); m.m34=0;
    m.m41=     0;         m.m42=     0;         m.m43=     0;         m.m44=1;

    return m;
}
inline matrix4 matRotAxis(const vec3& axis, float angle)// radians
{
    matrix4 m;
    vec3 a=axis.Unit();
    const float c = cos (angle),
                ivc=1.0f - c,
                s = sin (angle),
                y2 = a.y * a.y,
                x2 = a.x * a.x,
                z2 = a.z * a.z,
                xy = a.x * a.y,
                xz = a.x * a.z,
                yz = a.y * a.z;

    m.m11 = ivc*x2 + c;        m.m12 = ivc*xy - s*a.z;    m.m13 = ivc*xz + s*a.y;    m.m14 = 0;
    m.m21 = ivc*xy + s*a.z;    m.m22 = ivc*y2 + c;        m.m23 = ivc*yz - s*a.x;    m.m24 = 0;
    m.m31 = ivc*xz - s*a.y;    m.m32 = ivc*yz + s*a.x;    m.m33 = ivc*z2 + c;        m.m34 = 0;
    m.m41 = 0;                m.m42 = 0;                m.m43 = 0;                m.m44 = 1;

    return m;
}
inline matrix4 matScale(const float x,const float y,const float z)
{
    matrix4 m;

    m.m11=x;    m.m12=0;    m.m13=0;    m.m14=0;
    m.m21=0;    m.m22=y;    m.m23=0;    m.m24=0;
    m.m31=0;    m.m32=0;    m.m33=z;    m.m34=0;
    m.m41=0;    m.m42=0;    m.m43=0;    m.m44=1;

    return m;
}
inline matrix4 matTranslation( const float x, const float y, const float z )
{
    matrix4 m;

    m.m11=1;    m.m12=0;    m.m13=0;    m.m14=x;
    m.m21=0;    m.m22=1;    m.m23=0;    m.m24=y;
    m.m31=0;    m.m32=0;    m.m33=1;    m.m34=z;
    m.m41=0;    m.m42=0;    m.m43=0;    m.m44=1;

    return m;
}
inline matrix4 matTranslation( const vec3& v )
{
    matrix4 m;

    m.m11=1;    m.m12=0;    m.m13=0;    m.m14=v.x;
    m.m21=0;    m.m22=1;    m.m23=0;    m.m24=v.y;
    m.m31=0;    m.m32=0;    m.m33=1;    m.m34=v.z;
    m.m41=0;    m.m42=0;    m.m43=0;    m.m44=1;

    return m;
}
inline matrix4 matLookAt( const vec3& eye, const vec3& lookat, const vec3& up )
{
    vec3 _right, _up, _forward;
    matrix4 m;

    _forward = lookat - eye;        _forward = _forward.Unit();
    _right = Cross(up,_forward);    _right = _right.Unit();
    _up = Cross(_forward,_right);   _up = _up.Unit();

    m.m11=_right.x;     m.m12=_right.y;     m.m13=_right.z;     m.m14=-Dot(_right,eye);
    m.m21=_up.x;        m.m22=_up.y;        m.m23=_up.z;        m.m24=-Dot(_up,eye);
    m.m31=_forward.x;   m.m32=_forward.y;   m.m33=_forward.z;   m.m34=-Dot(_forward,eye);
    m.m41=0.0f;         m.m42=0.0f;         m.m43=0.0f;         m.m44=1.0f;

    return m;

}
// "matSkew" requires a plane normal and plane d input.
inline matrix4 matSkew( const vec3 n/*normalized*/, float d )
{
    matrix4 m;
    // distance=p*n+d
    // sp=p-2*distance*n=p-2*(p*n+d)*n=p - 2*(p*n)*n - 2*d*n

    // four initial points: X,Y,Z and O(the origin)
    // new axis system is created with
    // four new points: SX, SY, SZ and SO(new origin)

    // X=(1,0,0) Y=(0,1,0) Z=(0,0,1) O=(0,0,0)

    // In normal space:
    // SO=O - 2*(O*n)*n - 2*d*n = (0,0,0) - 2*((0,0,0)*n)*n - 2*d*n = -2*0*n - 2*d*n = -2*d*n
    // SX=X - 2*(X*n)*n - 2*d*n = (1,0,0) - 2*((1,0,0)*n)*n - 2*d*n = (1,0,0) - 2*n.x*n - 2*d*n =

    // (1,0,0) - 2*(n.x+d)*n = ( 1-2*(n.x+d)*n.x , -2*(n.x+d)*n.y , -2*(n.x+d)*n.z )
    //                    SY    =    ( -2*(n.y+d)*n.x , 1-2*(n.y+d)*n.y , -2*(n.y+d)*n.z )
    //                    SZ    =    ( -2*(n.z+d)*n.x , -2*(n.z+d)*n.y , 1-2*(n.z+d)*n.z )

    m.m11=1.0f    -2*n.x*n.x;        m.m12=        -2*n.x*n.y;        m.m13=        -2*n.x*n.z;        m.m14=    2*d*(m.m11*n.x + m.m12*n.y + m.m13*n.z);
    m.m21=        -2*n.y*n.x;        m.m22=1.0f    -2*n.y*n.y;        m.m23=        -2*n.y*n.z;        m.m24=    2*d*(m.m21*n.x + m.m22*n.y + m.m23*n.z);
    m.m31=        -2*n.z*n.x;        m.m32=        -2*n.z*n.y;        m.m33=1.0f    -2*n.z*n.z;        m.m34=    2*d*(m.m31*n.x + m.m32*n.y + m.m33*n.z);
    m.m41=        0;                m.m42=        0;                m.m43=        0;                m.m44=    1.0f;

    return m;
}
inline matrix4 matPerspec(float view_angle,float aspect_ratio,float near_viewdist,float far_viewdist)
{
    float a=0.5f*view_angle;
    float f=1.0f/tan(a);
    float zdiff=far_viewdist-near_viewdist;

    matrix4 m=matID();

    m.m11=f/aspect_ratio;
    m.m22=f;
    m.m33=(near_viewdist+far_viewdist)/zdiff;
    m.m34=-2*near_viewdist*far_viewdist/zdiff;
    m.m43=1.0f;
    m.m44=0.0f;

    return m;
}
inline matrix4 matOrtho(float leftX, float rightX, float upY, float downY, float nearZ, float farZ)
{
    float Xdiff = rightX - leftX,
          Ydiff = upY - downY,
          Zdiff = farZ - nearZ;

    matrix4 m=matID();

    m.m11 = 2.0f / Xdiff;
    m.m22 = 2.0f / Ydiff;
    m.m33 = 2.0f / Zdiff;
    m.m14 = -(rightX + leftX) / Xdiff;
    m.m24 = -(upY + downY) / Ydiff;
    m.m34 = -(farZ + nearZ) / Zdiff;

    return m;
}

#endif // MATRIX_H
