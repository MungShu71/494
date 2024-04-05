#include <stdio.h>
#include <stdlib.h>

int main(void)
{
  char key[5];
  int i, j;

  for (i = 0; i < 1000; ++i) {
    for (j = 0; j < 5; ++j) {
      key[j] = 33 + rand() % 126;
    }
    printf("I %s hello\n", key);
  }
}
