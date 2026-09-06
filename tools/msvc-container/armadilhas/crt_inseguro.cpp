// Terceira classe de armadilha, achada por mim ao testar as duas que o
// time pediu (nao e do commit citado - as duas do commit nao acenderam
// nem no cl.exe puro, ver relatorio): chamada a uma funcao de CRT que a
// Microsoft marca como insegura/obsoleta na especializacao dela, mesmo
// sendo padrao ISO C. GCC/MinGW nunca avisa disto por padrao; o cabecalho
// da Microsoft (corecrt_wstdio.h/corecrt_stdio_config etc.) marca a
// funcao com _CRT_INSECURE_DEPRECATE, e o cl.exe emite C4996 avisando
// para usar a variante *_s.
#include <cstdio>
#include <cstring>

void copy_and_print(char *dst, const char *src) {
    std::strcpy(dst, src);
    std::sprintf(dst, "%s", src);
}
