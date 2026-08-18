#include "common/platform.h"

namespace nj {
// Un seul emplacement, defini par la coque de l'application au demarrage. Vide sur desktop :
// les outils de debug continuent alors d'ecrire la ou ils ecrivaient.
std::string &writable_dir_storage() {
    static std::string dir;
    return dir;
}
}  // namespace nj
