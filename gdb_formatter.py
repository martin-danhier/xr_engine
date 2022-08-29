# Defines pretty printers for core classes

import gdb.printing as printing

# TODO define printers here


# ====== Register printers ======

def build_pretty_printer():
    pp = printing.RegexpCollectionPrettyPrinter("xre")
    return pp

print("Registering pretty printers for xr_engine")
printing.register_pretty_printer(None, build_pretty_printer())
