# functions to disable and re-enable the include of pj/compat/socket.h in pj/sock.h
def disable_include():
    import os.path as path
    import re
    
    current_dir = path.dirname(path.abspath(__file__))
    main_project = path.abspath(path.join(current_dir, '../../../../'))
    file_path = path.abspath(path.join(main_project, 'pjlib/include/pj/sock.h'))
    print("File to edit: %s" % file_path)
    
    global use_new_path
    
    yes_no = raw_input("is the path correct? (y/n): ")
    if yes_no == 'y':
        is_path_valid = True
    elif yes_no == 'n':
        is_path_valid = False

    if is_path_valid == True:
        
        use_new_path = False
        with open(file_path, 'r') as file:
            content = file.read()
            
        # Regular expression to match the line with #include <pj/compat/socket.h>
        pattern = re.compile(r'#include <pj/compat/socket.h>')
        
        # Replace the matched pattern
        new_content = pattern.sub(r'/* #include <pj/compat/socket.h> */', content)
        
        with open(file_path, 'w') as file:
            file.write(new_content)
            
        print("pj/compat/socket.h has been disabled in pj/sock.h!")
            
    elif is_path_valid == False:
        

        use_new_path = True
        print("Current location is %s" % main_project)
        print("Use ../ at the start of the path to go back a directory...")
        new_path = raw_input("Enter the correct path: ")
        global new_file_path
        new_file_path = path.abspath(path.join(main_project, new_path))
        
        with open(new_file_path, 'r') as file:
            content = file.read()
            
        # Regular expression to match the line with #include <pj/compat/socket.h>
        pattern = re.compile(r'#include <pj/compat/socket.h>')
        
        # Replace the matched pattern
        new_content = pattern.sub(r'/* #include <pj/compat/socket.h> */', content)
        
        with open(new_file_path, 'w') as file:
            file.write(new_content)
            
        print("pj/compat/socket.h has been disabled in pj/sock.h!")
            
def re_enable_include():
    
    import os.path as path
    import re
    
    current_dir = path.dirname(path.abspath(__file__))
    main_project = path.abspath(path.join(current_dir, '../../../'))
    file_path = path.abspath(path.join(main_project, 'pjlib/include/pj/sock.h'))
    
    if use_new_path == True:
        file_path = new_file_path
    
    with open(file_path, 'r') as file:
        content = file.read()
        
    # Regular expression to match the line with #include <pj/compat/socket.h>
    pattern = re.compile(r'/* #include <pj/compat/socket.h> */')
    
    # Replace the matched pattern
    new_content = pattern.sub(r'#include <pj/compat/socket.h>', content)
    
    with open(file_path, 'w') as file:
        file.write(new_content)
        
    print("pj/compat/socket.h has been re-enabled in pj/sock.h!")