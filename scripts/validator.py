# Verify replay
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


def verify_replay(fn):
    ret = True
    rep = parse_replay(fn)
    keys = []
    prev_frame = -1
    for ev in rep['ev']:
        if ev[0] < prev_frame:
            print(f'Event {ev} is below the event with frame {prev_frame}')
        prev_frame = ev[0]
        if ev[1] == 1:
            if ev[2] in keys:
                print(f'Double press at event {ev}')
                ret = False
            else:
                keys.append(ev[2])
        elif ev[1] == 2:
            if ev[2] in keys:
                keys.remove(ev[2])
            else:
                print(f'Double release at event {ev}')
                ret = False
        elif ev[1] == 6:
            if ev[3] < 0 or ev[3] > ev[2]:
                print(f'Impossible RNG at event {ev}; expected range: [0;{ev[2] - 1}]')
                ret = False
    if rep['ev'] and rep['ev'][-1][0] >= rep['total']:
        print(
            f'Last event is {rep["ev"][-1]}, but total ammount of frames is {rep["total"]}'
        )
        ret = False
    return ret


if __name__ == '__main__':
    verify_replay(sys.argv[1])
