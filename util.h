#ifndef __BITFREE_UTIL_H__
#define __BITFREE_UTIL_H__

#ifndef BEGIN
#define BEGIN(a)            ((char*)&(a))
#endif
#ifndef END
#define END(a)              ((char*)&((&(a))[1]))
#endif

#if !HAVE_DECL_BE32ENC
static inline void be32enc(void *pp, uint32_t x)
{
    uint8_t *p = (uint8_t *)pp;
    p[3] = x & 0xff;
    p[2] = (x >> 8) & 0xff;
    p[1] = (x >> 16) & 0xff;
    p[0] = (x >> 24) & 0xff;
}
#endif

#endif
