"""Ajoute classes.dex et lib/ a l'APK produit par aapt2 (pas de `zip` dans Git Bash).

Les .so sont STOCKES, pas compresses : zipalign les aligne ensuite sur 4 Ko (-p) pour que le
chargeur puisse les projeter en place. Usage : apk_add.py <base.apk> <sortie.apk> <dir>
"""
import os, shutil, sys, zipfile

base, out, root = sys.argv[1:4]
shutil.copyfile(base, out)
with zipfile.ZipFile(out, "a") as z:
    z.write(os.path.join(root, "dex", "classes.dex"), "classes.dex", zipfile.ZIP_DEFLATED)
    for dp, _, files in os.walk(os.path.join(root, "lib")):
        for f in files:
            full = os.path.join(dp, f)
            z.write(full, os.path.relpath(full, root).replace(os.sep, "/"), zipfile.ZIP_STORED)
