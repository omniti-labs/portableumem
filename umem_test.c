#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef UMEM_STANDALONE
#  include "umem.h"
#else
#  include "umem_impl.h"
#endif

int main(int argc, char *argv[])
{
  char *foo;

#ifdef UMEM_STANDALONE
  umem_startup(NULL, 0, 0, NULL, NULL);
#endif

  foo = umem_alloc(32, UMEM_DEFAULT);

  strcpy(foo, "hello there");

  printf("Hello %s\n", foo);

  umem_free(foo, 32);

  return EXIT_SUCCESS;
}
