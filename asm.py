opcodes = {
    'NOP':[0x00,0],
    'BNK':[0x01,0],
    'OUT':[0x02,0],
    'CLC':[0x03,0],
    'SEC':[0x04,0],
    'LSL':[0x05,0],
    'ROL':[0x06,0],
    'LSR':[0x07,0],
    'ROR':[0x08,0],
    'INP':[0x0a,0],
    'NEG':[0x0b,0],
    'INC':[0x0c,0],
    'DEC':[0x0d,0],
    'LDI':[0x0e,1],
    'ADI':[0x0f,1],
    'SBI':[0x10,1],
    'CPI':[0x11,1],
    'ACI':[0x12,1],
    'SCI':[0x13,1],
    'JPA':[0x14,2],
    'LDA':[0x15,2],
    'STA':[0x16,2],
    'ADA':[0x17,2],
    'SBA':[0x18,2],
    'CPA':[0x19,2],
    'ACA':[0x1a,2],
    'SCA':[0x1b,2],
    'JPR':[0x1c,2],
    'LDR':[0x1d,2],
    'STR':[0x1e,2],
    'ADR':[0x1f,2],
    'SBR':[0x20,2],
    'CPR':[0x21,2],
    'ACR':[0x22,2],
    'SCR':[0x23,2],
    'CLB':[0x24,2],
    'NEB':[0x25,2],
    'INB':[0x26,2],
    'DEB':[0x27,2],
    'ADB':[0x28,2],
    'SBB':[0x29,2],
    'ACB':[0x2a,2],
    'SCB':[0x2b,2],
    'CLW':[0x2c,2],
    'NEW':[0x2d,2],
    'INW':[0x2e,2],
    'DEW':[0x2f,2],
    'ADW':[0x30,2],
    'SBW':[0x31,2],
    'ACW':[0x32,2],
    'SCW':[0x33,2],
    'LDS':[0x34,1],
    'STS':[0x35,1],
    'PHS':[0x36,0],
    'PLS':[0x37,0],
    'JPS':[0x38,2],
    'RTS':[0x39,0],
    'BNE':[0x3a,2],
    'BEQ':[0x3b,2],
    'BCC':[0x3c,2],
    'BCS':[0x3d,2],
    'BPL':[0x3e,2],
    'BMI':[0x3f,2],
}

lines = []
tokens = []
labels = {}
output = b''

import sys
if len(sys.argv) != 3:
    print("Usage:\n asm.py <srcfile> <output>"); sys.exit(1);

f = open(sys.argv[1], 'r')
while True:
    line = f.readline()
    if not line: break
    lines.append(line.strip())
f.close()

for i in range(len(lines)):
    # replace string literals with char values
    while(lines[i].find('\'') >= 0):
        j = lines[i].find('\'')
        k = lines[i].find('\'', j+1)
        if j >= 0 and k >= 0:
            repl = ''
            for c in lines[i][j+1:k]: repl += ' ' + hex(ord(c)) + ' '
            lines[i] = lines[i][0:j] + repl + lines[i][k+1:]
        else: break

    # delete comments
    k = lines[i].find(';')
    if(k >= 0): lines[i] = lines[i][0:k]

    # replace commas with spaces
    lines[i] = lines[i].replace(',', ' ')

    tlist = lines[i].split()
    for t in tlist:
        tokens.append(t)

#print(tokens)

def scan_int(tok):
    a = tok
    base = 10
    if (a[:2] == '0x' or a[:2] == '0X'):
        base = 16
        a = a[2:]
    return int(a, base)

# pass 1
addr = 0
i = 0
tlen = len(tokens)
while(i < tlen):
    t = tokens[i]
    i += 1

    if t == '#org':
        a = tokens[i]
        i += 1
        addr = scan_int(a)
    elif t[-1] == ':':
        labels[t[:-1]] = addr
    elif t in opcodes:
        opc = opcodes[t]
        addr += opc[1] + 1
        if opc[1] > 0: i += 1
    else:
        addr += 1

#print(labels)

def emit(val):
    global output
    output += (val & 0xff).to_bytes(1, 'little')

def emit_word(val):
    global output
    output += (val & 0xffff).to_bytes(2, 'little')

def tok_value(t):
    tadj = t
    if t[0] == '+' or t[0] == '-':
        tadj = t[1:]
    ip = tadj.find('+')
    im = tadj.find('-')
    i = -1
    if ip > 0 and im < 0:
        i = ip
    elif ip < 0 and im > 0:
        i = im
    elif ip > im:
        i = ip
    elif ip < im:
        i = im
    r = ''
    if i >= 0:
        r = t[i:]
        t = t[:i]

    val = 0
    if t[0] >= '0' and t[0] <= '9':
        val = scan_int(t)
    elif t[0] == '+':
        val = tok_value(t[1:])
    elif t[0] == '-':
        val = -tok_value(t[1:])
    elif t[0] == '<' or t[0] == '>':
        val = labels[t[1:]]
        if t[0] == '>':
            val = ((val >> 8) & 0xff)
        elif t[0] == '<':
            val = ((val >> 0) & 0xff)
    else:
        val = labels[t]

    if len(r) > 0:
        if r[0] == '+':
            val = val + tok_value(r[1:])
        elif r[0] == '+':
            val = val - tok_value(r[1:])

    return val

# pass 2
addr = 0
i = 0
tlen = len(tokens)
while(i < tlen):
    t = tokens[i]
    i += 1

    if t == '#org':
        a = tokens[i]
        i += 1
        addr = scan_int(a)
    elif t[-1] == ':':
        continue
    elif t in opcodes:
        opc = opcodes[t]
        emit(opc[0])
        if opc[1] == 1:
            op = tokens[i]
            i += 1
            emit(tok_value(op))
        elif opc[1] == 2:
            op = tokens[i]
            i += 1
            emit_word(tok_value(op))
        addr += opc[1] + 1
    else:
        emit(tok_value(t))
        addr += 1

#print(output)
#
#txtoutput = ''
#for b in output:
#    h = hex(b)[2:]
#    if len(h) < 2:
#        h = '0' + h
#    txtoutput += h
#
#print(txtoutput)

f = open(sys.argv[2], 'wb')
f.write(output)
f.close()

for lbl in labels:
    print(lbl + " => " + "{0:#0{1}x}".format(labels[lbl], 6))
