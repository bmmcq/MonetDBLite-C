import os, sys
from MonetDBtesting import process

def main():
    user = 'skyserver'
    passwd = 'skyserver'
    dir = os.getenv('TSTSRCDIR')
    sys.stdout.write('Create User\n')
    clt = process.client('sql', args = [os.path.join(dir, '..','create_user.sql')], stdout = process.PIPE)
    out,err = clt.communicate()
    sys.stdout.write(out)
    sys.stdout.write('tables\n')
    clt1 = process.client('sql', user = user, passwd = passwd, args = [os.path.join(dir, '..','..','..','sql','math.sql')], stdout = process.PIPE)
    out,err = clt1.communicate()
    sys.stdout.write(out)
    clt1 = process.client('sql', user = user, passwd = passwd, args = [os.path.join(dir, '..','..','..','sql','cache.sql')], stdout = process.PIPE)
    out,err = clt1.communicate()
    sys.stdout.write(out)
    clt1 = process.client('sql', user = user, passwd = passwd, args = [os.path.join(dir, '..','..','..','sql','skyserver.sql')], stdout = process.PIPE)
    out,err = clt1.communicate()
    sys.stdout.write(out)
    clt1 = process.client('sql', user = user, passwd = passwd, args = [os.path.join(dir, '..','Skyserver_tables_v6.sql')], stdout = process.PIPE)
    out,err = clt1.communicate()
    sys.stdout.write(out)
    clt1 = process.client('sql', user = user, passwd = passwd, stdin = process.PIPE, stdout = process.PIPE)
    sql = open(os.path.join(dir, '..', 'Skyserver_import_v6.sql')).read().replace('DATA_DIR',os.path.join(dir,'..','microsky_v6').replace('\\','\\\\'))
    out,err = clt1.communicate(sql)
    sys.stdout.write(out)
    clt1 = process.client('sql', user = user, passwd = passwd, args = [os.path.join(dir, '..','Skyserver_constraints_v6.sql')], stdout = process.PIPE)
    out,err = clt1.communicate()
    sys.stdout.write(out)
    sys.stdout.write('views\n')
    clt1 = process.client('sql', user = user, passwd = passwd, args = [os.path.join(dir, '..','Skyserver_views_v6.sql')], stdout = process.PIPE)
    out,err = clt1.communicate()
    sys.stdout.write(out)
    sys.stdout.write('functions\n')
    clt1 = process.client('sql', user = user, passwd = passwd, args = [os.path.join(dir, '..','Skyserver_functions_v6.sql')], stdout = process.PIPE)
    out,err = clt1.communicate()
    sys.stdout.write(out)
    sys.stdout.write('Cleanup\n')
    clt1 = process.client('sql', user = user, passwd = passwd, args = [os.path.join(dir, '..','Skyserver_dropFunctions_v6.sql')], stdout = process.PIPE)
    out,err = clt1.communicate()
    sys.stdout.write(out)
    clt1 = process.client('sql', user = user, passwd = passwd, args = [os.path.join(dir, '..','Skyserver_dropMs_functions.sql')], stdout = process.PIPE)
    out,err = clt1.communicate()
    sys.stdout.write(out)
    clt1 = process.client('sql', user = user, passwd = passwd, args = [os.path.join(dir, '..','Skyserver_dropMath.sql')], stdout = process.PIPE)
    out,err = clt1.communicate()
    sys.stdout.write(out)
    clt1 = process.client('sql', user = user, passwd = passwd, args = [os.path.join(dir, '..','Skyserver_dropViews_v6.sql')], stdout = process.PIPE)
    out,err = clt1.communicate()
    sys.stdout.write(out)
    clt1 = process.client('sql', user = user, passwd = passwd, args = [os.path.join(dir, '..','Skyserver_dropConstraints_v6.sql')], stdout = process.PIPE)
    out,err = clt1.communicate()
    sys.stdout.write(out)
    clt1 = process.client('sql', user = user, passwd = passwd, args = [os.path.join(dir, '..','Skyserver_dropTables_v6.sql')], stdout = process.PIPE)
    out,err = clt1.communicate()
    sys.stdout.write(out)
    sys.stdout.write('Remove User\n')
    clt = process.client('sql', args = [os.path.join(dir, '..','drop_user.sql')], stdout = process.PIPE)
    out,err = clt.communicate()
    sys.stdout.write(out)

main()
