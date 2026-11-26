"""Create the two static screens used as artwork for a custom title launch logo."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, ImageFilter

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/"meta"

def font(path,size):
    try: return ImageFont.truetype(path,size)
    except OSError: return ImageFont.load_default()

bold=font(r"C:\Windows\Fonts\arialbd.ttf",26)
small=font(r"C:\Windows\Fonts\arial.ttf",12)
tiny=font(r"C:\Windows\Fonts\arialbd.ttf",10)

def background(size):
    w,h=size
    im=Image.new("RGB",size)
    p=im.load()
    for y in range(h):
        t=y/max(1,h-1)
        for x in range(w):
            glow=max(0,1-(((x-w*.54)/(w*.8))**2+((y-h*.42)/(h*.9))**2)**.5)
            p[x,y]=(int(7+8*t+5*glow),int(13+9*t+15*glow),int(25+17*t+35*glow))
    return im

def cube(draw,x,y,s,front,side,top):
    d=s*.28; h=s*.21
    draw.polygon([(x-s/2,y-s/2),(x-s/2+d,y-s/2-h),(x+s/2+d,y-s/2-h),(x+s/2,y-s/2)],fill=top)
    draw.polygon([(x+s/2,y-s/2),(x+s/2+d,y-s/2-h),(x+s/2+d,y+s/2-h),(x+s/2,y+s/2)],fill=side)
    draw.polygon([(x-s/2,y-s/2),(x+s/2,y-s/2),(x+s/2,y+s/2),(x-s/2,y+s/2)],fill=front)
    draw.line([(x-s/2,y-s/2),(x+s/2,y-s/2),(x+s/2,y+s/2),(x-s/2,y+s/2),(x-s/2,y-s/2)],fill=(255,239,241),width=1)

top=background((400,240)).convert("RGBA")
glow=Image.new("RGBA",top.size,(0,0,0,0)); gd=ImageDraw.Draw(glow)
gd.ellipse((70,42,360,227),fill=(42,110,230,56)); glow=glow.filter(ImageFilter.GaussianBlur(34)); top=Image.alpha_composite(top,glow).convert("RGB")
d=ImageDraw.Draw(top)
for x in range(10,400,20): d.line((x,0,x,240),fill=(17,25,41),width=1)
for y in range(12,240,20): d.line((0,y,400,y),fill=(17,25,41),width=1)
d.rounded_rectangle((18,15,382,225),radius=17,outline=(55,83,127),width=1)
d.text((200,43),"THE WORLD’S",font=small,fill=(255,116,128),anchor="mm")
d.text((200,74),"HARDEST GAMES",font=bold,fill=(244,247,255),anchor="mm",stroke_width=1,stroke_fill=(27,43,74))
d.line((100,94,300,94),fill=(237,73,93,190),width=2)
cube(d,104,153,37,(238,65,87),(166,40,62),(255,121,135))
cube(d,292,150,39,(49,160,212),(28,106,164),(104,216,245))
cube(d,198,168,53,(250,80,98),(179,39,58),(255,152,150))
d.ellipse((184,112,212,140),fill=(255,190,54),outline=(255,236,170),width=3)
d.ellipse((190,118,206,134),outline=(255,240,188),width=1)
d.text((200,209),"A CUBE-RUNNER CHALLENGE",font=tiny,fill=(170,193,228),anchor="mm")
logo_top=top.convert("RGB")
logo_top.save(OUT/"launch-logo-top.png")

bottom=background((320,240)).convert("RGBA")
blur=Image.new("RGBA",bottom.size,(0,0,0,0)); bd=ImageDraw.Draw(blur)
bd.ellipse((18,32,302,201),fill=(45,105,223,55)); blur=blur.filter(ImageFilter.GaussianBlur(30)); bottom=Image.alpha_composite(bottom,blur).convert("RGB")
d=ImageDraw.Draw(bottom)
d.rounded_rectangle((14,14,306,226),radius=18,outline=(55,83,127),width=1)
d.text((160,50),"WHG HUB",font=font(r"C:\Windows\Fonts\arialbd.ttf",24),fill=(244,247,255),anchor="mm")
d.text((160,72),"VERSION 1.2",font=tiny,fill=(255,116,128),anchor="mm")
d.line((74,89,246,89),fill=(237,73,93,170),width=2)
d.polygon([(44,148),(272,148),(257,184),(61,184)],fill=(17,31,52),outline=(76,111,161))
for i in range(5):
    x=67+i*42
    d.line((x,151,x-5,181),fill=(55,84,127,145),width=1)
cube(d,91,143,22,(238,65,87),(166,40,62),(255,121,135))
cube(d,169,147,22,(48,154,208),(29,98,150),(95,207,239))
d.ellipse((225,127,255,157),fill=(255,187,51),outline=(255,235,170),width=3)
d.ellipse((232,134,248,150),outline=(255,244,199),width=1)
d.text((160,202),"RUN  •  DODGE  •  REPEAT",font=tiny,fill=(185,204,232),anchor="mm")
logo_bottom=bottom.convert("RGB")
logo_bottom.save(OUT/"launch-logo-bottom.png")

print("Created final top- and bottom-screen launch-logo artwork in",OUT)
