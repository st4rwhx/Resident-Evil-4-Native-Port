// -*- coding: utf-8 -*-
// Les threads du GameCube, et le point ou le portage cesse de pouvoir tricher.
//
// RE4 a son propre ordonnanceur de taches (`src/game/scheduler.cpp`), mais celui-ci est bati
// **sur les threads du SDK** : `TaskExit` appelle `OSExitThread`, `TaskSleep` appelle
// `OSSuspendThread`, et le chargement depuis le disque rend la main par `iTaskExit`. Tant que
// ces fonctions etaient des souches vides, la premiere tache terminee revenait dans une pile
// morte - c'est exactement la faute observee : une lecture nulle a l'offset 0x30C, qui est le
// champ `error` d'OSThread, donc un OSThread nul.
//
// **Modele : cooperatif, un seul thread actif a la fois.** Les threads du GameCube sont
// preemptibles en theorie, mais RE4 s'en sert comme de coroutines : il suspend, reprend, et
// protege ses sections critiques par OSDisableInterrupts - qui n'arrete rien s'il y a du vrai
// parallelisme. Faire tourner ces threads reellement en parallele introduirait des courses que
// le jeu n'a jamais eu a supporter. On garde donc un **jeton unique** : exactement un thread
// s'execute, les autres attendent leur tour sur leur propre condition.
//
// Les threads sont des pthreads plutot qu'un basculement de contexte ecrit a la main, pour deux
// raisons : Bionic ne fournit pas `makecontext`/`swapcontext`, et une pile geree par le systeme
// se porte sur iOS sans une ligne d'assembleur.
//
// Consequence a connaitre : la pile que le jeu alloue et passe a OSCreateThread n'est **pas**
// utilisee. On la renseigne quand meme dans l'objet thread, parce que le jeu la remplit d'un
// motif et verifie le debordement.
#include <pthread.h>

#include <cstdio>
#include <cstring>

#include <dolphin.h>
#include <dolphin/os/OSSemaphore.h>

#include "port_log.h"
extern "C" void port_marquer_fil_coeur(void);   // arm_hooks.cpp

extern "C" void OSExitThread(void *val);
#if PORT_P64
extern "C" void* port_pile_fenetre(int slot, size_t* taille);   // port/os.cpp
#endif

namespace {

const int MAX_THREADS = 32;

struct PortThread {
    OSThread *os;
    pthread_cond_t cond;
    pthread_t tid;
    void *(*entry)(void *);
    void *param;
    bool used;
    bool started;
    bool finished;
    bool is_main;
    bool exited;          // N7 : sorti par OSExitThread (pthread joignable, pile de l'emplacement a recuperer)
    bool canceled;        // OSCancelThread (18/11) : le fil doit sortir des qu'il se reveille
};

pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
PortThread g_threads[MAX_THREADS];
PortThread *g_current = nullptr;   // le detenteur du jeton
PortThread *g_main = nullptr;
bool g_ready = false;

PortThread *find(OSThread *t)
{
    for (int i = 0; i < MAX_THREADS; ++i)
        if (g_threads[i].used && g_threads[i].os == t)
            return &g_threads[i];
    return nullptr;
}

PortThread *alloc_slot()
{
    for (int i = 0; i < MAX_THREADS; ++i)
        if (!g_threads[i].used) {
            PortThread &p = g_threads[i];
#if PORT_P64
            // La pile de cet emplacement (dans la fenetre) servait peut-etre a un fil qui finit son pthread_exit : on
            // attend qu'il l'ait quittee avant de la redonner.
            if (p.exited)
                pthread_join(p.tid, nullptr);
#endif
            std::memset(&p, 0, sizeof(p));
            pthread_cond_init(&p.cond, nullptr);
            p.used = true;
            return &p;
        }
    return nullptr;
}

bool runnable(const PortThread &p)
{
    return p.used && !p.finished && p.os &&
           p.os->suspend <= 0 &&
           (p.os->state == OS_THREAD_STATE_READY || p.os->state == OS_THREAD_STATE_RUNNING);
}

// Le prochain a qui donner le jeton : la priorite la plus forte (valeur la plus basse) parmi
// les executables, en excluant celui qui rend la main sauf s'il est seul.
PortThread *pick_next(PortThread *except)
{
    PortThread *best = nullptr;
    for (int i = 0; i < MAX_THREADS; ++i) {
        PortThread &p = g_threads[i];
        if (&p == except || !runnable(p))
            continue;
        if (!best || p.os->priority < best->os->priority)
            best = &p;
    }
    if (!best && except && runnable(*except))
        best = except;
    return best;
}

void ensure_main()
{
    if (g_ready)
        return;
    g_ready = true;
    // Le fil d'execution qui appelle main() est lui aussi un thread du jeu : sans cela, la
    // premiere suspension n'aurait personne a qui rendre la main.
    static OSThread main_os;
    std::memset(&main_os, 0, sizeof(main_os));
    main_os.state = OS_THREAD_STATE_RUNNING;
    main_os.priority = 16;
    main_os.base = 16;
    PortThread *p = alloc_slot();
    p->os = &main_os;
    p->is_main = true;
    p->started = true;
    p->tid = pthread_self();
    g_main = p;
    g_current = p;
}

// Le SDK reordonnance **immediatement** quand un thread plus prioritaire devient executable :
// c est ce qui fait qu apres `OSResumeThread(&t->thread)` le corps de la tache s execute avant
// le retour de l ordonnanceur. Sans cela, le jeu croit avoir lance sa tache, verifie aussitot
// sa pile, et tout le systeme de taches tourne a vide.
void reschedule_if_higher();

// Fil annule (OSCancelThread) reveille dans une attente du jeton : il rend son emplacement et sort pour de bon (le SDK fait
// de meme). Sans cela, a la remise a zero (TaskAllClear) les taches endormies restaient attachees a leur OSThread, que le jeu
// reutilise aussitot pour la tache du titre : le titre ne repartait jamais (ecran noir apres un GAME RESET en partie, 18/11).
[[noreturn]] void cancel_exit_locked(PortThread *self)
{
    self->used = false;
    self->exited = true;
    pthread_mutex_unlock(&g_lock);
    pthread_exit(nullptr);
}

// Rend le jeton et attend de le recevoir. A appeler avec g_lock tenu.
void yield_locked(PortThread *self)
{
    PortThread *next = pick_next(self);
    if (!next || next == self)
        return;   // personne d'autre ne peut avancer : on garde la main

    if (self && !self->finished && self->os && self->os->state == OS_THREAD_STATE_RUNNING)
        self->os->state = OS_THREAD_STATE_READY;
    g_current = next;
    if (next->os)
        next->os->state = OS_THREAD_STATE_RUNNING;
    pthread_cond_signal(&next->cond);

    if (!self)
        return;
    while (g_current != self) {
        if (self->canceled)
            cancel_exit_locked(self);
        pthread_cond_wait(&self->cond, &g_lock);
    }
    if (self->os)
        self->os->state = OS_THREAD_STATE_RUNNING;
}

void reschedule_if_higher()
{
    if (!g_current || !g_current->os)
        return;
    PortThread *best = pick_next(g_current);
    if (best && best != g_current && best->os &&
        best->os->priority < g_current->os->priority)
        yield_locked(g_current);
}

void *trampoline(void *arg)
{
    port_marquer_fil_coeur();   // iOS : fil du jeu (tas du jeu), pas un fil du moteur
    PortThread *self = (PortThread *) arg;
    pthread_mutex_lock(&g_lock);
    while (g_current != self) {
        if (self->canceled)
            cancel_exit_locked(self);
        pthread_cond_wait(&self->cond, &g_lock);
    }
    pthread_mutex_unlock(&g_lock);

    void *ret = self->entry ? self->entry(self->param) : nullptr;

    // Une fonction de thread qui rend la main se termine comme si elle avait appele
    // OSExitThread : c'est ce que fait le SDK, et le jeu s'appuie dessus.
    OSExitThread(ret);
    return nullptr;
}

} // namespace

extern "C" {

int OSCreateThread(OSThread *thread, void *(*func)(void *), void *param, void *stack,
                   u32 stackSize, OSPriority priority, u16 attr)
{
    pthread_mutex_lock(&g_lock);
    ensure_main();

    PortThread *p = alloc_slot();
    if (!p || !thread) {
        pthread_mutex_unlock(&g_lock);
        return 0;
    }
    std::memset(thread, 0, sizeof(*thread));
    thread->state = OS_THREAD_STATE_READY;
    thread->attr = attr;
    thread->priority = priority;
    thread->base = priority;
    // Le SDK cree un thread **deja suspendu** : c'est OSResumeThread qui le lance. Manquer cela
    // ferait partir la tache avant que le jeu n'ait fini de la decrire.
    thread->suspend = 1;
    thread->stackBase = (u8 *) stack;
    thread->stackEnd = (u32 *) ((u8 *) stack - stackSize);
    // Le SDK ecrit 0xDEADBABE au bout de la pile, et `StackOverflowCheck` du jeu le relit a
    // **chaque tour d ordonnanceur** : sans ce mot, le jeu conclut a un debordement et appelle
    // OSPanic des la premiere tache. La pile elle-meme n est pas la notre (pthread en fournit
    // une), mais le temoin doit y etre.
    if (thread->stackEnd)
        *thread->stackEnd = OS_THREAD_STACK_MAGIC;
    OSInitThreadQueue(&thread->queueJoin);

    p->os = thread;
    p->entry = func;
    p->param = param;

    pthread_attr_t at;
    pthread_attr_init(&at);
#if PORT_P64
    {   // pile dans la fenetre de 4 Go (port/os.cpp port_pile_fenetre)
        size_t t = 0;
        void* pile = port_pile_fenetre((int) (p - g_threads), &t);
        pthread_attr_setstack(&at, pile, t);
    }
#endif
    const int cree = pthread_create(&p->tid, &at, trampoline, p);
    pthread_attr_destroy(&at);
    if (cree != 0) {
        p->used = false;
        pthread_mutex_unlock(&g_lock);
        std::printf("[PORT] OSCreateThread : creation impossible\n");
        return 0;
    }
    p->started = true;
    pthread_mutex_unlock(&g_lock);
    return 1;
}

s32 OSResumeThread(OSThread *thread)
{
    pthread_mutex_lock(&g_lock);
    ensure_main();
    s32 prev = 0;
    if (thread) {
        prev = thread->suspend;
        if (thread->suspend > 0)
            --thread->suspend;
        if (thread->suspend <= 0 && thread->state != OS_THREAD_STATE_MORIBUND) {
            thread->state = OS_THREAD_STATE_READY;
            reschedule_if_higher();
        }
    }
    pthread_mutex_unlock(&g_lock);
    return prev;
}

s32 OSSuspendThread(OSThread *thread)
{
    pthread_mutex_lock(&g_lock);
    ensure_main();
    s32 prev = 0;
    if (thread) {
        prev = thread->suspend;
        ++thread->suspend;
        PortThread *p = find(thread);
        if (p && p == g_current)
            yield_locked(p);   // se suspendre soi-meme rend la main immediatement
    }
    pthread_mutex_unlock(&g_lock);
    return prev;
}

void OSExitThread(void *val)
{
    pthread_mutex_lock(&g_lock);
    ensure_main();
    PortThread *self = g_current;
    if (self && self->os) {
        self->os->val = val;
        self->os->state = OS_THREAD_STATE_MORIBUND;
        // Reveiller ceux qui attendaient la fin de ce thread.
        OSThreadQueue *q = &self->os->queueJoin;
        for (OSThread *t = q->head; t; t = t->link.next)
            if (t->state == OS_THREAD_STATE_WAITING)
                t->state = OS_THREAD_STATE_READY;
        q->head = q->tail = nullptr;
    }
    if (self) {
        self->finished = true;
        PortThread *next = pick_next(self);
        if (next) {
            g_current = next;
            if (next->os)
                next->os->state = OS_THREAD_STATE_RUNNING;
            pthread_cond_signal(&next->cond);
        } else {
            g_current = nullptr;
        }
        self->used = false;
        self->exited = !self->is_main;
    }
    const bool was_main = self && self->is_main;
    pthread_mutex_unlock(&g_lock);

    // Le thread principal ne peut pas se terminer : il porte le processus. Les autres sortent
    // reellement, ce que le jeu attend (OSExitThread ne revient jamais).
    if (!was_main)
        pthread_exit(nullptr);
}

void OSCancelThread(OSThread *thread)
{
    pthread_mutex_lock(&g_lock);
    if (thread) {
        thread->state = OS_THREAD_STATE_MORIBUND;
        if (PortThread *p = find(thread)) {
            p->finished = true;
            if (p != g_current && !p->is_main) {   // detache de son OSThread (le jeu le reutilise), reveille pour qu'il sorte
                p->canceled = true;
                p->os = nullptr;
                pthread_cond_signal(&p->cond);
            }
        }
    }
    pthread_mutex_unlock(&g_lock);
}

OSThread *OSGetCurrentThread(void)
{
    pthread_mutex_lock(&g_lock);
    ensure_main();
    OSThread *t = g_current ? g_current->os : (g_main ? g_main->os : nullptr);
    pthread_mutex_unlock(&g_lock);
    return t;
}

void OSYieldThread(void)
{
    pthread_mutex_lock(&g_lock);
    ensure_main();
    yield_locked(g_current);
    pthread_mutex_unlock(&g_lock);
}

BOOL OSIsThreadTerminated(OSThread *thread)
{
    return thread && thread->state == OS_THREAD_STATE_MORIBUND;
}

BOOL OSIsThreadSuspended(OSThread *thread) { return thread && thread->suspend > 0; }

OSPriority OSGetThreadPriority(OSThread *thread) { return thread ? thread->priority : 0; }

BOOL OSSetThreadPriority(OSThread *thread, OSPriority priority)
{
    if (!thread)
        return FALSE;
    thread->priority = priority;
    return TRUE;
}

// ---------------------------------------------------------------- files d'attente

void OSInitThreadQueue(OSThreadQueue *queue)
{
    if (queue)
        queue->head = queue->tail = nullptr;
}

void OSSleepThread(OSThreadQueue *queue)
{
    pthread_mutex_lock(&g_lock);
    ensure_main();
    PortThread *self = g_current;
    if (self && self->os && queue) {
        OSThread *t = self->os;
        t->state = OS_THREAD_STATE_WAITING;
        t->queue = queue;
        t->link.next = nullptr;
        t->link.prev = queue->tail;
        if (queue->tail)
            queue->tail->link.next = t;
        else
            queue->head = t;
        queue->tail = t;
        yield_locked(self);
    }
    pthread_mutex_unlock(&g_lock);
}

void OSWakeupThread(OSThreadQueue *queue)
{
    pthread_mutex_lock(&g_lock);
    if (queue) {
        for (OSThread *t = queue->head; t;) {
            OSThread *next = t->link.next;
            if (t->state == OS_THREAD_STATE_WAITING)
                t->state = OS_THREAD_STATE_READY;
            t->queue = nullptr;
            t->link.next = t->link.prev = nullptr;
            t = next;
        }
        queue->head = queue->tail = nullptr;
        reschedule_if_higher();
    }
    pthread_mutex_unlock(&g_lock);
}

// ---------------------------------------------------------------- semaphores

void OSInitSemaphore(OSSemaphore *sem, s32 count)
{
    if (!sem)
        return;
    sem->count = count;
    OSInitThreadQueue(&sem->queue);
}

s32 OSWaitSemaphore(OSSemaphore *sem)
{
    if (!sem)
        return 0;
    // Pas de boucle d'attente active : on rend la main tant que le compte est nul, ce qui
    // laisse le thread qui doit signaler avancer.
    while (sem->count <= 0)
        OSSleepThread(&sem->queue);
    return --sem->count;
}

s32 OSTryWaitSemaphore(OSSemaphore *sem)
{
    if (!sem || sem->count <= 0)
        return -1;
    return --sem->count;
}

s32 OSSignalSemaphore(OSSemaphore *sem)
{
    if (!sem)
        return 0;
    const s32 prev = sem->count++;
    OSWakeupThread(&sem->queue);
    return prev;
}

s32 OSGetSemaphoreCount(OSSemaphore *sem) { return sem ? sem->count : 0; }

}
