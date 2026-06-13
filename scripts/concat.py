# Concatenate 2 replays
import sys


def parse_replay(fn, limit_frames=-1):
    f = open(fn, encoding='utf-8')
    total = 0
    rerecords = 0
    ev = []
    for line in f:
        s = line.split(',,')[0].strip().rstrip(',').split(',')
        if len(s) < 2:
            continue
        frame = int(s[0])
        if frame < 0:
            if s[1] == 'total':
                total = int(s[2])
            elif s[1] == 'rerecords':
                rerecords = int(s[2])
        else:
            if limit_frames > 0 and frame >= limit_frames:
                break
            ev.append(list(map(int, s)))
    return {'total': total, 'rerecords': rerecords, 'ev': ev}


def export_replay(fn, replay):
    f = open(fn, 'w', encoding='utf-8')
    f.write('-4,brep,2\n')
    f.write(f'-3,total,{replay["total"]}\n')
    f.write(f'-2,rerecords,{replay["rerecords"]}\n')
    f.write('-1,data,\n')
    for ev in replay['ev']:
        f.write(','.join(map(str, ev)) + '\n')


def calc_keys(replay, frame):
    ret = []
    for i in replay['ev']:
        if i[0] >= frame:
            break
        if i[1] == 1:
            ret.append(i[2])
        elif i[1] == 2:
            ret.remove(i[2])
    return ret


def concat(fn1, fn2, frame1, frame2, out_fn):
    r1 = parse_replay(fn1, frame1)
    r2 = parse_replay(fn2)
    r1['ev'].append((frame1, 7, 0))
    k1 = calc_keys(r1, frame1)
    k2 = calc_keys(r2, frame2)
    for i in k1:
        if i not in k2:
            print(f'key {i} -> up')
            r1['ev'].append((frame1, 2, i))
    for i in k2:
        if i not in k1:
            print(f'key {i} -> down')
            r1['ev'].append((frame1, 1, i))
    r1['total'] = frame1 + r2['total'] - frame2
    r1['rerecords'] += r2['rerecords']
    for ev in r2['ev']:
        ev[0] = ev[0] - frame2 + frame1
        if ev[0] < frame1:
            continue
        r1['ev'].append(ev)
    export_replay(out_fn, r1)


if __name__ == '__main__':
    concat(sys.argv[1], sys.argv[3], int(sys.argv[2]), int(sys.argv[4]), sys.argv[5])
