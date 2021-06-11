
import glob

batcap = 1738000

#Discharge-1p0C_Cont-5dC.csv


def get_temp_from_name(name):
    tmp = name.split('-')
    if tmp[0] != "Discharge":
        return 0xffffffff
    temp_l = tmp[2].split("dC")
    temp = int(temp_l[0])
    return temp

def header_name(name):
    tmp = name.replace(".csv", ".h")
    return tmp

def prepare_hdr(h, name):
    h.write("#ifndef %s\n" % name.upper().replace("-", "_"))
    h.write("#define %s\n\n" % name.upper().replace("-", "_"))

def finalize_hdr(h):
    h.write("#endif\n")
    h.close()

def start_arr(h, s):
    h.write("static int test_%s[VALUES] = {\n" % s)

def end_arr(h):
    h.write("};\n")

def as_to_uah(asec):
    return 10000*asec / 6 / 6

def uah_to_as(ah):
    return ah * 6 * 6 / 10000

#Perf would be better is all values were parsed at one loop.
def fill_uah(h, s):
    ptime = 0
    As = 0
    for line in s.readlines():
        #print(line)
        if line.startswith("Time"):
            continue
        tokens = line.split(",")
        current = float(tokens[2].replace("\n", ""))
        time = int(tokens[0])
        As = As + current * (time - ptime)
        ptime = time
        capAs = uah_to_as(batcap) + As
        h.write("   %d,\n" % as_to_uah(capAs))

def fill_vsys(h, s):
    ptime = 0
    As = 0
    for line in s.readlines():
        #print(line)
        if line.startswith("Time"):
            continue
        tokens = line.split(",")
        vsys = float(tokens[1])
        #Add 0.5 to avoid flooring
        h.write("   %d,\n" % int(vsys * 1000000 + 0.5))

#NOTE: Works when Rsens is 10MOhm. With that Rsens, the 16 high bits of CCNTD
#directly value represent capacity in units of 10As
def fill_ccntd(h, s):
    ptime = 0
    As = 0
    for line in s.readlines():
        if line.startswith("Time"):
            continue
        tokens = line.split(",")
        current = float(tokens[2].replace("\n", ""))
        time = int(tokens[0])
        # CCNTD format - increase accuracy by using also the sub As portion
        # of CCNTD (two registers)
        As = int((65536 / 10 * current * (time - ptime) + 0.5))
        ptime = time
        h.write("   %d,\n" % As)

def fill_current(h, s):
    for line in s.readlines():
        if line.startswith("Time"):
            continue
        tokens = line.split(",")
        current = float(tokens[2].replace("\n", ""))
        current2 = int(current * 1000000)
        h.write("   %d,\n" % current2)

def fill_time(h, s):
    ptime = 0
    for line in s.readlines():
        if line.startswith("Time"):
            continue
        tokens = line.split(",")
        time = int(tokens[0])
        time2 = time - ptime
        ptime = time
        h.write("   %d,\n" % time2)

def start_header(name):
    hname = header_name(name)
    h = open("out/%s" % hname, "w")
    prepare_hdr(h, hname.replace(".", "_"))
    return h

def add_uah(h, s):
    print("Add uah array\n")
    start_arr(h, "uah")
    fill_uah(h, s)
    end_arr(h)
    s.seek(0, 0)

def add_vsys(h, s):
    print("Add vsys array\n")
    start_arr(h, "vsys")
    fill_vsys(h, s)
    end_arr(h)
    s.seek(0, 0)

def add_ccntd(h, s):
    print("Add ccntd array\n")
    start_arr(h, "ccntd")
    fill_ccntd(h, s)
    end_arr(h)
    s.seek(0, 0)

def add_current(h, s):
    print("Add current array\n")
    start_arr(h, "current")
    fill_current(h, s)
    end_arr(h)
    s.seek(0, 0)

def add_times(h, s):
    print("Add time array\n")
    start_arr(h, "time")
    fill_time(h, s)
    end_arr(h)
    s.seek(0, 0)

def get_values(s):
    values = 0
    for line in s.readlines():
        if line.startswith("Time"):
            continue
        if line.count(",") != 2:
            continue
        values += 1
    s.seek(0, 0)
    return values

def main():

    var = glob.glob("Discharge-*-*.csv")

    for f in var:
        print("Handling file %s\n" % f)
        temperature = get_temp_from_name(f)
        if (temperature == 0xffffffff):
            continue
        source = open(f, "r");
        values = get_values(source)
#        print(temperature)
        hdr = start_header(f)
        hdr.write("#define TEST_TEMP %d\n" % temperature * 10)
        hdr.write("#define VALUES %d\n\n" % values)
        add_uah(hdr, source)
        hdr.write("\n")
        add_vsys(hdr, source)
        hdr.write("\n")
        hdr.write("/*\n * CCNTD change in CCNTD register format for Rsens 10MOhm\n * - (10 As corrsponds 16 high bits)\n */\n")
        hdr.write("/* NOTE: Convert to correct the byte-order. */\n")
        add_ccntd(hdr, source)
        hdr.write("\n")
        hdr.write("/* Current in uA */\n")
        add_current(hdr, source)
        hdr.write("\n")
        hdr.write("/* Time lapses */\n")
        add_times(hdr, source)
        finalize_hdr(hdr)

if __name__ == "__main__":
    main()


