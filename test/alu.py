#!/usr/bin/env python3

import struct

def load_tests_8x8(fname):
    with open(fname, 'rb') as f:
        t = f.read()

    tests = []
    while t:
        for val1 in range(0, 256):
            for val2 in range(0, 256):
                b = t[0:3]
                t = t[3:]
                exp_val, exp_flags = struct.unpack('<BH', b)
                tests.append((exp_val, exp_flags))
    return tests

def load_tests_8(fname):
    with open(fname, 'rb') as f:
        t = f.read()

    tests = []
    while t:
        b = t[0:3]
        t = t[3:]
        exp_val, exp_flags = struct.unpack('<BH', b)
        tests.append((exp_val, exp_flags))
    return tests

def load_tests_16(fname):
    with open(fname, 'rb') as f:
        t = f.read()

    tests = []
    while t:
        b = t[0:4]
        t = t[4:]
        exp_val, exp_flags = struct.unpack('<HH', b)
        tests.append((exp_val, exp_flags))
    return tests

CF = (1 << 0)
ON = (1 << 1)
PF = (1 << 2)
AF = (1 << 4)
ZF = (1 << 6)
SF = (1 << 7)
OF = (1 << 11)

def decode_flags(fl):
    s = ''
    s += 'O' if ((fl & OF) != 0) else '.'
    s += '...'
    s += 'S' if ((fl & SF) != 0) else '.'
    s += 'Z' if ((fl & ZF) != 0) else '.'
    s += '.'
    s += 'A' if ((fl & AF) != 0) else '.'
    s += '..'
    s += 'P' if ((fl & PF) != 0) else '.'
    s += '1' if ((fl & ON) != 0) else '.'
    s += 'C' if ((fl & CF) != 0) else '.'
    return s

def format_bin(v):
    s = format(v, '08b')
    return s[0:4] + ' ' + s[4:8]

def must_set_zf(v):
    return v == 0

def must_set_sf(v):
    return (v & 0x80) != 0

def must_set_pf(v):
    num_1 = 0
    for n in range(0, 8):
        if (v & (1 << n)): num_1 += 1
    return (num_1 & 1) == 0

def set_flag(fl, on, flag):
    if on:
        fl = fl | flag
    else:
        fl = fl & ~flag
    return fl


def set_flags_pzs(v, fl):
    fl = set_flag(fl, must_set_zf(v), ZF)
    fl = set_flag(fl, must_set_sf(v), SF)
    fl = set_flag(fl, must_set_pf(v), PF)
    return fl

def must_set_ov_add(a, b, res):
    sign_a = (a & 0x80) != 0
    sign_b = (b & 0x80) != 0
    sign_r = (res & 0x80) != 0

    ov = False
    if sign_a == False and sign_b == False and sign_r == True:
        ov = True
    if sign_a == True and sign_b == True and sign_r == False:
        ov = True
    return ov

def must_set_ov_sub(a, b, res):
    sign_a = (a & 0x80) != 0
    sign_b = (b & 0x80) != 0
    sign_r = (res & 0x80) != 0

    ov = False
    if sign_a == False and sign_b == True and sign_r == True:
        ov = True
    if sign_a == True and sign_b == False and sign_r == False:
        ov = True
    return ov

def set_flags_sub(a, b, c, res, fl):
    fl = set_flags_pzs(res, fl)
    if must_set_ov_sub(a, b, res): fl = fl | OF
    if (b & 0xf) + c > (a & 0xf):
        fl = fl | AF
    return fl

def set_flags_add(a, b, c, res, fl):
    fl = set_flags_pzs(res, fl)
    if must_set_ov_add(a, b, res): fl = fl | OF
    if (b & 0xf) + (a & 0xf) + c >= 0x10:
        fl = fl | AF
    return fl

def my_add(a, b, initial_flags):
    fl = initial_flags
    res = (a + b) & 0xffff
    if res & 0xff00: fl = fl | CF
    res = res & 0xff
    return (res, set_flags_add(a, b, 0, res, fl))

def my_sub(a, b, initial_flags):
    fl = initial_flags
    res = (a - b) & 0xffff
    if res & 0xff00: fl = fl | CF
    res = res & 0xff
    return (res, set_flags_sub(a, b, 0, res, fl))

def my_adc(a, b, initial_flags):
    fl = initial_flags
    c = 1 if initial_flags & CF else 0
    fl = initial_flags & ~CF
    res = (a + b + c) & 0xffff
    if res & 0xff00: fl = fl | CF
    res = res & 0xff
    return (res, set_flags_add(a, b, c, res, fl))

def my_sbb(a, b, initial_flags):
    fl = initial_flags
    c = 1 if initial_flags & CF else 0
    fl = initial_flags & ~CF
    res = (a - b - c) & 0xffff
    fl = set_flag(fl, res & 0xff00, CF)
    res = res & 0xff
    return (res, set_flags_sub(a, b, c, res, fl))

def my_shl(a, cnt, initial_flags):
    cnt = cnt & 0x1f
    if cnt == 0:
        return (a, initial_flags)

    new_fl = initial_flags & ~CF

    res = a
    for _ in range(0, cnt):
        new_fl = set_flag(new_fl, res & 0x80, CF)
        res = (res << 1) & 0xff

    # OF is undefined if count > 1
    cf = 0x80 if (new_fl & CF) else 0
    new_fl = set_flag(new_fl, (res & 0x80) ^ cf, OF)
    new_fl = set_flags_pzs(res, new_fl)
    return (res, new_fl)

def my_shl1(a, initial_flags):
    return my_shl(a, 1, initial_flags)

def my_shr(a, cnt, initial_flags):
    cnt = cnt & 0x1f
    if cnt == 0:
        return (a, initial_flags)

    new_fl = initial_flags & ~CF

    res = a
    for _ in range(0, cnt):
        new_fl = set_flag(new_fl, res & 1, CF)
        res = (res >> 1) & 0xff

    if cnt == 1:
        new_fl = set_flag(new_fl, a & 0x80, OF)
    else:
        # OF is undefined, but does not seem to get set
        pass

    return (res, set_flags_pzs(res, new_fl))

def my_shr1(a, initial_flags):
    return my_shr(a, 1, initial_flags)

def my_sar(a, cnt, initial_flags):
    cnt = cnt & 0x1f
    if cnt == 0:
        # no flags changed
        return (a, initial_flags)

    new_fl = initial_flags & ~CF

    res = a
    for _ in range(0, cnt):
        new_fl = set_flag(new_fl, res & 1, CF)
        expand = 0x80 if res & 0x80 else 0
        res = expand | (res >> 1)

    # shifts of 1 always clear OF - otherwise OF is undefined but it always
    # seems to be cleared...
    return (res, set_flags_pzs(res, new_fl))

def my_sar1(a, initial_flags):
    return my_sar(a, 1, initial_flags)

def my_rol(a, cnt, initial_flags):
    cnt = cnt & 0x1f

    new_fl = initial_flags
    res = a
    for _ in range(0, cnt % 8):
        temp_cf = 1 if (res & 0x80) != 0 else 0
        res = ((res << 1) + temp_cf) & 0xff

    if cnt > 0:
        new_fl = set_flag(new_fl, res & 1, CF)
        # OF is undefined if the count != 1, but it is set anyway
        cf = 0x80 if (new_fl & CF) else 0
        new_fl = set_flag(new_fl, (res & 0x80) ^ cf, OF)

    return (res, new_fl)

def my_rol1(a, initial_flags):
    return my_rol(a, 1, initial_flags)

def my_ror(a, cnt, initial_flags):
    cnt = cnt & 0x1f

    new_fl = initial_flags
    res = a
    for _ in range(0, cnt % 8):
        temp_cf = 0x80 if (res & 1) != 0 else 0
        res = ((res >> 1) + temp_cf) & 0xff

    if cnt > 0:
        new_fl = set_flag(new_fl, res & 0x80, CF)
        # OF is undefined if != 1, but it is set anyway
        msb_0 = 1 if res & 0x80 else 0
        msb_1 = 1 if res & 0x40 else 0
        new_fl = set_flag(new_fl, msb_0 ^ msb_1, OF)

    return (res, new_fl)

def my_ror1(a, initial_flags):
    return my_ror(a, 1, initial_flags)

def my_rcl(a, cnt, initial_flags):
    cnt = cnt & 0x1f

    cf = 1 if (initial_flags & CF) else 0
    new_fl = initial_flags
    res = a
    for _ in range(0, cnt):
        temp_cf = 1 if (res & 0x80) != 0 else 0
        res = ((res << 1) + cf) & 0xff
        cf = temp_cf

    if cnt > 0:
        # OF is undefined if != 1, but it is set anyway
        new_fl = set_flag(new_fl, (a & 0x80) ^ (res & 0x80), OF)

    new_fl = set_flag(new_fl, cf, CF)
    return (res, new_fl)

def my_rcl1(a, initial_flags):
    return my_rcl(a, 1, initial_flags)

def my_rcr(a, cnt, initial_flags):
    cnt = cnt & 0x1f

    new_fl = initial_flags

    res = a
    cf = 1 if (initial_flags & CF) else 0
    for _ in range(cnt % 9):
        temp_cf = 1 if (res & 1) != 0 else 0
        res = (res >> 1) + cf * 0x80
        cf = temp_cf

    new_fl = set_flag(new_fl, cf, CF)
    if cnt > 0:
        new_fl = set_flag(new_fl, (a & 0x80) ^ (res & 0x80), OF)
    return (res, new_fl)

def my_rcr1(a, initial_flags):
    return my_rcr(a, 1, initial_flags)

def my_or(a, b, fl):
    res = a | b
    return (res, set_flags_pzs(res, fl))

def my_and(a, b, fl):
    res = a & b
    return (res, set_flags_pzs(res, fl))

def my_xor(a, b, fl):
    res = a ^ b
    return (res, set_flags_pzs(res, fl))

def my_inc(a, fl):
    res = (a + 1) & 0xff
    return (res, set_flags_add(a, 1, 0, res, fl))

def my_dec(a, fl):
    res = (a - 1) & 0xff
    return (res, set_flags_sub(a, 1, 0, res, fl))

def my_neg(a, fl):
    return my_sub(0, a, fl)

def my_daa(a, fl):
    res = a
    old_cf = 1 if fl & CF else 0
    new_fl = fl & ~CF
    if (a & 0xf) > 9 or (fl & AF):
        res = (res + 6) & 0xff
        new_fl = new_fl | AF
    else:
        new_fl = new_fl & ~AF

    if a > 0x99 or old_cf:
        res = (res + 0x60) & 0xff
        new_fl = new_fl | CF
    else:
        new_fl = new_fl & ~CF

    new_fl = set_flags_pzs(res, new_fl)
    return (res, new_fl)

def my_das(a, fl):
    res = a
    old_cf = 1 if fl & CF else 0
    new_fl = fl & ~CF
    if (a & 0xf) > 9 or (fl & AF):
        if old_cf or (res & 0xff) < 6:
            new_fl = fl | CF
        res = (res - 6) & 0xff
        new_fl = new_fl | AF
    else:
        new_fl = new_fl & ~AF

    if a > 0x99 or old_cf:
        res = (res - 0x60) & 0xff
        new_fl = new_fl | CF

    new_fl = set_flags_pzs(res, new_fl)
    return (res, new_fl)

def my_aaa(a, fl):
    new_fl = fl
    if (a & 0xf) > 9 or (fl & AF):
        a = (a + 0x106) & 0xffff
        new_fl = new_fl | AF
        new_fl = new_fl | CF
    else:
        new_fl = new_fl & ~AF
        new_fl = new_fl & ~CF
    a = (a & 0xff0f)
    return (a, new_fl)

def my_aas(a, fl):
    new_fl = fl
    if (a & 0xf) > 9 or (fl & AF):
        a = (a - 0x6) & 0xffff
        ah = (((a & 0xff00) >> 8) - 1) & 0xff
        a = (a & 0xff) | (ah << 8)

        new_fl = new_fl | AF
        new_fl = new_fl | CF
    else:
        new_fl = new_fl & ~AF
        new_fl = new_fl & ~CF
    a = (a & 0xff0f)
    return (a, new_fl)

def verify_op8x8(tests, op_text, op_fn, initial_flags):
    assert len(tests) == 256 * 256
    print('Testing %s (8x8 bit input, initial flags: %s)' % (op_text, decode_flags(initial_flags)))
    for a in range(0, 256):
        for b in range(0, 256):
            (t_res, t_fl) = tests[256 * a + b]
            (res, fl) = op_fn(a, b, initial_flags)

            ok = True
            if t_res != res:
                print('*** RESULT MISMATCH: got %x expected %x' % (res, t_res))
                ok = False
            if t_fl != fl:
                print('*** FLAGS MISMATCH: initial flags %s (%x)' % (decode_flags(initial_flags), initial_flags))
                #my_of = 1 if fl & OF else 0
                #exp_of = 1 if t_fl & OF else 0
                #print("  my_of %d exp_of %d" % (my_of, exp_of))
                #print('FL: %s %s --> %s %s' % (format_bin(a), b, format_bin(t_res), 'OF' if exp_of else '--'))
                ok = False

            if not ok:
                print('>: %02x %s %02x = %02x   flags %04x %s' % (a, op_text, b, res, fl, decode_flags(fl)))
                print('e: %02x %s %02x = %02x   flags %04x %s' % (a, op_text, b, t_res, t_fl, decode_flags(t_fl)))

                print('          %s' % (format_bin(a)))
                print(' %8s %s' % (op_text, format_bin(b)))
                print('====================')
                print('          %s' % (format_bin(t_res)))
                print()
                return False
    return True

def verify_op8(tests, op_text, op_fn, initial_flags):
    assert len(tests) == 256
    print('Testing %s (8 bit input, initial flags: %s)' % (op_text, decode_flags(initial_flags)))
    for a in range(0, 256):
        (t_res, t_fl) = tests[a]
        (res, fl) = op_fn(a, initial_flags)

        ok = True
        if t_res != res:
            print('*** RESULT MISMATCH: got %x expected %x' % (res, t_res))
            ok = False
        if t_fl != fl:
            print('*** FLAGS MISMATCH: got %s (%x) expected %s (%x)' % (decode_flags(fl), fl, decode_flags(t_fl), t_fl))
            ok = False

        if not ok:
            print('t: %02x %s = %02x   flags %04x %s' % (a, op_text, t_res, t_fl, decode_flags(t_fl)))
            print('>: %02x %s = %02x   flags %04x %s' % (a, op_text, res, fl, decode_flags(fl)))

            print(' %8s %s' % (op_text, format_bin(a)))
            print('====================')
            print('          %s' % (format_bin(t_res)))
            print()
            return False
    return True

def verify_op16(tests, op_text, op_fn, initial_flags):
    assert len(tests) == 65536
    print('Testing %s (16 bit input, initial flags: %s)' % (op_text, decode_flags(initial_flags)))
    for a in range(0, 65536):
        (t_res, t_fl) = tests[a]
        (res, fl) = op_fn(a, initial_flags)

        ok = True
        if t_res != res:
            print('*** RESULT MISMATCH: got %x expected %x' % (res, t_res))
            ok = False
        if t_fl != fl:
            print('*** FLAGS MISMATCH: got %s (%x) expected %s (%x)' % (decode_flags(fl), fl, decode_flags(t_fl), t_fl))
            ok = False

        if not ok:
            print('t: %04x %s = %04x   flags %04x %s' % (a, op_text, t_res, t_fl, decode_flags(t_fl)))
            print('>: %04x %s = %04x   flags %04x %s' % (a, op_text, res, fl, decode_flags(fl)))

            print(' %8s %s' % (op_text, format_bin(a)))
            print('====================')
            print('          %s' % (format_bin(t_res)))
            print()
            return False
    return True

add_tests = load_tests_8x8("add8.bin")
if not verify_op8x8(add_tests, "+", my_add, ON):
    quit()

sub_tests = load_tests_8x8("sub8.bin")
if not verify_op8x8(sub_tests, "-", my_sub, ON):
    quit()

adc_tests = load_tests_8x8("adc8.bin")
if not verify_op8x8(adc_tests[:65536], "adc", my_adc, ON):
    quit()
if not verify_op8x8(adc_tests[65536:], "adc", my_adc, ON | CF):
    quit()

sbb_tests = load_tests_8x8("sbb8.bin")
if not verify_op8x8(sbb_tests[:65536], "sbb", my_sbb, ON):
    quit()
if not verify_op8x8(sbb_tests[65536:], "sbb", my_sbb, ON | CF):
    quit()

shl_tests = load_tests_8("shl8_1.bin")
if not verify_op8(shl_tests, "shl1", my_shl1, ON):
    quit()

shl_tests = load_tests_8("shl8_8.bin")
if not verify_op8x8(shl_tests, "shl", my_shl, ON):
    quit()

shr_tests = load_tests_8("shr8_1.bin")
if not verify_op8(shr_tests, "shr1", my_shr1, ON):
    quit()

shr_tests = load_tests_8x8("shr8_8.bin")
if not verify_op8x8(shr_tests, "shr", my_shr, ON):
    quit()

sar_tests = load_tests_8("sar8_1.bin")
if not verify_op8(sar_tests, "sar1", my_sar1, ON):
    quit()

sar_tests = load_tests_8x8("sar8_8.bin")
if not verify_op8x8(sar_tests, "sar", my_sar, ON):
    quit()

rol_tests = load_tests_8("rol8_1.bin")
if not verify_op8(rol_tests, "rol1", my_rol1, ON):
    quit()

rol_tests = load_tests_8x8("rol8_8.bin")
if not verify_op8x8(rol_tests, "rol", my_rol, ON):
    quit()

ror_tests = load_tests_8("ror8_1.bin")
if not verify_op8(ror_tests, "ror1", my_ror1, ON):
    quit()

ror_tests = load_tests_8x8("ror8_8.bin")
if not verify_op8x8(ror_tests, "ror", my_ror, ON):
    quit()

rcl_tests = load_tests_8("rcl8_1.bin")
if not verify_op8(rcl_tests[:256], "rcl1", my_rcl1, ON):
    quit()
if not verify_op8(rcl_tests[256:], "rcl1", my_rcl1, ON | CF):
    quit()
rcl_tests = load_tests_8x8("rcl8_8.bin")
if not verify_op8x8(rcl_tests[:65536], "rcl", my_rcl, ON):
    quit()
if not verify_op8x8(rcl_tests[65536:], "rcl", my_rcl, ON | CF):
    quit()

rcr_tests = load_tests_8("rcr8_1.bin")
if not verify_op8(rcr_tests[:256], "rcr1", my_rcr1, ON):
    quit()
if not verify_op8(rcr_tests[256:], "rcr1", my_rcr1, ON | CF):
    quit()

rcr_tests = load_tests_8x8("rcr8_8.bin")
if not verify_op8x8(rcr_tests[:65536], "rcr", my_rcr, ON):
    quit()
if not verify_op8x8(rcr_tests[65536:], "rcr", my_rcr, ON | CF):
    quit()

or_tests = load_tests_8x8("or8.bin")
if not verify_op8x8(or_tests, "or", my_or, ON):
    quit()

and_tests = load_tests_8x8("and8.bin")
if not verify_op8x8(and_tests, "and", my_and, ON):
    quit()
xor_tests = load_tests_8x8("xor8.bin")
if not verify_op8x8(xor_tests, "xor", my_xor, ON):
    quit()

inc_tests = load_tests_8("inc8.bin")
if not verify_op8(inc_tests[:256], "inc", my_inc, ON):
    quit()
if not verify_op8(inc_tests[256:], "inc", my_inc, ON | CF):
    quit()

dec_tests = load_tests_8("dec8.bin")
if not verify_op8(dec_tests[:256], "dec", my_dec, ON):
    quit()
if not verify_op8(dec_tests[256:], "dec", my_dec, ON | CF):
    quit()

neg_tests = load_tests_8("neg8.bin")
if not verify_op8(neg_tests, "neg", my_neg, ON):
    quit()

daa_tests = load_tests_8("daa.bin")
if not verify_op8(daa_tests[:256], "daa", my_daa, ON):
    quit()
if not verify_op8(daa_tests[256:512], "daa", my_daa, ON | CF):
    quit()
if not verify_op8(daa_tests[512:768], "daa", my_daa, ON | AF):
    quit()
if not verify_op8(daa_tests[768:], "daa", my_daa, ON | CF | AF):
    quit()

das_tests = load_tests_8("das.bin")
if not verify_op8(das_tests[:256], "das", my_das, ON):
    quit()
if not verify_op8(das_tests[256:512], "das", my_das, ON | CF):
    quit()
if not verify_op8(das_tests[512:768], "das", my_das, ON | AF):
    quit()
if not verify_op8(das_tests[768:], "das", my_das, ON | CF | AF):
    quit()

aaa_tests = load_tests_16("aaa.bin")
if not verify_op16(aaa_tests[:65536], "aaa", my_aaa, ON):
    quit()
if not verify_op16(aaa_tests[65536:], "aaa", my_aaa, ON | AF):
    quit()

aas_tests = load_tests_16("aas.bin")
if not verify_op16(aas_tests[:65536], "aas", my_aas, ON):
    quit()
if not verify_op16(aas_tests[65536:], "aas", my_aas, ON | AF):
    quit()

# TODO: need to think about how to test these...
# div
# mul
# idiv
# imul

print()
print("Everything OK")
