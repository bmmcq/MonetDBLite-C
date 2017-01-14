try:
    from MonetDBtesting import process
except ImportError:
    import process
import os, sys, socket

process.verbose = True
dbfarm = os.getenv('GDK_DBFARM')
tstdb = os.getenv('TSTDB')

if not tstdb or not dbfarm:
    print 'No TSTDB or GDK_DBFARM in environment'
    sys.exit(1)

def freeport():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.bind(('', 0))
    port = sock.getsockname()[1]
    sock.close()
    return port

cloneport = freeport()

dbname = tstdb
dbnameclone = tstdb + '-clone'

# clean up before we start
if os.path.exists(os.path.join(dbfarm, dbnameclone)):
    import shutil
    shutil.rmtree(os.path.join(dbfarm, dbnameclone))

master = process.server(dbname = dbname, stdin = process.PIPE, stdout = process.PIPE, stderr = process.PIPE)
slave = process.server(dbname = dbnameclone, mapiport = cloneport, stdin = process.PIPE, stdout = process.PIPE, stderr = process.PIPE)

c = process.client('sql', dbname = dbnameclone, port = cloneport, stdin = process.PIPE, stdout = process.PIPE, stderr = process.PIPE)

cout, cerr = c.communicate('''\
call clone('%s');
select * from tmp;
''' % dbname)

sout, serr = slave.communicate()
mout, merr = master.communicate()

sys.stdout.write(mout)
sys.stdout.write(sout)
sys.stdout.write(cout)
sys.stderr.write(merr)
sys.stderr.write(serr)
sys.stderr.write(cerr)
