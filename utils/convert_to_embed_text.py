# The script to convert the text file to embed inside char* variable in .ino or .cpp file

import sys
import os

def upper(s: str) -> str:
    return s.upper()

if len(sys.argv) < 2:
    txt_file = input("Please specify the file to convert: ")
else:
    txt_file = sys.argv[1]

if not os.path.isfile(txt_file):
    print("File not found")
    sys.exit(1)

with open(txt_file, "r") as f:
    txt = f.read()

out_name = txt_file.replace(".", "_") + ".h"
out_var = "_".join(txt_file.split(".")).upper()

with open(out_name, "w") as f:
    f.write(f"const char* {out_var} = R\"rawliteral(\n{txt}\n)rawliteral\";")