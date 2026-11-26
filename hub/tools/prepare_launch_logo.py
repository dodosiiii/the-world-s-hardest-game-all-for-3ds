"""Prepare small alpha-mask PNGs for the static Nintendo 3DS title logo."""
from pathlib import Path
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
META=ROOT/"meta"

def make_mask(source,destination,size,cutoff=58,gain=2.15):
    image=Image.open(source).convert("RGB").resize(size,Image.Resampling.LANCZOS)
    gray=image.convert("L")
    # The 3DS logo texture tool stores the PNG alpha mask (A4), not RGB colour.
    # Convert the artwork luminance to alpha so its shaded cubes retain their form.
    mask=gray.point(lambda lum: max(0,min(255,int((lum-cutoff)*gain))))
    rgba=Image.new("RGBA",size,(255,255,255,0))
    rgba.putalpha(mask)
    rgba.save(destination,dpi=(96,96),optimize=True)

make_mask(META/"launch-logo-top.png",META/"launch-logo-top-mask.png",(256,152))
make_mask(META/"launch-logo-bottom.png",META/"launch-logo-bottom-mask.png",(208,160))
print("Prepared final 256x152 top and 208x160 bottom alpha-mask artwork.")
