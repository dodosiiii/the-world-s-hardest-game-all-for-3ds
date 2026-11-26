"""Convert the HOME banner tune to broadly compatible stereo PCM WAV."""
from pathlib import Path
import struct
import wave

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "meta" / "banner.wav"
OUTPUT = ROOT / "meta" / "banner-home.wav"

with wave.open(str(SOURCE), "rb") as wav:
    channels, width, rate, frames, compression, _ = wav.getparams()
    if (channels, width, rate, compression) != (1, 2, 22050, "NONE"):
        raise SystemExit("Expected mono 22050 Hz, 16-bit PCM in meta/banner.wav")
    samples = struct.unpack("<" + "h" * frames, wav.readframes(frames))

# Linear interpolation doubles the sample rate; copy each sample to L and R.
stereo = []
for index in range(frames * 2):
    source_index, half = divmod(index, 2)
    if half == 0 or source_index + 1 == frames:
        sample = samples[source_index]
    else:
        sample = (samples[source_index] + samples[source_index + 1]) // 2
    stereo.extend((sample, sample))

with wave.open(str(OUTPUT), "wb") as wav:
    wav.setnchannels(2)
    wav.setsampwidth(2)
    wav.setframerate(44100)
    wav.writeframes(struct.pack("<" + "h" * len(stereo), *stereo))
print(f"Created {OUTPUT} ({frames / rate:.3f} seconds, stereo 44100 Hz PCM)")
