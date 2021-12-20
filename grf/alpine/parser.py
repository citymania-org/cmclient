import struct

import ply.lex
import ply.yacc

OP_ADD = 0x00
OP_SUB = 0x01
OP_MIN = 0x02
OP_MAX = 0x03
OP_MINU = 0x04
OP_MAXU = 0x05
OP_MUL = 0x0A
OP_BINAND = 0x0B
OP_BINOR = 0x0C
OP_BINXOR = 0x0D
OP_TSTO = 0x0E
OP_INIT = 0x0F
OP_PSTO = 0x10
OP_ROT = 0x11
OP_CMP = 0x12
OP_CMPU = 0x13
OP_SHL = 0x14
OP_SHRU = 0x15
OP_SHR = 0x16

OPERATORS = {
    OP_ADD: ('{a} + {b}', 5, False),
    OP_SUB: ('{a} - {b}', 5, True),
    OP_MIN: ('min({a}, {b})', 7, False),
    OP_MAX: ('max({a}, {b})', 7, False),
    OP_MINU: ('minu({a}, {b})', 7, False),
    OP_MAXU: ('maxu({a}, {b})', 7, False),
    OP_MUL: ('{a} * {b}', 6, False),
    OP_BINAND: ('{a} & {b}', 4, True),
    OP_BINOR: ('{a} | {b}', 4, True),
    OP_BINXOR: ('{a} ^ {b}', 4, True),
    OP_TSTO: ('TEMP[{b}] = {a}', 2, True),
    OP_INIT: (None, 1, False),
    OP_PSTO: ('PERM[{b}] = {a}', 2, True),
    OP_ROT: ('rot({a}, {b})', 7, False),
    OP_CMP: ('cmp({a}, {b})', 7, False),
    OP_CMPU: ('cmpu({a}, {b})', 7, False),
    OP_SHL: ('{a} << {b}', 4, True),
    OP_SHRU: ('{a} u>> {b}', 4, True),
    OP_SHR: ('{a} >> {b}', 4, True),
}

DEFAULT_INDENT_STR = '    '


def hex_str(s):
    if isinstance(s, (bytes, memoryview)):
        return ':'.join('{:02x}'.format(b) for b in s)
    return ':'.join('{:02x}'.format(ord(c)) for c in s)


class Node:
    def __init__(self):
        pass

    def _make_node(self, value):
        if isinstance(value, int):
            return Value(value)
        assert isinstance(value, Node)
        return value

    def __add__(self, other):
        return Expr(OP_ADD, self, self._make_node(other))

    def __sub__(self, other):
        return Expr(OP_SUB, self, self._make_node(other))

    def store_temp(self, register):
        return Expr(OP_TSTO, self, self._make_node(register))

    def store_perm(self, register):
        return Expr(OP_PSTO, self, self._make_node(register))

    def format(self, parent_priority=0):
        raise NotImplementedError

    def __str__(self):
        return '\n'.join(self.format())

    def __repr__(self):
        return '; '.join(self.format())

    def compile(self, register, shift=0, and_mask=0xffffffff):
        raise NotImplementedError


class Expr(Node):
    def __init__(self, op, a, b):
        self.op = op
        self.a = a
        self.b = b

    def format(self, parent_priority=0):
        if self.op in OPERATORS:
            fmt, prio, bracket = OPERATORS[self.op]
        else:
            fmt, prio, bracket = f'{{a}} <{self.op:02x}> {{b}}', 1, True

        ares = self.a.format(prio - 1)
        bres = self.b.format(prio - int(not bracket))
        assert len(bres) == 1, bres

        if self.op == OP_INIT:
            ares.append(bres[0])
            return ares

        res = fmt.format(a=ares[-1], b=bres[-1])
        if prio <= parent_priority:
            res = f'({res})'
        ares[-1] = res
        return ares

    def compile(self, register, shift=0, and_mask=0xffffffff):
        is_value, b_code = self.b.compile(register, shift, and_mask)
        if is_value:
            # Second arg is a simple value, do operation directly
            res = self.a.compile(register, shift, and_mask)[1]
            res += bytes((self.op,))
            res += b_code
            return False, res

        # Calculate secord arg first and store in in a temp var
        res = b_code
        res += struct.pack('<BBBIB', OP_TSTO, 0x1a, 0x20, register, OP_INIT)
        res += self.a.compile(register + 1, shift, and_mask)[1]
        res += struct.pack('<BBBBI', self.op, 0x7d, register, 0x20, 0xffffffff)
        return False, res


class Value(Node):
    def __init__(self, value):
        super().__init__()
        self.value = value

    def format(self, parent_priority=0):
        return [str(self.value)]

    def compile(self, register, shift=0, and_mask=0xffffffff):
        assert shift < 0x20, shift
        assert and_mask <= 0xffffffff, and_mask
        valueadj = (self.value >> shift) & and_mask
        return True, struct.pack('<BBI', 0x1a, 0x20, valueadj)


NML_VARACT2_GLOBALVARS = {
    'current_month'        : {'var': 0x02, 'start':  0, 'size':  8},
    'current_day_of_month' : {'var': 0x02, 'start':  8, 'size':  5},
    'is_leapyear'          : {'var': 0x02, 'start': 15, 'size':  1},
    'current_day_of_year'  : {'var': 0x02, 'start': 16, 'size':  9},
    'traffic_side'         : {'var': 0x06, 'start':  4, 'size':  1},
    'animation_counter'    : {'var': 0x0A, 'start':  0, 'size': 16},
    'current_callback'     : {'var': 0x0C, 'start':  0, 'size': 16},
    'extra_callback_info1' : {'var': 0x10, 'start':  0, 'size': 32},
    'game_mode'            : {'var': 0x12, 'start':  0, 'size':  8},
    'extra_callback_info2' : {'var': 0x18, 'start':  0, 'size': 32},
    'display_options'      : {'var': 0x1B, 'start':  0, 'size':  6},
    'last_computed_result' : {'var': 0x1C, 'start':  0, 'size': 32},
    'snowline_height'      : {'var': 0x20, 'start':  0, 'size':  8},
    'difficulty_level'     : {'var': 0x22, 'start':  0, 'size':  8},
    'current_date'         : {'var': 0x23, 'start':  0, 'size': 32},
    'current_year'         : {'var': 0x24, 'start':  0, 'size': 32},

    # TODO object vars
    'relative_x'             : {'var': 0x40, 'start':  0, 'size':  8},
    'relative_y'             : {'var': 0x40, 'start':  8, 'size':  8},
    'relative_pos'           : {'var': 0x40, 'start':  0, 'size': 16},

    'terrain_type'           : {'var': 0x41, 'start':  0, 'size':  3},
    'tile_slope'             : {'var': 0x41, 'start':  8, 'size':  5},

    'build_date'             : {'var': 0x42, 'start':  0, 'size': 32},

    'animation_frame'        : {'var': 0x43, 'start':  0, 'size':  8},
    'company_colour'         : {'var': 0x43, 'start':  0, 'size':  8},

    'owner'                  : {'var': 0x44, 'start':  0, 'size':  8},

    'town_manhattan_dist'    : {'var': 0x45, 'start':  0, 'size': 16},
    'town_zone'              : {'var': 0x45, 'start': 16, 'size':  8},

    'town_euclidean_dist'    : {'var': 0x46, 'start':  0, 'size': 16},
    'view'                   : {'var': 0x48, 'start':  0, 'size':  8},
    'random_bits'            : {'var': 0x5F, 'start':  8, 'size':  8},

    # TODO object nearby vars
    'tile_height'            : {'var': 0x62, 'start':  16, 'size':  8, 'param': 0},
}


class Var(Node):
    def __init__(self, name):
        super().__init__()
        self.name = name

    def format(self, parent_priority=0):
        return [self.name]

    def compile(self, register, shift=0, and_mask=0xffffffff):
        var_data = NML_VARACT2_GLOBALVARS.get(self.name)

        if var_data is None:
            raise ValueError(f'Unknown variable `{self.name}`')
        var_mask = (1 << var_data['size']) - 1
        and_mask &= var_mask >> shift
        shift += var_data['start']
        assert shift < 0x20, shift
        assert and_mask <= 0xffffffff, and_mask
        if 'param' in var_data:
            return True, struct.pack('<BBBI', var_data['var'], var_data['param'], 0x20 | shift, and_mask)
        else:
            return True, struct.pack('<BBI', var_data['var'], 0x20 | shift, and_mask)


class Temp(Node):
    def __init__(self, register):
        super().__init__()
        assert isinstance(register, int), type(register)
        self.register = register

    def format(self, parent_priority=0):
        return [f'TEMP[{self.register}]']

    def compile(self, register, shift=0, and_mask=0xffffffff):
        assert shift < 0x20, shift
        assert and_mask <= 0xffffffff, and_mask
        return True, struct.pack('<BBBI', 0x7d, self.register, 0x20 | shift, and_mask)


class Perm(Node):
    def __init__(self, register):
        super().__init__()
        self.register = register

    def format(self, parent_priority=0):
        return [f'PERM[{self.register}]']

    def compile(self, register, shift=0, and_mask=0xffffffff):
        assert shift < 0x20, shift
        assert and_mask <= 0xffffffff, and_mask
        return True, struct.pack('<BBI', 0x7c, 0x20 | shift, and_mask)


class Call(Node):
    def __init__(self, subroutine):
        assert 0 <= subroutine < 256, subroutine
        self.subroutine = subroutine

    def format(self, parent_priority=0):
        return [f'call({self.subroutine})']

    def compile(self, register, shift=0, and_mask=0xffffffff):
        assert shift < 0x20, shift
        assert and_mask <= 0xffffffff, and_mask
        return True, struct.pack('<BBBI', 0x7e, self.subroutine, 0x20 | shift, and_mask)


tokens = (
    'NAME', 'NUMBER', 'NEWLINE',
    'ADD', 'SUB', 'MUL',
    'BINAND', 'BINOR', 'BINXOR', 'SHR', 'SHL',
    'ASSIGN', 'COMMA',
    'LPAREN', 'RPAREN', 'LBRACKET', 'RBRACKET',
)

# Tokens

t_ADD = r'\+'
t_SUB = r'-'
t_MUL = r'\*'
# t_DIV = r'/'
# t_MOD = r'%'
t_BINAND = r'\&'
t_BINOR = r'\|'
t_BINXOR = r'\^'
t_SHR = r'>>'
t_SHL = r'<<'
t_ASSIGN = r'='
t_COMMA = r','
t_LPAREN = r'\('
t_RPAREN = r'\)'
t_LBRACKET = r'\['
t_RBRACKET = r'\]'
t_NAME = r'[a-zA-Z_][a-zA-Z0-9_]*'


def t_NUMBER(t):
    r'\d+'
    try:
        t.value = int(t.value)
    except ValueError:
        print("Integer value too large %d", t.value)
        t.value = 0
    return t


# Ignored characters
t_ignore = ' \t'


def t_NEWLINE(t):
    r'\n+'
    t.lexer.lineno += t.value.count("\n")
    return t


def t_error(t):
    print("Illegal character '%s'" % t.value[0])
    t.lexer.skip(1)


# Parsing rules

precedence = (
    ('left', 'SHR', 'SHL'),
    ('left', 'BINOR'),
    ('left', 'BINXOR'),
    ('left', 'BINAND'),
    ('left', 'ADD', 'SUB'),
    ('left', 'MUL'),
    ('right', 'UMINUS'),
)


def p_lines(t):
    '''lines : line
             | expression
             | lines line
    '''
    # print('LINES', t[1])
    if len(t) == 2:
        lines, line = [], t[1]
    else:
        lines, line = t[1], t[2]

    if line is not None:
        lines.append(line)
    t[0] = lines


def p_line(t):
    '''line : expression NEWLINE
            | NEWLINE
    '''
    # print('LINE', t[1], len(t))
    if len(t) == 2:
        t[0] = None
    else:
        t[0] = t[1]


# def p_statement_expr(t):
#     'statement : expression'
#     t[0] = t[1]


# def p_storagge(t):
#     'storage : NAME LBRACKET NUMBER RBRACKET'
#     storage = {
#         'TEMP': grf.Temp,
#         'PERM': grf.Perm,
#     }.get(t[1])
#     assert storage is not None, t[1]
#     register = int(t[3])

#     t[0] = storage(grf.Value(register))


def p_expression_binop(t):
    '''expression : expression ADD expression
                  | expression SUB expression
                  | expression MUL expression
                  | expression BINAND expression
                  | expression BINOR expression
                  | expression BINXOR expression
                  | expression SHL expression
                  | expression SHR expression
    '''
    op = {
        '+': OP_ADD,
        '-': OP_SUB,
        '*': OP_MUL,
        '&': OP_BINAND,
        '|': OP_BINOR,
        '^': OP_BINXOR,
        '>>': OP_SHL,
        '<<': OP_SHR,
    }.get(t[2])

    assert op is not None, t[2]
    # print(op, t[1], t[3])
    t[0] = Expr(op, t[1], t[3])


# def p_storagge(t):
#     'storage : NAME LBRACKET NUMBER RBRACKET'
#     storage = {
#         'TEMP': grf.Temp,
#         'PERM': grf.Perm,
#     }.get(t[1])
#     assert storage is not None, t[1]
#     register = int(t[3])

#     t[0] = storage(grf.Value(register))


def p_expression_assign(t):
    'expression : NAME LBRACKET NUMBER RBRACKET ASSIGN expression'
    assert t[1] in ('TEMP', 'PERM'), t[1]
    op = OP_TSTO if t[1] == 'TEMP' else OP_PSTO
    register = int(t[3])
    t[0] = Expr(op, t[6], Value(register))


# def p_expression_uminus(t):
#     'expression : SUB expression %prec UMINUS'
#     t[0] = -t[2]


def p_expression_call1(t):
    'expression : NAME LPAREN NUMBER RPAREN'
    assert t[1] == 'call', t[1]
    t[0] = Call(int(t[3]))


def p_expression_call2(t):
    'expression : NAME LPAREN expression COMMA expression RPAREN'
    op = {
        'min': OP_MIN,
        'max': OP_MAX,
        'minu': OP_MINU,
        'maxu': OP_MAXU,
        'rot': OP_ROT,
        'cmp': OP_CMP,
        'cmpu': OP_CMPU,
    }.get(t[1])
    assert op is not None, t[1]
    t[0] = Expr(op, t[3], t[5])


def p_expression_group(t):
    'expression : LPAREN expression RPAREN'
    t[0] = t[2]


def p_expression_storage(t):
    'expression : NAME LBRACKET NUMBER RBRACKET'
    assert t[1] in ('TEMP', 'PERM'), t[1]
    cls = Temp if t[1] == 'TEMP' else Perm
    register = int(t[3])
    t[0] = cls(register)


def p_expression_number(t):
    '''expression : NUMBER
                  | SUB NUMBER %prec UMINUS
    '''
    if len(t) == 2:
        t[0] = Value(int(t[1]))
    else:
        t[0] = Value(-int(t[2]))


def p_expression_name(t):
    'expression : NAME'
    t[0] = Var(t[1])


def p_error(t):
    # stack_state_str = ' '.join([symbol.type for symbol in parser.symstack][1:])

    # print('Syntax error in input! Parser State:{} {} . {}'
    #       .format(parser.state,
    #               stack_state_str,
    #               t))

    if t is None:
        print("Unexpected syntax error")
        return

    print(f'Syntax error at `{t.value}` line {t.lineno}')


def parse_code(code):
    lexer = ply.lex.lex()
    parser = ply.yacc.yacc()
    return parser.parse(code)


if __name__ == "__main__":
    res = parse_code('''
        TEMP[128] = (cmp(tile_slope, 30) & 1) * 18
        TEMP[129] = (cmp(tile_slope, 29) & 1) * 15
        TEMP[130] = (cmp(tile_slope, 27) & 1) * 17
        TEMP[131] = (cmp(tile_slope, 23) & 1) * 16
        TEMP[132] = min(cmp(tile_slope, 0), 1)
        TEMP[134] = (min((cmp(tile_slope, 14) ^ 2), 1) & TEMP[132]) * tile_slope + TEMP[131] + TEMP[130] + TEMP[129] + TEMP[128]
    ''')

    for line in res:
        print(line.format())
