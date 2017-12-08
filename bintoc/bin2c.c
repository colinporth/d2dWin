// bin2c.c
#include <stdio.h>
#include <assert.h>

int main(int argc, char** argv) {
	assert (argc == 3);

	char* fn = argv[1];
	char* ofn = argv[2];

  FILE* f = fopen(fn, "rb");
  FILE* of = fopen(ofn, "w");

  printf ("char a[] = {\n");
  fprintf (of, "char a[] = {\n");

	unsigned long n = 0;
  	
	while (!feof(f)) {
		unsigned char c;
		if (fread (&c, 1, 1, f) == 0)
			break;

    if (n % 16 == 0) {
      printf("  ");
      fprintf(of, "  ");
      }

		printf ("0x%.2X,", (int)c);
		fprintf (of, "0x%.2X,", (int)c);

		++n;
		if (n % 16 == 0) {
			printf("\n");
			fprintf (of, "\n");
			}
		}

	fclose (f);
	printf ("};\n");
	printf ("%d,", n);

	fprintf (of, "};\n");
	fprintf (of, "%d,", n);
	fclose (of);
}
