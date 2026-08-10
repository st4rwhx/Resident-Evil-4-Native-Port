"""Symbol-name sanitisation shared by gen_config.py (DOL) and gen_rel_config.py (REL modules).

Turns the demangled names of the debug build's .sym files into linker-safe identifiers.
"""
import re

REPL = {'::': '__', 'operator=': 'op_assign', 'operator==': 'op_eq', 'operator!=': 'op_ne', 'operator<': 'op_lt', 'operator>': 'op_gt', 'operator<=': 'op_le', 'operator>=': 'op_ge', 'operator+': 'op_add', 'operator-': 'op_sub', 'operator*': 'op_mul', 'operator/': 'op_div', 'operator[]': 'op_idx', 'operator()': 'op_call', 'operator new': 'op_new', 'operator delete': 'op_delete', 'operator+=': 'op_addeq', 'operator-=': 'op_subeq', 'operator*=': 'op_muleq', 'operator/=': 'op_diveq', 'operator!': 'op_not', 'operator&': 'op_and', 'operator|': 'op_or', 'operator^': 'op_xor', 'operator<<': 'op_shl', 'operator>>': 'op_shr', 'operator->': 'op_arrow', 'operator++': 'op_inc', 'operator--': 'op_dec', 'operator%': 'op_mod'}


def sanitize(n):
    for k, v in sorted(REPL.items(), key=lambda kv: -len(kv[0])): n = n.replace(k, v)
    n = n.replace('~', 'dt_')
    n = re.sub(r'[<>,\s\*&\(\)\[\]]+', '_', n).rstrip('_')
    n = re.sub(r'(?<=.)_{2,}', '_', n)  # collapse runs, but keep leading underscores
    if not re.match(r'^[A-Za-z_@$.]', n): n = '_' + n
    return n
