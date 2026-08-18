// P3 : sans NJ_P3_JIT, pas de compilation en memoire (p3_jit.cpp et p3_host.cpp sinon).
extern "C" void nj_p3_jit_start() {}
extern "C" bool nj_p3_host_open() { return false; }
