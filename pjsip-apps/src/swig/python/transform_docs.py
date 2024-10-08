import re

def transform():
    file_path = 'pjsua2.py'

    with open(file_path, 'r') as file:
        content = file.read()

    # Regular expression to match the property with doc=...
    pattern = re.compile(r'(\w+\s*=\s*property\([^,]+,[^,]+),\s*doc=r?\"\"\"(.*?)\"\"\"\)', re.DOTALL)

    # Replace the matched pattern
    new_content = pattern.sub(r'\1)\n    """\2"""', content)

    with open(file_path, 'w') as file:
        file.write(new_content)
