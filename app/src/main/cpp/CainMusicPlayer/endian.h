//
// Created by admin on 2018/4/9.
//

#ifndef CAINPLAYER_ENDIAN_H
#define CAINPLAYER_ENDIAN_H
#define SDL_LIL_ENDIAN  1234
#define SDL_BIG_ENDIAN  4321

#ifndef SDL_BYTEORDER           /* Not defined in SDL_config.h? */
#ifdef __linux__
#include <endian.h>
#define SDL_BYTEORDER  __BYTE_ORDER
#else /* __linux __ */
#if defined(__hppa__) || \
    defined(__m68k__) || defined(mc68000) || defined(_M_M68K) || \
    (defined(__MIPS__) && defined(__MISPEB__)) || \
    defined(__ppc__) || defined(__POWERPC__) || defined(_M_PPC) || \
    defined(__sparc__)
#define SDL_BYTEORDER   SDL_BIG_ENDIAN
#else
#define SDL_BYTEORDER   SDL_LIL_ENDIAN
#endif
#endif /* __linux __ */
#endif /* !SDL_BYTEORDER */

#endif //CAINPLAYER_ENDIAN_H
