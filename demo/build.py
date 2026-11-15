import subprocess
# Narration is 28.032 s. Clips sum to 29.5 s; the last 1.5 s holds the
# ledger frame under silence, so the video lands at ~30 s.
shots = [
    ("01-table.png", 11.2, "01 \u00b7 The comparison \u2014 nine sites, one window"),
    ("02-sky.png", 11.2, "02 \u00b7 The sky over each site \u2014 Sun path gold, Earth blue"),
    ("03-ledger.png", 7.1, "03 \u00b7 The decision \u2014 Mons Mouton Plateau"),
]
for i, (img, dur, label) in enumerate(shots, 1):
    vf = (
        f"scale=1920:1080,"
        f"fade=t=in:st=0:d=0.4,fade=t=out:st={dur-0.4:.2f}:d=0.4,"
        f"drawbox=x=0:y=1000:w=1920:h=80:color=black@0.6:t=fill,"
        f"drawtext=text='{label}':fontfile=/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf:"
        f"fontsize=38:fontcolor=white:x=36:y=1016"
    )
    subprocess.run(["ffmpeg", "-y", "-v", "error", "-loop", "1", "-i", f"shots/{img}",
                    "-vf", vf, "-t", str(dur), "-c:v", "libx264", "-pix_fmt", "yuv420p",
                    f"clips/c{i}.mp4"], check=True)
with open("concat.txt", "w") as f:
    f.write("\n".join(f"file 'clips/c{i}.mp4'" for i in range(1, 4)))
subprocess.run(["ffmpeg", "-y", "-v", "error", "-f", "concat", "-safe", "0",
                "-i", "concat.txt", "-i", "narration.wav",
                "-af", "apad", "-t", "29.5",
                "-c:v", "copy", "-c:a", "aac",
                "lunar-navigator-demo-30s.mp4"], check=True)
print("built lunar-navigator-demo-30s.mp4")
