#!/bin/bash
# polaris_retry.sh <commande Polaris...> : relance la commande jusqu'a 8 fois.
# Polaris tire des nombres au hasard a chaque passage ; environ une fois sur trois, le backend ARM 32 bits refuse le
# resultat (« error in backend: Do not know how to expand this operator's operand! », nj_emit.cpp, 16/10). Une autre
# tirette passe : le brouillage reste complet, seul le tirage change.
for i in 1 2 3 4 5 6 7 8; do
    "$@" && exit 0
    echo "polaris_retry.sh : essai $i echoue, nouveau tirage" >&2
done
exit 1
