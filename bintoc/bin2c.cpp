// bin2c.cpp
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <stdio.h>
#include <assert.h>
#include <string>

int main(int argc, char** argv) {

	assert (argc == 2);
	
  std::string root = argv[1];
	root.erase (root.find_last_of('.'), std::string::npos);

	std::string symbol = root;
	auto slash = root.find_last_of('\\');
	symbol.erase (0, slash+1);

	FILE* f = fopen ((root + ".bmp").c_str(), "rb");
	FILE* of = fopen ((root + ".h").c_str(), "w");

	fprintf (of, "#pragma once\n");
	fprintf (of, "#include <stdint.h>\n");
	fprintf (of, "const uint8_t %s[] {\n", symbol.c_str());

	unsigned long n = 0;
	while (!feof (f)) {
		unsigned char c;
		if (fread (&c, 1, 1, f) == 0)
			break;

		if (n % 16 == 0)
			fprintf(of, "  ");
		fprintf (of, "0x%.2X,", (int)c);

		++n;
		if (n % 16 == 0) 
			fprintf (of, "\n");
			
		}

	fprintf (of, "};\n");
	fclose (of);

	printf ("converted %s length %d to symbol %s\n", root.c_str(), n, symbol.c_str());

	fclose (f);
	}
