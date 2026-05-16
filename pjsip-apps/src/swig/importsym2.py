#!/usr/bin/env python3
#
# importsym2.py: Import C symbol decls (structs, enums, typedefs) and write
#                them to another file. Python 3 rewrite of importsym.py
#                with no external dependencies (no pycparser needed).
#
# Copyright (C)2013-2026 Teluu Inc. (http://www.teluu.com)
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
import subprocess
import shutil
import sys
import os
import re
import tempfile

PJ_ROOT_PATH = "../../../"

INCLUDE_DIRS = [
    PJ_ROOT_PATH + "pjlib/include",
    PJ_ROOT_PATH + "pjlib-util/include",
    PJ_ROOT_PATH + "pjnath/include",
    PJ_ROOT_PATH + "pjmedia/include",
    PJ_ROOT_PATH + "pjsip/include",
]


def find_cpp():
    """Find the C preprocessor."""
    cpp = shutil.which("cpp")
    if cpp:
        return cpp
    # On some systems, try gcc -E
    gcc = shutil.which("gcc")
    if gcc:
        return gcc
    cc = shutil.which("cc")
    if cc:
        return cc
    return None


def preprocess(header_files):
    """Run the C preprocessor on the given header files and return the
    preprocessed output as a string.
    """
    cpp = find_cpp()
    if not cpp:
        print("Error: need cpp, gcc, or cc in PATH")
        sys.exit(1)

    # Build a temporary C file that includes all the headers
    content = "#define PJ_AUTOCONF 1\n"
    content += "#define __attribute__(x)\n"
    content += "#define PJ_DEPRECATED(x)\n"
    content += "#define PJ_ATTR_DEPRECATED\n"
    for hf in header_files:
        content += '#include <%s>\n' % hf

    # Write to temp file
    fd, tmpfile = tempfile.mkstemp(suffix='.h')
    try:
        os.write(fd, content.encode())
        os.close(fd)

        # Build cpp command
        cmd = [cpp]
        if 'gcc' in os.path.basename(cpp) or 'cc' == os.path.basename(cpp):
            cmd.append('-E')
        for d in INCLUDE_DIRS:
            cmd.extend(['-I', d])
        cmd.append(tmpfile)

        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print("Preprocessor error:")
            print(result.stderr)
            sys.exit(1)
        return result.stdout
    finally:
        os.unlink(tmpfile)


def find_matching_brace(source, start):
    """Find the position of the closing '}' that matches the '{' at start,
    handling nested braces. Returns the index of the closing '}'.
    """
    depth = 0
    i = start
    while i < len(source):
        if source[i] == '{':
            depth += 1
        elif source[i] == '}':
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


def extract_declarations(preprocessed, names):
    """Extract enum, struct, and typedef declarations for the given names
    from preprocessed C source.

    Returns a dict mapping name -> declaration string.
    """
    # Remove line directives (# 123 "file.h" ...)
    lines = []
    for line in preprocessed.split('\n'):
        if line.startswith('#'):
            continue
        lines.append(line)
    source = '\n'.join(lines)

    # Collapse multiple blank lines
    source = re.sub(r'\n{3,}', '\n\n', source)

    results = {}
    name_set = set(names)

    # Find all typedef/enum/struct declarations using brace matching
    # Pattern: typedef (enum|struct) NAME { ... } NAME ;
    for m in re.finditer(
            r'typedef\s+(enum|struct)\s+(\w+)\s*\{', source):
        kind = m.group(1)
        tag_name = m.group(2)
        brace_start = m.end() - 1  # position of '{'
        brace_end = find_matching_brace(source, brace_start)
        if brace_end < 0:
            continue
        # Find the alias name and semicolon after '}'
        rest = source[brace_end+1:brace_end+100]
        alias_m = re.match(r'\s*(\w+)\s*;', rest)
        if not alias_m:
            continue
        alias = alias_m.group(1)
        full_decl = source[m.start():brace_end+1+alias_m.end()]
        if tag_name in name_set or alias in name_set:
            key = tag_name if tag_name in name_set else alias
            if key not in results:
                results[key] = clean_decl(full_decl)

    # Pattern: enum NAME { ... };  (no typedef)
    for m in re.finditer(r'(?<!\w)enum\s+(\w+)\s*\{', source):
        enum_name = m.group(1)
        if enum_name not in name_set or enum_name in results:
            continue
        # Check this isn't preceded by 'typedef'
        prefix = source[max(0, m.start()-20):m.start()]
        if 'typedef' in prefix:
            continue
        brace_start = m.end() - 1
        brace_end = find_matching_brace(source, brace_start)
        if brace_end < 0:
            continue
        rest = source[brace_end+1:brace_end+20]
        semi_m = re.match(r'\s*;', rest)
        if not semi_m:
            continue
        full_decl = source[m.start():brace_end+1+semi_m.end()]
        results[enum_name] = clean_decl(full_decl)

    # Pattern: typedef <type> NAME;  (simple typedef)
    for m in re.finditer(r'typedef\s+(\w[\w\s]*?)\s+(\w+)\s*;', source):
        alias = m.group(2)
        if alias in name_set and alias not in results:
            results[alias] = clean_decl(m.group(0))

    return results


def clean_decl(decl):
    """Clean up a declaration: normalize whitespace, fix indentation,
    remove blank lines inside braces.
    """
    lines = decl.split('\n')
    cleaned = []
    for line in lines:
        line = line.rstrip()
        # Skip blank lines inside the declaration
        if line == '' and cleaned and cleaned[-1] != '':
            # Keep blank lines only between declarations, not inside braces
            in_braces = any('{' in l for l in cleaned) and \
                        not any('}' in l for l in cleaned)
            if in_braces:
                continue
        cleaned.append(line)

    # Remove any trailing blank lines before closing
    while len(cleaned) > 1 and cleaned[-2] == '':
        cleaned.pop(-2)

    result = '\n'.join(cleaned).strip()
    return result


def reformat_enum(decl):
    """Reformat an enum declaration for clean output."""
    # Already clean from cpp output, just ensure consistent style
    return decl


def process(listfile, outfile):
    """Main processing: read listfile, preprocess headers, extract symbols,
    write output.
    """
    # Read listfile
    with open(listfile) as f:
        lines = f.readlines()

    # Parse list file
    header_files = []
    names = []  # ordered list of names to extract
    for line in lines:
        spec = line.split()
        if len(spec) < 2:
            continue
        header_files.append(spec[0])
        names.extend(spec[1:])

    print('Preprocessing %d files for %d symbols..' %
          (len(header_files), len(names)))

    # Preprocess
    preprocessed = preprocess(header_files)

    # Extract declarations
    print('Extracting declarations..')
    decls = extract_declarations(preprocessed, names)

    # Write output in the order specified in listfile
    print('Writing declarations..')
    with open(outfile, 'w') as f:
        f.write("// This file is autogenerated by importsym script, "
                "do not modify!\n\n")
        for name in names:
            if name not in decls:
                print("  warning: declaration for '%s' is not found **" % name)
            else:
                print("  writing '%s'.." % name)
                decl = decls[name]
                # Avoid double semicolon
                if decl.endswith(';'):
                    f.write(decl + '\n\n')
                else:
                    f.write(decl + ';\n\n')

    print("Done. Output written to '%s'" % outfile)


if __name__ == "__main__":
    listfile = "symbols.lst"
    outfile = "symbols.i"

    if len(sys.argv) >= 3:
        listfile = sys.argv[1]
        outfile = sys.argv[2]

    print("Importing symbols: '%s' --> '%s'" % (listfile, outfile))
    process(listfile, outfile)
