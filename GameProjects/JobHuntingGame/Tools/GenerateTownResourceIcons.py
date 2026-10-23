"""Draw original, antialiased resource icons as transparent 64px PNG assets."""
from pathlib import Path
import struct,zlib,math

SIZE=64;AA=4;N=SIZE*AA
OUT=Path(__file__).resolve().parents[1]/'Assets/Application/UI/Resources'
class Icon:
    def __init__(self):self.pixels=[(0,0,0,0)]*(N*N)
    def paint(self,x,y,color):
        if 0<=x<N and 0<=y<N:self.pixels[y*N+x]=color
    def polygon(self,points,color):
        points=[(x*AA,y*AA) for x,y in points]
        for y in range(max(0,int(min(p[1] for p in points))),min(N,int(max(p[1] for p in points))+1)):
            yy=y+.5;cross=[]
            for a,b in zip(points,points[1:]+points[:1]):
                if (a[1]<=yy<b[1]) or (b[1]<=yy<a[1]):cross.append(a[0]+(yy-a[1])*(b[0]-a[0])/(b[1]-a[1]))
            cross.sort()
            for a,b in zip(cross[::2],cross[1::2]):
                for x in range(max(0,math.ceil(a-.5)),min(N,math.ceil(b-.5))):self.paint(x,y,color)
    def circle(self,x,y,r,color):
        for yy in range(max(0,int((y-r)*AA)),min(N,int((y+r)*AA)+1)):
            for xx in range(max(0,int((x-r)*AA)),min(N,int((x+r)*AA)+1)):
                if ((xx+.5)/AA-x)**2+((yy+.5)/AA-y)**2<=r*r:self.paint(xx,yy,color)
    def save(self,name):
        data=bytearray()
        for y in range(SIZE):
            data.append(0)
            for x in range(SIZE):
                samples=[self.pixels[(y*AA+dy)*N+x*AA+dx] for dy in range(AA) for dx in range(AA)];alpha=sum(c[3] for c in samples)
                data.extend([round(sum(c[k]*c[3] for c in samples)/alpha) if alpha else 0 for k in range(3)]+[round(alpha/(AA*AA))])
        def chunk(kind,payload):return struct.pack('>I',len(payload))+kind+payload+struct.pack('>I',zlib.crc32(kind+payload)&0xffffffff)
        png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',SIZE,SIZE,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(data,9))+chunk(b'IEND',b'')
        OUT.mkdir(parents=True,exist_ok=True);(OUT/(name+'.png')).write_bytes(png)

def logs():
    m=Icon()
    for x,y in [(17,40),(39,40),(28,21)]:
        m.polygon([(x-11,y-8),(x+4,y-14),(x+19,y-6),(x+5,y+6)],(115,65,30,255))
        m.polygon([(x+5,y+6),(x+19,y-6),(x+19,y+8),(x+5,y+18)],(92,48,23,255))
        m.circle(x,y+5,11,(69,40,24,255));m.circle(x,y+5,8,(224,168,92,255));m.circle(x,y+5,5,(157,99,47,255));m.circle(x,y+5,3,(231,180,108,255))
    m.save('Wood')
def rocks():
    m=Icon();m.polygon([(7,43),(13,22),(30,12),(47,19),(56,43),(43,55),(17,56)],(73,87,101,255))
    m.polygon([(13,22),(30,12),(35,35),(20,43),(7,43)],(174,190,201,255));m.polygon([(30,12),(47,19),(56,43),(35,35)],(124,145,163,255));m.polygon([(7,43),(20,43),(43,55),(17,56)],(100,115,130,255))
    m.polygon([(3,51),(10,42),(21,45),(25,57),(10,60)],(140,156,170,255));m.save('Stone')
def crystals():
    m=Icon()
    for x,bottom,w,h in [(14,54,11,27),(33,57,17,49),(51,53,11,33)]:
        m.polygon([(x-w/2,bottom),(x-w/2,bottom-h+10),(x,bottom-h),(x+w/2,bottom-h+10),(x+w/2,bottom)],(15,111,166,255))
        m.polygon([(x-w/2,bottom-h+10),(x,bottom-h),(x+1,bottom-4),(x-w/2,bottom)],(117,241,255,255));m.polygon([(x,bottom-h),(x+w/2,bottom-h+10),(x+1,bottom-4)],(44,179,231,255))
    m.save('Crystal')
def light():
    m=Icon()
    for r,color in [(28,(255,174,25,20)),(23,(255,189,36,45)),(18,(255,200,55,90)),(13,(255,221,95,255)),(8,(255,245,185,255))]:m.circle(32,32,r,color)
    m.polygon([(32,6),(35,27),(57,32),(35,36),(32,58),(28,36),(7,32),(28,28)],(255,229,133,255));m.circle(32,32,5,(255,255,232,255));m.save('Light')
def parameters():
    m=Icon();m.circle(32,32,25,(244,187,58,255));m.circle(32,32,21,(255,222,106,255))
    m.circle(24,27,3,(92,65,39,255));m.circle(41,27,3,(92,65,39,255))
    m.polygon([(18,37),(24,42),(32,44),(40,42),(47,36),(43,46),(34,50),(26,49),(20,44)],(123,66,36,255));m.save('Happiness')
    m=Icon();m.polygon([(32,5),(55,14),(53,34),(45,48),(32,59),(19,49),(11,35),(9,14)],(42,109,153,255))
    m.polygon([(32,10),(50,18),(48,34),(41,45),(32,53),(32,10)],(100,220,240,255))
    m.polygon([(18,30),(23,25),(30,33),(42,20),(47,25),(30,44)],(228,255,242,255));m.save('Safety')
    m=Icon();m.polygon([(9,55),(9,35),(20,35),(20,55)],(68,174,149,255));m.polygon([(26,55),(26,27),(37,27),(37,55)],(84,214,165,255));m.polygon([(43,55),(43,17),(54,17),(54,55)],(146,247,166,255))
    m.polygon([(7,25),(34,7),(47,8),(47,2),(59,7),(53,19),(51,13),(36,14),(11,32)],(255,220,102,255));m.save('Prosperity')
    m=Icon();m.polygon([(17,19),(47,19),(44,57),(20,57)],(198,65,65,255));m.polygon([(13,12),(25,12),(25,7),(39,7),(39,12),(51,12),(51,18),(13,18)],(255,143,127,255))
    for x in [25,32,39]:m.polygon([(x-1,25),(x+1,25),(x+1,49),(x-1,49)],(255,197,183,255))
    m.save('Remove')
    m=Icon();m.polygon([(32,5),(61,56),(3,56)],(245,82,65,255));m.polygon([(32,14),(53,51),(11,51)],(255,202,106,255));m.polygon([(29,25),(35,25),(34,40),(30,40)],(102,43,35,255));m.circle(32,46,2.5,(102,43,35,255));m.save('Warning')
if __name__=='__main__':
    logs();rocks();crystals();light();parameters();print('Generated resource and parameter PNG icons in',OUT)
