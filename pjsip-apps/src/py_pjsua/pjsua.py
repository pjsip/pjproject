# import module py_pjsua
import py_pjsua

print '''Testing py_pjsua.create : '''
status = py_pjsua.create()
print "py status " + `status`

# perror
print '''Testing error code 70006 : '''
py_pjsua.perror("py_pjsua","hello",70006)

# test py_pjsua.destroy
print '''Testing py_pjsua.destroy : '''
status = py_pjsua.destroy()
print "py status " + `status`

print '''End Of py_pjsua'''
