// Filigrane de la beta fermee (docs/SECURITE_ET_JURIDIQUE.md, P5) : 16 octets propres au testeur
// pour qui ce build a ete fabrique (-DNIGHTJAR_TESTER=<nom>), calcules par CMake a partir d'un
// secret garde hors du depot. Rien ne les affiche ; tools/p3/filigrane.py retrouve le testeur
// d'un executable. Build sans testeur : zeros.
#ifndef NJ_BUILD_TAG
#define NJ_BUILD_TAG 0
#endif

extern "C" __attribute__((used)) const volatile unsigned char nj_build_tag[16] = {NJ_BUILD_TAG};
