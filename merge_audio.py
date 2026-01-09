import os
import subprocess

data = []
for i in os.listdir():
    if (i.startswith('cap_') or i.startswith('audio_')) and i.endswith('.wav'):
        offset = int(i.split('_')[1].replace('.wav', ''))
        data.append((i, offset))

if not data:
    print("No audio files found.")
    exit()

inputs = []
filter_parts = []

for idx, (filename, offset_ms) in enumerate(data):
    inputs.extend(['-i', filename])
    filter_parts.append(f"[{idx}:a]adelay={offset_ms}:all=1[a{idx}]")

mix_inputs = "".join(f"[a{i}]" for i in range(len(data)))
filter_complex = (
    f"{';'.join(filter_parts)};"
    f"{mix_inputs}amix=inputs={len(data)}[mixed];"
    f"[mixed]aformat=sample_rates=48000:channel_layouts=stereo[out]"
)

command = [
    'ffmpeg', '-y',
    *inputs,
    '-filter_complex', filter_complex,
    '-map', '[out]',
    '-ac', '2',       # Force 2 channels (stereo)
    '-ar', '48000',   # Force 48kHz sample rate
    'output.wav'
]

try:
    subprocess.run(command, check=True)
    print("Successfully created output.wav (2 channels, 48kHz)")
except subprocess.CalledProcessError as e:
    print(f"Error calling FFmpeg: {e}")