# Tar des fichiers en collision de casse, depuis git (HEAD) : src/Tools/* tels quels, src/tools/* -> src/tools_lc/*.
# Usage : casse_tar.py <depot> > sortie.tar
import io, subprocess, sys, tarfile, time
repo = sys.argv[1]
files = subprocess.run(['git', '-C', repo, 'ls-files', 'src/Tools', 'src/tools'], capture_output=True, text=True).stdout.split()
out = tarfile.open(fileobj=sys.stdout.buffer, mode='w|')
for f in files:
    data = subprocess.run(['git', '-C', repo, 'show', 'HEAD:' + f], capture_output=True).stdout
    name = f if f.startswith('src/Tools/') else 'src/tools_lc/' + f[len('src/tools/'):]
    ti = tarfile.TarInfo(name); ti.size = len(data); ti.mtime = time.time()
    out.addfile(ti, io.BytesIO(data))
out.close()
