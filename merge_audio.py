import os
import math
import subprocess
import tempfile

data = []
for i in os.listdir():
    if (i.startswith('cap_') or i.startswith('audio_')) and i.endswith('.wav'):
        try:
            offset = int(i.split('_')[1])
            volume = int(i.split('_')[2])
            data.append((i, offset, volume))
        except (IndexError, ValueError):
            continue

if not data:
    print("No audio files found.")
    exit()

data.sort(key=lambda x: x[1])

inputs = []
filter_parts = []

for idx, (filename, offset_ms, volume_mb) in enumerate(data):
    inputs.extend(['-i', filename])
    if volume_mb == 0:
        filter_parts.append(f"[{idx}:a]adelay={offset_ms}:all=1[a{idx}]")
    else:
        volume_linear = math.pow(10, volume_mb / 2000.0)
        filter_parts.append(f"[{idx}:a]adelay={offset_ms}:all=1,volume={volume_linear:.8f}[a{idx}]")

mix_inputs = "".join(f"[a{i}]" for i in range(len(data)))

filter_complex_string = (
    f"{';'.join(filter_parts)};"
    f"{mix_inputs}amix=inputs={len(data)}:normalize=0:dropout_transition=0[mixed];"
    f"[mixed]dynaudnorm,aformat=sample_rates=48000:channel_layouts=stereo[out]"
)

with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as tf:
    tf.write(filter_complex_string)
    filter_script_path = tf.name

command = [
    'ffmpeg', '-y',
    *inputs,
    '-filter_complex_script', filter_script_path,
    '-map', '[out]',
    '-ac', '2',
    '-ar', '48000',
    'output.wav'
]

try:
    subprocess.run(command, check=True)
    print(f"Successfully joined {len(data)} files into output.wav.")
finally:
    if os.path.exists(filter_script_path):
        os.remove(filter_script_path)
