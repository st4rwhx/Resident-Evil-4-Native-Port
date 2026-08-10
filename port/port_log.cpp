#include "port_log.h"
#include <cstring>

// Sans STL : les en-tetes du decompile definissent des macros qui entrent en conflit avec
// libc++ (`<set>` echoue sur __throw_bad_array_new_length). Une table plate suffit, et un
// portage a de toute facon interet a ne pas trainer d'allocations dans sa journalisation.
namespace {
const char *g_seen[512];
int g_n = 0;
}

void port_todo(const char *fn)
{
    for (int i = 0; i < g_n; ++i)
        if (std::strcmp(g_seen[i], fn) == 0)
            return;
    if (g_n < (int)(sizeof(g_seen) / sizeof(g_seen[0]))) {
        g_seen[g_n++] = fn;
        std::printf("[PORT] non implemente : %s\n", fn);
    }
}
