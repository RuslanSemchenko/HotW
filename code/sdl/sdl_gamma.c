#ifdef USE_LOCAL_HEADERS
#	include "SDL.h"
#else
#	include <SDL3/SDL.h>
#endif

#include "../renderercommon/tr_common.h"
#include "../qcommon/qcommon.h"

/*
=================
GLimp_SetGamma
=================
*/
void GLimp_SetGamma(unsigned char red[256], unsigned char green[256], unsigned char blue[256])
{
    // SDL3: SDL_SetWindowGammaRamp удалена.
    // Управление гаммой теперь должно осуществляться через шейдеры или sRGB фреймбуферы
    // внутри самого рендерера, а не через платформенный уровень.

    // Мы оставляем эту функцию пустой, чтобы удовлетворить требования линковщика.
    // Параметры помечаем как неиспользуемые, чтобы не было предупреждений.

    (void)red;
    (void)green;
    (void)blue;

    if (!glConfig.deviceSupportsGamma || r_ignorehwgamma->integer > 0) {
        return;
    }

    // Если в будущем вы захотите реализовать гамму, это нужно делать не здесь,
    // а в tr_backend.c, загружая текстуру цветокоррекции или используя фрагментный шейдер.

    // ri.Printf( PRINT_DEVELOPER, "GLimp_SetGamma: Hardware gamma not supported in SDL3. Use shader based gamma.\n" );
}