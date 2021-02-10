#ifndef ZHC_TYPES_H
#define ZHC_TYPES_H

struct V2
{
    int32 x,y;
};

inline V2 v2(int32 x, int32 y)
{
    V2 result = {};
    result.x = x;
    result.y = y;

    return(result);
}

struct V4
{
    union
    {
        int32 x,y,w,h;
        int32 left,top,right,bottom;
        real32 r,g,b,a;
    };
};

inline V4
rect(int32 x, int32 y, int32 w, int32 h)
{
    V4 result = {};
    result.x = x;
    result.y = y;
    result.w = w;
    result.h = h;

    return(result);
}

#endif // ZHC_TYPES_H
