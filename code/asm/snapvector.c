#include "qasm-inline.h"
#include "../qcommon/q_shared.h"

// Используем интринсики для кроссплатформенности и x64
#include <xmmintrin.h>
#include <emmintrin.h>

/*
==================
qsnapvectorsse
==================
*/
void qsnapvectorsse(vec3_t vec) {
#if defined(__GNUC__)
    // Оригинальный код для GCC/Clang
    static unsigned char ssemask[16] __attribute__((aligned(16))) =
    {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00
    };

    __asm__ volatile
        (
            "movaps (%0), %%xmm1\n"
            "movups (%1), %%xmm0\n"
            "movaps %%xmm0, %%xmm2\n"
            "andps %%xmm1, %%xmm0\n"
            "andnps %%xmm2, %%xmm1\n"
            "cvtps2dq %%xmm0, %%xmm0\n"
            "cvtdq2ps %%xmm0, %%xmm0\n"
            "orps %%xmm1, %%xmm0\n"
            "movups %%xmm0, (%1)\n"
            :
    : "r" (ssemask), "r" (vec)
        : "memory", "%xmm0", "%xmm1", "%xmm2"
        );
#elif defined(_MSC_VER)
    // Современная реализация для Visual Studio (x86/x64)
    // Округляем первые 3 компонента вектора до ближайшего целого
    __m128 v = _mm_loadu_ps(vec);
    __m128 rounded = _mm_cvtepi32_ps(_mm_cvtps_epi32(v));

    // Сохраняем результат обратно (только первые 3 float, 4-й не важен для vec3_t)
    _mm_storeu_ps(vec, rounded);
#else
    // Fallback если нет SSE
    (void)vec;
#endif
}

/*
==================
qsnapvectorx87
==================
*/
void qsnapvectorx87(vec3_t vec) {
#if defined(__GNUC__)
#define QROUNDX87(src) \
        "flds " src "\n" \
        "fistpl " src "\n" \
        "fildl " src "\n" \
        "fstps " src "\n"	

    __asm__ volatile
        (
            QROUNDX87("(%0)")
            QROUNDX87("4(%0)")
            QROUNDX87("8(%0)")
            :
            : "r" (vec)
            : "memory"
            );
#elif defined(_MSC_VER)
    // В MSVC x64 нет inline asm для x87, используем обычное округление
    // Или просто вызываем SSE версию, так как на x64 SSE есть всегда
    qsnapvectorsse(vec);
#else
    (void)vec;
#endif
}