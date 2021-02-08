#ifndef ZHC_TYPES_H
#define ZHC_TYPES_H

struct Rect
{
    union
    {
        int32 x,y,w,h;
        int32 left,top,right,bottom;
    };
};

Rect
rect(int32 x, int32 y, int32 w, int32 h)
{
    Rect result = {};
    result.x = x;
    result.y = y;
    result.w = w;
    result.h = h;

    return(result);
}

#endif // ZHC_TYPES_H
