import sys

def load_module_from_file(module_name, module_path):
   if sys.version_info[0] == 3 and sys.version_info[1] >= 5:
      import importlib.util
      spec = importlib.util.spec_from_file_location(module_name, module_path)
      module = importlib.util.module_from_spec(spec)
      spec.loader.exec_module(module)
   elif sys.version_info[0] == 3 and sys.version_info[1] < 5:
      import importlib.machinery
      loader = importlib.machinery.SourceFileLoader(module_name, module_path)
      module = loader.load_module()
   return module
